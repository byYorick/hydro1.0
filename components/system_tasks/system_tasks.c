/**
 * @file system_tasks.c
 * @brief Реализация задач FreeRTOS системы гидропоники
 */

#include "system_tasks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

// Компоненты системы
#include "notification_system.h"
#include "data_logger.h"
#include "task_scheduler.h"
#include "ph_ec_controller.h"
#include "config_manager.h"

// Датчики
#include "sht3x.h"
#include "ccs811.h"
#include "trema_ph.h"
#include "trema_ec.h"
#include "trema_lux.h"

// UI
#include "lvgl_main.h"

// Энкодер
#include "encoder.h"

static const char *TAG = "SYS_TASKS";

/*******************************************************************************
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

static system_tasks_context_t task_context = {0};
static system_task_handles_t task_handles = {0};
static bool context_initialized = false;

/*******************************************************************************
 * ПРОТОТИПЫ ФУНКЦИЙ ЗАДАЧ
 ******************************************************************************/

static void sensor_task(void *pvParameters);
static void display_task(void *pvParameters);
static void notification_task(void *pvParameters);
static void data_logger_task(void *pvParameters);
static void scheduler_task(void *pvParameters);
static void ph_ec_task(void *pvParameters);
// encoder_task создается в lvgl_main.c - не дублируем!

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

static esp_err_t read_all_sensors(sensor_data_t *data);

/*******************************************************************************
 * ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
 ******************************************************************************/

esp_err_t system_tasks_init_context(void)
{
    if (context_initialized) {
        ESP_LOGW(TAG, "Context already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing task context...");

    // Создаем мьютекс для защиты данных датчиков
    task_context.sensor_data_mutex = xSemaphoreCreateMutex();
    if (task_context.sensor_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor_data_mutex");
        return ESP_ERR_NO_MEM;
    }

    // Создаем очередь данных датчиков
    task_context.sensor_data_queue = xQueueCreate(QUEUE_SIZE_SENSOR_DATA, sizeof(sensor_data_t));
    if (task_context.sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor_data_queue");
        return ESP_ERR_NO_MEM;
    }

    // Очередь энкодера создается в encoder.c - не дублируем!

    task_context.sensor_data_valid = false;
    memset(&task_context.last_sensor_data, 0, sizeof(sensor_data_t));

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

    // Задача чтения датчиков
    ret = xTaskCreate(sensor_task, "sensor_task", TASK_STACK_SIZE_SENSOR, NULL,
                      TASK_PRIORITY_SENSOR, &task_handles.sensor_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ sensor_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_SENSOR, TASK_STACK_SIZE_SENSOR);

    // Задача обновления дисплея
    ret = xTaskCreate(display_task, "display_task", TASK_STACK_SIZE_DISPLAY, NULL,
                      TASK_PRIORITY_DISPLAY, &task_handles.display_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ display_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_DISPLAY, TASK_STACK_SIZE_DISPLAY);

    // Задача уведомлений
    ret = xTaskCreate(notification_task, "notification_task", TASK_STACK_SIZE_NOTIFICATION, NULL,
                      TASK_PRIORITY_NOTIFICATION, &task_handles.notification_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create notification_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ notification_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_NOTIFICATION, TASK_STACK_SIZE_NOTIFICATION);

    // Задача логирования
    ret = xTaskCreate(data_logger_task, "data_logger_task", TASK_STACK_SIZE_DATALOGGER, NULL,
                      TASK_PRIORITY_DATALOGGER, &task_handles.data_logger_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create data_logger_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ data_logger_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_DATALOGGER, TASK_STACK_SIZE_DATALOGGER);

    // Задача планировщика
    ret = xTaskCreate(scheduler_task, "scheduler_task", TASK_STACK_SIZE_SCHEDULER, NULL,
                      TASK_PRIORITY_SCHEDULER, &task_handles.scheduler_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ scheduler_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_SCHEDULER, TASK_STACK_SIZE_SCHEDULER);

    // Задача pH/EC контроля
    ret = xTaskCreate(ph_ec_task, "ph_ec_task", TASK_STACK_SIZE_PH_EC, NULL,
                      TASK_PRIORITY_PH_EC, &task_handles.ph_ec_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ph_ec_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ ph_ec_task created (Pri: %d, Stack: %d)", 
             TASK_PRIORITY_PH_EC, TASK_STACK_SIZE_PH_EC);

    // Задача энкодера создается в lvgl_main.c - не дублируем!
    ESP_LOGI(TAG, "✓ encoder_task will be created by lvgl_main.c");

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

/*******************************************************************************
 * РЕАЛИЗАЦИЯ ЗАДАЧ
 ******************************************************************************/

static void sensor_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t read_count = 0;

    ESP_LOGI(TAG, "Sensor task started (interval: %d ms)", TASK_INTERVAL_SENSOR);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Задержка для инициализации UI

    while (1) {
        read_count++;
        ESP_LOGD(TAG, "Reading sensors (cycle %lu)", (unsigned long)read_count);

        // Читаем все датчики
        esp_err_t ret = read_all_sensors(&sensor_data);

        if (ret == ESP_OK) {
            // Обновляем глобальные данные с защитой мьютексом
            if (xSemaphoreTake(task_context.sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                task_context.last_sensor_data = sensor_data;
                task_context.sensor_data_valid = true;
                xSemaphoreGive(task_context.sensor_data_mutex);
            }

            // Отправляем данные в очередь для display_task
            xQueueSend(task_context.sensor_data_queue, &sensor_data, 0);

            // Обновляем контроллер pH/EC
            ph_ec_controller_update_values(sensor_data.ph, sensor_data.ec);
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
        // Получаем данные из очереди
        if (xQueueReceive(task_context.sensor_data_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdPASS) {
            // Обновляем дисплей
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

    ESP_LOGI(TAG, "Notification task started (interval: %d ms)", TASK_INTERVAL_NOTIFICATION);

    while (1) {
        // Обрабатываем уведомления
        notification_process();

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_NOTIFICATION));
    }
}

static void data_logger_task(void *pvParameters)
{
    ESP_LOGW(TAG, "Data logger task disabled (memory optimization)");
    vTaskDelete(NULL);
}

static void scheduler_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "Scheduler task started (interval: %d ms)", TASK_INTERVAL_SCHEDULER);

    while (1) {
        // Выполняем готовые задачи планировщика
        task_scheduler_process();

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_SCHEDULER));
    }
}

