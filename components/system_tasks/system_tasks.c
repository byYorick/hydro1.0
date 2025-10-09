/**
 * @file system_tasks.c
 * @brief Реализация задач FreeRTOS системы гидропоники
 */

#include "system_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "esp_system.h"
#include "notification_system.h"
#include "data_logger.h"
#include "task_scheduler.h"
#include "ph_ec_controller.h"
#include "config_manager.h"
#include "system_interfaces.h"
#include "error_handler.h"
#include "trema_ec.h"

#include "lvgl_ui.h"
#include "encoder.h"

static const char *TAG = "SYS_TASKS";

#define SENSOR_FAILURE_THRESHOLD 5

static const char *SENSOR_NAMES[SENSOR_INDEX_COUNT] = {
    "pH",
    "EC",
    "Temperature",
    "Humidity",
    "Lux",
    "CO2",
};

static const char *SENSOR_UNITS[SENSOR_INDEX_COUNT] = {
    "",
    "mS/cm",
    "°C",
    "%",
    "lux",
    "ppm",
};

typedef struct {
    uint32_t total_cycles;
    uint32_t successful_cycles;
    uint32_t failed_cycles;
    uint32_t max_cycle_time_ms;
    uint32_t min_cycle_time_ms;
} sensor_task_stats_t;

/*******************************************************************************
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

static system_tasks_context_t task_context = {0};
static system_task_handles_t task_handles = {0};
static bool context_initialized = false;

static uint32_t sensor_failure_counters[SENSOR_INDEX_COUNT] = {0};
static uint32_t sensor_failure_events[SENSOR_INDEX_COUNT] = {0};
static bool sensor_failure_active[SENSOR_INDEX_COUNT] = {false};
static sensor_task_stats_t sensor_stats = {
    .min_cycle_time_ms = UINT32_MAX,
};

/*******************************************************************************
 * ПРОТОТИПЫ ФУНКЦИЙ ЗАДАЧ
 ******************************************************************************/

static void sensor_task(void *pvParameters);
static void display_task(void *pvParameters);
static void notification_task(void *pvParameters);
static void data_logger_task(void *pvParameters);
static void scheduler_task(void *pvParameters);
static void ph_ec_task(void *pvParameters);

static esp_err_t read_all_sensors(sensor_data_t *data);
static void register_sensor_failure(sensor_index_t index, const char *details);
static void register_sensor_recovery(sensor_index_t index);
static float get_sensor_fallback(sensor_index_t index);
static float get_last_sensor_value(sensor_index_t index);
static void sensor_update_success(sensor_index_t index, float value);
static void sensor_update_failure(sensor_index_t index, const char *reason, float fallback);

static void register_sensor_failure(sensor_index_t index, const char *details)
{
    if (index >= SENSOR_INDEX_COUNT) {
        return;
    }

    sensor_failure_counters[index]++;

    if (!sensor_failure_active[index] && sensor_failure_counters[index] >= SENSOR_FAILURE_THRESHOLD) {
        sensor_failure_active[index] = true;
        sensor_failure_events[index]++;

        char message[128];
        const char *suffix = (details != NULL) ? details : "используется значение по умолчанию";
        snprintf(message, sizeof(message),
                 "Датчик %s не отвечает %u циклов, %s",\
                 SENSOR_NAMES[index], (unsigned)sensor_failure_counters[index], suffix);

        notification_create(NOTIF_TYPE_ERROR, NOTIF_PRIORITY_HIGH, NOTIF_SOURCE_SENSOR, message);
        data_logger_log_alarm(LOG_LEVEL_WARNING, message);
    }
}

static void register_sensor_recovery(sensor_index_t index)
{
    if (index >= SENSOR_INDEX_COUNT) {
        return;
    }

    if (sensor_failure_active[index]) {
        char message[128];
        snprintf(message, sizeof(message),
                 "Датчик %s восстановился после %u циклов сбоев",\
                 SENSOR_NAMES[index], (unsigned)sensor_failure_counters[index]);

        notification_create(NOTIF_TYPE_INFO, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SENSOR, message);
        data_logger_log_system_event(LOG_LEVEL_INFO, message);
    }

    sensor_failure_active[index] = false;
    sensor_failure_counters[index] = 0;
}

esp_err_t system_tasks_init_context(void)
{
    if (context_initialized) {
        ESP_LOGW(TAG, "Context already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing task context...");

    task_context.sensor_data_mutex = xSemaphoreCreateMutex();
    if (task_context.sensor_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor_data_mutex");
        return ESP_ERR_NO_MEM;
    }

    task_context.sensor_data_queue = xQueueCreate(QUEUE_SIZE_SENSOR_DATA, sizeof(sensor_data_t));
    if (task_context.sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor_data_queue");
        return ESP_ERR_NO_MEM;
    }

    task_context.sensor_data_valid = false;
    memset(&task_context.last_sensor_data, 0, sizeof(sensor_data_t));
    memset(task_context.sensor_failure_streak, 0, sizeof(task_context.sensor_failure_streak));
    memset(task_context.sensor_failure_total, 0, sizeof(task_context.sensor_failure_total));
    memset(task_context.sensor_fault_active, 0, sizeof(task_context.sensor_fault_active));
    memset(&task_context.sensor_stats, 0, sizeof(task_context.sensor_stats));
    memset(&task_context.data_logger_stats, 0, sizeof(task_context.data_logger_stats));
    memset(&task_context.notification_stats, 0, sizeof(task_context.notification_stats));
    task_context.config_valid = false;

    context_initialized = true;
    ESP_LOGI(TAG, "Task context initialized successfully");
    return ESP_OK;
}

esp_err_t system_tasks_create_all(system_task_handles_t *handles)
{
    if (!context_initialized) {
        ESP_LOGE(TAG, "Context not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Creating all system tasks...");

    BaseType_t ret;

    ret = xTaskCreate(sensor_task, "sensor_task", TASK_STACK_SIZE_SENSOR, NULL,
                      TASK_PRIORITY_SENSOR, &task_handles.sensor_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] sensor_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_SENSOR, TASK_STACK_SIZE_SENSOR);

    ret = xTaskCreate(display_task, "display_task", TASK_STACK_SIZE_DISPLAY, NULL,
                      TASK_PRIORITY_DISPLAY, &task_handles.display_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] display_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_DISPLAY, TASK_STACK_SIZE_DISPLAY);

    ret = xTaskCreate(notification_task, "notification_task", TASK_STACK_SIZE_NOTIFICATION, NULL,
                      TASK_PRIORITY_NOTIFICATION, &task_handles.notification_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create notification_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] notification_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_NOTIFICATION, TASK_STACK_SIZE_NOTIFICATION);

    ret = xTaskCreate(data_logger_task, "data_logger_task", TASK_STACK_SIZE_DATALOGGER, NULL,
                      TASK_PRIORITY_DATALOGGER, &task_handles.data_logger_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create data_logger_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] data_logger_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_DATALOGGER, TASK_STACK_SIZE_DATALOGGER);

    ret = xTaskCreate(scheduler_task, "scheduler_task", TASK_STACK_SIZE_SCHEDULER, NULL,
                      TASK_PRIORITY_SCHEDULER, &task_handles.scheduler_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] scheduler_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_SCHEDULER, TASK_STACK_SIZE_SCHEDULER);

    ret = xTaskCreate(ph_ec_task, "ph_ec_task", TASK_STACK_SIZE_PH_EC, NULL,
                      TASK_PRIORITY_PH_EC, &task_handles.ph_ec_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ph_ec_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[OK] ph_ec_task created (Pri: %d, Stack: %d)",
             TASK_PRIORITY_PH_EC, TASK_STACK_SIZE_PH_EC);

    ESP_LOGI(TAG, "[OK] encoder_task will be created by lvgl_main.c");

    if (handles != NULL) {
        *handles = task_handles;
    }

    ESP_LOGI(TAG, "All tasks created successfully!");
    return ESP_OK;
}

system_tasks_context_t* system_tasks_get_context(void)
{
    return &task_context;
}

system_task_handles_t* system_tasks_get_handles(void)
{
    return &task_handles;
}