static void ph_ec_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "pH/EC control task started (interval: %d ms)", TASK_INTERVAL_PH_EC);

    while (1) {
        // Обрабатываем контроллер pH/EC
        ph_ec_controller_process();

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_PH_EC));
    }
}

// encoder_task реализована в lvgl_main.c - не дублируем!

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

static esp_err_t read_all_sensors(sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(sensor_data_t));
    data->timestamp = esp_timer_get_time();

    int successful_reads = 0;
    esp_err_t ret;

    // Чтение температуры и влажности (SHT3x)
    float temp, hum;
    bool sht3x_ok = sht3x_read(&temp, &hum);
    if (sht3x_ok) {
        data->temperature = temp;
        data->humidity = hum;
        data->temp = temp;
        data->hum = hum;
        data->valid[SENSOR_INDEX_TEMPERATURE] = true;
        data->valid[SENSOR_INDEX_HUMIDITY] = true;
        successful_reads++;
        ESP_LOGI(TAG, "SHT3x read: temp=%.1f°C, hum=%.1f%%", temp, hum);
    } else {
        data->temperature = TEMP_TARGET_DEFAULT;
        data->humidity = HUMIDITY_TARGET_DEFAULT;
        ESP_LOGW(TAG, "SHT3x read failed, using defaults");
    }

    // Чтение pH
    float ph;
    ret = trema_ph_read(&ph);
    if (ret == ESP_OK) {
        data->ph = ph;
        data->valid[SENSOR_INDEX_PH] = true;
        successful_reads++;
        ESP_LOGI(TAG, "pH read: %.2f", ph);
    } else {
        data->ph = PH_TARGET_DEFAULT;
        ESP_LOGW(TAG, "pH read failed (ret=%s), using default %.2f", esp_err_to_name(ret), PH_TARGET_DEFAULT);
    }

    // Чтение EC
    float ec;
    ret = trema_ec_read(&ec);
    if (ret == ESP_OK) {
        data->ec = ec;
        data->valid[SENSOR_INDEX_EC] = true;
        successful_reads++;
        ESP_LOGI(TAG, "EC read: %.2f mS/cm", ec);
    } else {
        data->ec = EC_TARGET_DEFAULT;
        ESP_LOGW(TAG, "EC read failed (ret=%s), using default %.2f", esp_err_to_name(ret), EC_TARGET_DEFAULT);
    }

    // Чтение освещенности
    uint16_t lux_raw;
    bool lux_ok = trema_lux_read(&lux_raw);
    if (lux_ok) {
        data->lux = (float)lux_raw;
        data->valid[SENSOR_INDEX_LUX] = true;
        successful_reads++;
        ESP_LOGI(TAG, "LUX read: %d lux", lux_raw);
    } else {
        data->lux = LUX_TARGET_DEFAULT;
        ESP_LOGW(TAG, "LUX read failed, using default %.0f", LUX_TARGET_DEFAULT);
    }

    // Чтение CO2
    float co2, tvoc;
    bool co2_ok = ccs811_read_data(&co2, &tvoc);
    if (co2_ok) {
        data->co2 = co2;
        data->valid[SENSOR_INDEX_CO2] = true;
        successful_reads++;
        ESP_LOGI(TAG, "CO2 read: %.0f ppm (TVOC: %.0f)", co2, tvoc);
    } else {
        data->co2 = CO2_TARGET_DEFAULT;
        ESP_LOGW(TAG, "CO2 read failed, using default %.0f", CO2_TARGET_DEFAULT);
    }

    ESP_LOGD(TAG, "Sensors read: %d/%d successful (pH=%.2f, EC=%.2f, T=%.1f, H=%.1f)", 
             successful_reads, 5, data->ph, data->ec, data->temperature, data->humidity);

    return (successful_reads > 0) ? ESP_OK : ESP_FAIL;
}

/*******************************************************************************
 * ФУНКЦИИ УПРАВЛЕНИЯ
 ******************************************************************************/

esp_err_t system_tasks_stop_all(void)
{
    ESP_LOGI(TAG, "Stopping all tasks...");

    if (task_handles.sensor_task) vTaskDelete(task_handles.sensor_task);
    if (task_handles.display_task) vTaskDelete(task_handles.display_task);
    if (task_handles.notification_task) vTaskDelete(task_handles.notification_task);
    if (task_handles.data_logger_task) vTaskDelete(task_handles.data_logger_task);
    if (task_handles.scheduler_task) vTaskDelete(task_handles.scheduler_task);
    if (task_handles.ph_ec_task) vTaskDelete(task_handles.ph_ec_task);
    // encoder_task управляется в lvgl_main.c

    memset(&task_handles, 0, sizeof(task_handles));

    ESP_LOGI(TAG, "All tasks stopped");
    return ESP_OK;
}

esp_err_t system_tasks_get_stats(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(buffer, size,
             "Tasks running: %d\n"
             "Free heap: %lu bytes\n"
             "Min heap: %lu bytes\n",
             (int)uxTaskGetNumberOfTasks(),
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size());

    return ESP_OK;
}