esp_err_t system_tasks_set_config(const system_config_t *config)
{
    if (!context_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    task_context.active_config = *config;
    task_context.config_valid = true;
    return ESP_OK;
}

static void sensor_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t last_cycle_tick = last_wake_time;
    uint32_t read_count = 0;

    ESP_LOGI(TAG, "Sensor task started (interval: %d ms)", TASK_INTERVAL_SENSOR);
    vTaskDelay(pdMS_TO_TICKS(3000));
    last_wake_time = xTaskGetTickCount();
    last_cycle_tick = last_wake_time;
    
    // ТЕСТОВАЯ ОШИБКА УДАЛЕНА для проверки фокуса

    while (1) {
        read_count++;
        TickType_t now_tick = xTaskGetTickCount();
        if (now_tick - last_cycle_tick > pdMS_TO_TICKS(TASK_INTERVAL_SENSOR)) {
            task_context.sensor_stats.missed_deadlines++;
        }
        last_cycle_tick = now_tick;

        uint64_t cycle_start_us = esp_timer_get_time();

        // Читаем все датчики
        esp_err_t ret = read_all_sensors(&sensor_data);

        task_context.sensor_stats.execution_count++;
        if (ret != ESP_OK) {
            task_context.sensor_stats.failure_count++;
            ERROR_WARN(ERROR_CATEGORY_SENSOR, TAG,
                      "Ошибка чтения датчиков (цикл #%lu)",
                      task_context.sensor_stats.execution_count);
        } else {
            // Отладочная информация раз в 30 циклов (~1 минута при интервале 2 сек)
            if (task_context.sensor_stats.execution_count % 30 == 0) {
                ERROR_DEBUG(TAG,
                           "Датчики OK: pH=%.2f EC=%.2f T=%.1f H=%.1f Lux=%.0f CO2=%.0f",
                           sensor_data.ph, sensor_data.ec, sensor_data.temperature,
                           sensor_data.humidity, sensor_data.lux, sensor_data.co2);
            }
        }

        uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - cycle_start_us) / 1000ULL);
        task_context.sensor_stats.last_duration_ms = duration_ms;
        if (duration_ms > task_context.sensor_stats.max_duration_ms) {
            task_context.sensor_stats.max_duration_ms = duration_ms;
        }

        if (ret == ESP_OK) {
            if (xSemaphoreTake(task_context.sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                task_context.last_sensor_data = sensor_data;
                task_context.sensor_data_valid = true;
                xSemaphoreGive(task_context.sensor_data_mutex);
            }

            // Отправляем данные в очередь, удаляя старые при переполнении
            if (xQueueSend(task_context.sensor_data_queue, &sensor_data, 0) != pdTRUE) {
                // Очередь полная - удаляем самое старое значение
                sensor_data_t oldest;
                if (xQueueReceive(task_context.sensor_data_queue, &oldest, 0) == pdTRUE) {
                    // Пытаемся добавить новое значение
                    xQueueSend(task_context.sensor_data_queue, &sensor_data, 0);
                    ESP_LOGW(TAG, "Sensor queue full, replaced oldest data");
                }
            }
            
            ph_ec_controller_update_values(sensor_data.ph, sensor_data.ec);

            data_logger_log_sensor_data(sensor_data.ph, sensor_data.ec,
                                        sensor_data.temperature, sensor_data.humidity,
                                        sensor_data.lux, sensor_data.co2);

            sensor_stats.successful_cycles++;
        } else {
            if (xSemaphoreTake(task_context.sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                task_context.sensor_data_valid = false;
                xSemaphoreGive(task_context.sensor_data_mutex);
            }
            sensor_stats.failed_cycles++;
        }

        sensor_stats.total_cycles++;

        uint32_t cycle_time_ms = (uint32_t)((esp_timer_get_time() - cycle_start_us) / 1000ULL);
        if (cycle_time_ms > sensor_stats.max_cycle_time_ms) {
            sensor_stats.max_cycle_time_ms = cycle_time_ms;
        }
        if (cycle_time_ms < sensor_stats.min_cycle_time_ms) {
            sensor_stats.min_cycle_time_ms = cycle_time_ms;
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_SENSOR));
    }
}

static void display_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "Display task started (interval: %d ms)", TASK_INTERVAL_DISPLAY);

    while (1) {
        if (xQueueReceive(task_context.sensor_data_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdPASS) {
            lvgl_update_sensor_values(sensor_data.ph, sensor_data.ec,
                                      sensor_data.temperature, sensor_data.humidity,
                                      sensor_data.lux, sensor_data.co2);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_DISPLAY));
    }
}

static void notification_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t last_cycle_tick = last_wake_time;

    ESP_LOGI(TAG, "Notification task started (interval: %d ms)", TASK_INTERVAL_NOTIFICATION);

    while (1) {
        TickType_t now_tick = xTaskGetTickCount();
        if (now_tick - last_cycle_tick > pdMS_TO_TICKS(TASK_INTERVAL_NOTIFICATION)) {
            task_context.notification_stats.missed_deadlines++;
        }
        last_cycle_tick = now_tick;

        uint64_t start_us = esp_timer_get_time();
        esp_err_t ret = notification_process();
        task_context.notification_stats.execution_count++;
        if (ret != ESP_OK) {
            task_context.notification_stats.failure_count++;
        }

        uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - start_us) / 1000ULL);
        task_context.notification_stats.last_duration_ms = duration_ms;
        if (duration_ms > task_context.notification_stats.max_duration_ms) {
            task_context.notification_stats.max_duration_ms = duration_ms;
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_NOTIFICATION));
    }
}

static void data_logger_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "Data logger task started (interval: %d ms)", TASK_INTERVAL_DATALOGGER);

    while (1) {
        data_logger_process();
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_DATALOGGER));
    }
}

static void scheduler_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "Scheduler task started (interval: %d ms)", TASK_INTERVAL_SCHEDULER);

    while (1) {
        task_scheduler_process();
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_SCHEDULER));
    }
}

static void ph_ec_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "pH/EC control task started (interval: %d ms)", TASK_INTERVAL_PH_EC);

    while (1) {
        ph_ec_controller_process();
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_PH_EC));
    }
}

static float get_sensor_target(sensor_index_t index)
{
    if (task_context.config_valid && index < SENSOR_COUNT) {
        return task_context.active_config.sensor_config[index].target_value;
    }

    const float defaults[SENSOR_COUNT] = {
        PH_TARGET_DEFAULT,
        EC_TARGET_DEFAULT,
        TEMP_TARGET_DEFAULT,
        HUMIDITY_TARGET_DEFAULT,
        LUX_TARGET_DEFAULT,
        CO2_TARGET_DEFAULT,
    };
    return defaults[index];
}

static float get_sensor_fallback(sensor_index_t index)
{
    float last_value = get_last_sensor_value(index);
    if (!task_context.sensor_data_valid) {
        return get_sensor_target(index);
    }
    return last_value;
}

static float get_last_sensor_value(sensor_index_t index)
{
    if (!task_context.sensor_data_valid) {
        return get_sensor_target(index);
    }

    switch (index) {
        case SENSOR_INDEX_PH:          return task_context.last_sensor_data.ph;
        case SENSOR_INDEX_EC:          return task_context.last_sensor_data.ec;
        case SENSOR_INDEX_TEMPERATURE: return task_context.last_sensor_data.temperature;
        case SENSOR_INDEX_HUMIDITY:    return task_context.last_sensor_data.humidity;
        case SENSOR_INDEX_LUX:         return task_context.last_sensor_data.lux;
        case SENSOR_INDEX_CO2:         return task_context.last_sensor_data.co2;
        default:                       return get_sensor_target(index);
    }
}

static void sensor_update_success(sensor_index_t index, float value)
{
    if (index >= SENSOR_COUNT) {
        return;
    }

    if (task_context.sensor_fault_active[index]) {
        char message[128];
        snprintf(message, sizeof(message), "%s sensor recovered: %.2f %s",
                 SENSOR_NAMES[index], value, SENSOR_UNITS[index]);
        notification_system(NOTIF_TYPE_INFO, message, NOTIF_SOURCE_SENSOR);
        data_logger_log_system_event(LOG_LEVEL_INFO, message);
    }

    task_context.sensor_failure_streak[index] = 0;
    task_context.sensor_fault_active[index] = false;
}

static void sensor_update_failure(sensor_index_t index, const char *reason, float fallback)
{
    if (index >= SENSOR_COUNT) {
        return;
    }

    task_context.sensor_failure_total[index]++;
    uint32_t streak = ++task_context.sensor_failure_streak[index];

    if (!task_context.sensor_fault_active[index] && streak >= SENSOR_FAILURE_THRESHOLD) {
        char message[128];
        snprintf(message, sizeof(message), "%s sensor failure (%s). Using %.2f %s",
                 SENSOR_NAMES[index], (reason != NULL) ? reason : "no data",
                 fallback, SENSOR_UNITS[index]);
        notification_system(NOTIF_TYPE_WARNING, message, NOTIF_SOURCE_SENSOR);
        data_logger_log_alarm(LOG_LEVEL_WARNING, message);
        task_context.sensor_fault_active[index] = true;
    }
}

static esp_err_t read_all_sensors(sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const sensor_interface_t *sensor_if = system_interfaces_get_sensor_interface();
    if (sensor_if == NULL) {
        ERROR_CRITICAL(ERROR_CATEGORY_SENSOR, ESP_ERR_INVALID_STATE, TAG,
                      "Интерфейс датчиков не инициализирован!");
        ESP_LOGE(TAG, "Sensor interface not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memset(data, 0, sizeof(sensor_data_t));
    data->timestamp = esp_timer_get_time();

    int successful_reads = 0;

    float temp = 0.0f;
    float hum = 0.0f;
    if (sensor_if->read_temperature_humidity != NULL &&
        sensor_if->read_temperature_humidity(&temp, &hum)) {
        data->temperature = temp;
        data->humidity = hum;
        data->temp = temp;
        data->hum = hum;
        data->valid[SENSOR_INDEX_TEMPERATURE] = true;
        data->valid[SENSOR_INDEX_HUMIDITY] = true;
        successful_reads += 2;
        register_sensor_recovery(SENSOR_INDEX_TEMPERATURE);
        register_sensor_recovery(SENSOR_INDEX_HUMIDITY);
        ESP_LOGI(TAG, "Temperature/Humidity: %.1fC %.1f%%", temp, hum);
    } else {
        data->temperature = TEMP_TARGET_DEFAULT;
        data->humidity = HUMIDITY_TARGET_DEFAULT;
        char temp_msg[128];
        char hum_msg[128];
        snprintf(temp_msg, sizeof(temp_msg), "используется значение %.1f°C по умолчанию", TEMP_TARGET_DEFAULT);
        snprintf(hum_msg, sizeof(hum_msg), "используется значение %.1f%% по умолчанию", HUMIDITY_TARGET_DEFAULT);
        register_sensor_failure(SENSOR_INDEX_TEMPERATURE, temp_msg);
        register_sensor_failure(SENSOR_INDEX_HUMIDITY, hum_msg);
        ESP_LOGW(TAG, "Failed to read temperature/humidity, fallback to defaults");
    }

    float ph = PH_TARGET_DEFAULT;
    if (sensor_if->read_ph != NULL && sensor_if->read_ph(&ph) == ESP_OK) {
        data->ph = ph;
        data->valid[SENSOR_INDEX_PH] = true;
        sensor_update_success(SENSOR_INDEX_PH, ph);
        successful_reads++;
        register_sensor_recovery(SENSOR_INDEX_PH);
        ESP_LOGI(TAG, "pH read: %.2f", ph);
    } else {
        data->ph = PH_TARGET_DEFAULT;
        register_sensor_failure(SENSOR_INDEX_PH, "применяется целевое значение pH");
        ESP_LOGW(TAG, "pH read failed, using default %.2f", PH_TARGET_DEFAULT);
    }

    // Устанавливаем температуру для компенсации EC измерений (критично для точности!)
    if (data->valid[SENSOR_INDEX_TEMPERATURE]) {
        trema_ec_set_temperature(data->temperature);
        ESP_LOGD(TAG, "Temperature compensation set for EC: %.1fC", data->temperature);
    }
    
    float ec = EC_TARGET_DEFAULT;
    if (sensor_if->read_ec != NULL && sensor_if->read_ec(&ec) == ESP_OK) {
        data->ec = ec;
        data->valid[SENSOR_INDEX_EC] = true;
        sensor_update_success(SENSOR_INDEX_EC, ec);
        successful_reads++;
        register_sensor_recovery(SENSOR_INDEX_EC);
        ESP_LOGI(TAG, "EC read: %.2f mS/cm", ec);
    } else {
        data->ec = EC_TARGET_DEFAULT;
        register_sensor_failure(SENSOR_INDEX_EC, "применяется целевое значение EC");
        ESP_LOGW(TAG, "EC read failed, using default %.2f", EC_TARGET_DEFAULT);
    }

    float lux = LUX_TARGET_DEFAULT;
    if (sensor_if->read_lux != NULL && sensor_if->read_lux(&lux)) {
        data->lux = lux;
        data->valid[SENSOR_INDEX_LUX] = true;
        sensor_update_success(SENSOR_INDEX_LUX, data->lux);
        successful_reads++;
        register_sensor_recovery(SENSOR_INDEX_LUX);
        ESP_LOGI(TAG, "Lux read: %.0f", lux);
    } else {
        data->lux = LUX_TARGET_DEFAULT;
        register_sensor_failure(SENSOR_INDEX_LUX, "применяется целевое значение освещенности");
        ESP_LOGW(TAG, "Lux read failed, using default %.0f", LUX_TARGET_DEFAULT);
    }

    float co2 = CO2_TARGET_DEFAULT;
    float tvoc = 0.0f;
    if (sensor_if->read_co2 != NULL && sensor_if->read_co2(&co2, &tvoc)) {
        data->co2 = co2;
        data->valid[SENSOR_INDEX_CO2] = true;
        sensor_update_success(SENSOR_INDEX_CO2, co2);
        successful_reads++;
        register_sensor_recovery(SENSOR_INDEX_CO2);
        ESP_LOGI(TAG, "CO2 read: %.0f ppm (TVOC: %.0f)", co2, tvoc);
    } else {
        data->co2 = CO2_TARGET_DEFAULT;
        register_sensor_failure(SENSOR_INDEX_CO2, "применяется целевое значение CO2");
        ESP_LOGW(TAG, "CO2 read failed, using default %.0f", CO2_TARGET_DEFAULT);
    }

    ESP_LOGD(TAG, "Sensors read: %d successful values", successful_reads);

    return (successful_reads > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t system_tasks_stop_all(void)
{
    ESP_LOGI(TAG, "Stopping all tasks...");

    if (task_handles.sensor_task) vTaskDelete(task_handles.sensor_task);
    if (task_handles.display_task) vTaskDelete(task_handles.display_task);
    if (task_handles.notification_task) vTaskDelete(task_handles.notification_task);
    if (task_handles.data_logger_task) vTaskDelete(task_handles.data_logger_task);
    if (task_handles.scheduler_task) vTaskDelete(task_handles.scheduler_task);
    if (task_handles.ph_ec_task) vTaskDelete(task_handles.ph_ec_task);

    memset(&task_handles, 0, sizeof(task_handles));

    ESP_LOGI(TAG, "All tasks stopped");
    return ESP_OK;
}

esp_err_t system_tasks_get_stats(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t min_cycle = (sensor_stats.min_cycle_time_ms == UINT32_MAX) ? 0 : sensor_stats.min_cycle_time_ms;

    int written = snprintf(buffer, size,
                           "Tasks running: %d\n"
                           "Free heap: %lu bytes\n"
                           "Min heap: %lu bytes\n",
                           (int)uxTaskGetNumberOfTasks(),
                           (unsigned long)esp_get_free_heap_size(),
                           (unsigned long)esp_get_minimum_free_heap_size());

    if (written < 0 || (size_t)written >= size) {
        return ESP_OK;
    }

    int remaining = (int)(size - (size_t)written);
    written += snprintf(buffer + written, remaining,
                        "Sensor cycles: %lu total, %lu ok, %lu failed\n"
                        "Sensor loop time (ms) min/max: %lu/%lu\n",
                        (unsigned long)sensor_stats.total_cycles,
                        (unsigned long)sensor_stats.successful_cycles,
                        (unsigned long)sensor_stats.failed_cycles,
                        (unsigned long)min_cycle,
                        (unsigned long)sensor_stats.max_cycle_time_ms);

    if (written < 0 || (size_t)written >= size) {
        return ESP_OK;
    }

    remaining = (int)(size - (size_t)written);
    snprintf(buffer + written, remaining,
             "Sensor fault events: pH=%lu EC=%lu T=%lu H=%lu Lux=%lu CO2=%lu\n",
             (unsigned long)sensor_failure_events[SENSOR_INDEX_PH],
             (unsigned long)sensor_failure_events[SENSOR_INDEX_EC],
             (unsigned long)sensor_failure_events[SENSOR_INDEX_TEMPERATURE],
             (unsigned long)sensor_failure_events[SENSOR_INDEX_HUMIDITY],
             (unsigned long)sensor_failure_events[SENSOR_INDEX_LUX],
             (unsigned long)sensor_failure_events[SENSOR_INDEX_CO2]);

    return ESP_OK;
}
