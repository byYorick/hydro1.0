/**
 * @file sensor_manager.c
 * @brief Реализация менеджера датчиков
 */

#include "sensor_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

// Драйверы датчиков
#include "sht3x.h"
#include "ccs811.h"
#include "trema_ph.h"
#include "trema_ec.h"
#include "trema_lux.h"

/*******************************************************************************
 * КОНСТАНТЫ
 ******************************************************************************/

static const char *TAG = "SENSOR_MGR";

// Маппинг типов датчиков на индексы из system_config.h
#define SENSOR_MANAGER_TYPE_TO_INDEX(type) ((type) < SENSOR_TYPE_COUNT ? (type) : 0)

#define RETRY_COUNT             3       // Количество попыток при ошибке
#define RETRY_DELAY_MS          50      // Задержка между попытками
#define CACHE_VALIDITY_MS       5000    // Время актуальности кэша (5 сек)
#define HEALTH_CHECK_WINDOW     50      // Окно для проверки здоровья (последние N чтений)

/*******************************************************************************
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 ******************************************************************************/

// Мьютекс для защиты данных
static SemaphoreHandle_t sensor_mutex = NULL;

// Кэш данных датчиков
static sensor_data_t cached_data = {0};

// Статистика датчиков
static sensor_stats_t stats[SENSOR_TYPE_COUNT] = {0};

// Калибровочные данные
static sensor_calibration_t calibrations[SENSOR_TYPE_COUNT] = {0};

// Флаг инициализации
static bool initialized = false;

// Защита от спама I2C ошибок - счетчики последовательных ошибок
#define MAX_CONSECUTIVE_ERRORS 10  // После 10 ошибок подряд временно отключаем датчик
#define ERROR_RETRY_INTERVAL_MS 60000  // Пытаемся включить через 60 секунд
static uint8_t consecutive_errors[SENSOR_TYPE_COUNT] = {0};
static uint64_t sensor_disabled_until[SENSOR_TYPE_COUNT] = {0};

/*******************************************************************************
 * ВНУТРЕННИЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Обновление статистики датчика с защитой от спама ошибок
 */
static void update_stats(sensor_type_t sensor, bool success)
{
    if (sensor >= SENSOR_TYPE_COUNT) {
        return;
    }
    
    sensor_stats_t *s = &stats[sensor];
    s->total_reads++;
    
    uint64_t now = esp_timer_get_time() / 1000; // мс
    
    if (success) {
        s->successful_reads++;
        s->last_success_time = now;
        // Сбрасываем счетчик ошибок при успехе
        consecutive_errors[sensor] = 0;
        sensor_disabled_until[sensor] = 0;
    } else {
        s->failed_reads++;
        s->last_failure_time = now;
        
        // Увеличиваем счетчик последовательных ошибок
        consecutive_errors[sensor]++;
        
        // Если слишком много ошибок подряд - временно отключаем датчик
        if (consecutive_errors[sensor] >= MAX_CONSECUTIVE_ERRORS) {
            sensor_disabled_until[sensor] = now + ERROR_RETRY_INTERVAL_MS;
            ESP_LOGW(TAG, "Sensor %d disabled until %llu ms (too many errors)", sensor, sensor_disabled_until[sensor]);
            consecutive_errors[sensor] = 0; // Сбрасываем чтобы не переполнить uint8_t
        }
    }
    
    // Расчет процента успешных чтений
    if (s->total_reads > 0) {
        s->success_rate = (float)s->successful_reads / (float)s->total_reads * 100.0f;
    }
    
    // Проверка здоровья (последние HEALTH_CHECK_WINDOW чтений)
    if (s->total_reads >= HEALTH_CHECK_WINDOW) {
        s->is_healthy = (s->success_rate >= 80.0f);
    } else {
        // Если чтений мало, считаем здоровым если нет недавних ошибок
        s->is_healthy = (now - s->last_failure_time > 10000); // 10 сек
    }
}

/**
 * @brief Применение калибровки к значению
 */
static float apply_calibration(sensor_type_t sensor, float value)
{
    if (sensor >= SENSOR_TYPE_COUNT) {
        return value;
    }
    
    sensor_calibration_t *cal = &calibrations[sensor];
    if (!cal->is_calibrated) {
        return value;
    }
    
    return (value * cal->scale) + cal->offset;
}

/**
 * @brief Проверка, не отключен ли датчик временно
 */
static bool is_sensor_enabled(sensor_type_t sensor)
{
    if (sensor >= SENSOR_TYPE_COUNT) {
        return false;
    }
    
    uint64_t now = esp_timer_get_time() / 1000; // мс
    return now >= sensor_disabled_until[sensor];
}

/**
 * @brief Чтение SHT3x с retry
 */
static bool read_sht3x_with_retry(float *temp, float *hum)
{
    // Пропускаем если датчик временно отключен
    if (!is_sensor_enabled(SENSOR_TYPE_TEMPERATURE)) {
        return false;
    }
    
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (sht3x_read(temp, hum)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    return false;
}

/**
 * @brief Чтение pH с retry
 */
static bool read_ph_with_retry(float *ph)
{
    // Пропускаем если датчик временно отключен
    if (!is_sensor_enabled(SENSOR_TYPE_PH)) {
        return false;
    }
    
    for (int i = 0; i < RETRY_COUNT; i++) {
        esp_err_t ret = trema_ph_read(ph);
        if (ret == ESP_OK && !isnan(*ph) && *ph >= 0.0f && *ph <= 14.0f) {
            // Применяем калибровку
            *ph = apply_calibration(SENSOR_TYPE_PH, *ph);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    return false;
}

/**
 * @brief Чтение EC с retry
 */
static bool read_ec_with_retry(float *ec)
{
    // Пропускаем если датчик временно отключен
    if (!is_sensor_enabled(SENSOR_TYPE_EC)) {
        return false;
    }
    
    for (int i = 0; i < RETRY_COUNT; i++) {
        esp_err_t ret = trema_ec_read(ec);
        if (ret == ESP_OK && !isnan(*ec) && *ec >= 0.0f) {
            // Применяем калибровку
            *ec = apply_calibration(SENSOR_TYPE_EC, *ec);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    return false;
}

/**
 * @brief Чтение Lux с retry
 */
static bool read_lux_with_retry(float *lux)
{
    // Пропускаем если датчик временно отключен
    if (!is_sensor_enabled(SENSOR_TYPE_LUX)) {
        return false;
    }
    
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (trema_lux_read_float(lux) && !isnan(*lux) && *lux >= 0.0f) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    return false;
}

/**
 * @brief Чтение CCS811 с retry
 */
static bool read_ccs811_with_retry(float *co2, float *tvoc)
{
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (ccs811_read_data(co2, tvoc)) {
            if (!isnan(*co2) && !isnan(*tvoc) && *co2 >= 0.0f && *tvoc >= 0.0f) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    return false;
}

/*******************************************************************************
 * ПУБЛИЧНЫЕ ФУНКЦИИ
 ******************************************************************************/

esp_err_t sensor_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Создаем мьютекс
    sensor_mutex = xSemaphoreCreateMutex();
    if (sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Инициализируем кэш
    memset(&cached_data, 0, sizeof(sensor_data_t));
    memset(stats, 0, sizeof(stats));
    memset(calibrations, 0, sizeof(calibrations));
    
    // Устанавливаем калибровку по умолчанию (scale=1, offset=0)
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        calibrations[i].scale = 1.0f;
        calibrations[i].offset = 0.0f;
        calibrations[i].is_calibrated = false;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Sensor Manager initialized");
    
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(sensor_data_t *data)
{
    if (!initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Берем мьютекс
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    uint64_t now = esp_timer_get_time(); // микросекунды
    
    // Читаем SHT3x (температура, влажность)
    bool temp_ok = read_sht3x_with_retry(&cached_data.temperature, &cached_data.humidity);
    cached_data.temp = cached_data.temperature;  // Алиас
    cached_data.hum = cached_data.humidity;      // Алиас
    cached_data.valid[SENSOR_INDEX_TEMPERATURE] = temp_ok;
    cached_data.valid[SENSOR_INDEX_HUMIDITY] = temp_ok;
    update_stats(SENSOR_TYPE_TEMPERATURE, temp_ok);
    update_stats(SENSOR_TYPE_HUMIDITY, temp_ok);
    
    // Читаем pH
    bool ph_ok = read_ph_with_retry(&cached_data.ph);
    cached_data.valid[SENSOR_INDEX_PH] = ph_ok;
    update_stats(SENSOR_TYPE_PH, ph_ok);
    
    // Читаем EC
    bool ec_ok = read_ec_with_retry(&cached_data.ec);
    cached_data.valid[SENSOR_INDEX_EC] = ec_ok;
    update_stats(SENSOR_TYPE_EC, ec_ok);
    
    // Читаем Lux
    bool lux_ok = read_lux_with_retry(&cached_data.lux);
    cached_data.valid[SENSOR_INDEX_LUX] = lux_ok;
    update_stats(SENSOR_TYPE_LUX, lux_ok);
    
    // Читаем CCS811
    float tvoc_temp = 0.0f; // Временная переменная для tvoc (нет поля в sensor_data_t)
    bool co2_ok = read_ccs811_with_retry(&cached_data.co2, &tvoc_temp);
    cached_data.valid[SENSOR_INDEX_CO2] = co2_ok;
    // TVOC читается, но не сохраняется в cached_data (нет поля)
    update_stats(SENSOR_TYPE_CO2, co2_ok);
    update_stats(SENSOR_TYPE_TVOC, co2_ok);
    
    // Обновляем timestamp
    cached_data.timestamp = now;
    
    // Копируем данные
    memcpy(data, &cached_data, sizeof(sensor_data_t));
    
    xSemaphoreGive(sensor_mutex);
    
    // Логируем результаты
    ESP_LOGD(TAG, "Sensors read: T=%.1f°C, H=%.1f%%, pH=%.2f, EC=%.2f, Lux=%.0f, CO2=%.0f",
             cached_data.temperature, cached_data.humidity, cached_data.ph,
             cached_data.ec, cached_data.lux, cached_data.co2);
    
    // Возвращаем успех если хотя бы один датчик прочитан
    bool any_ok = temp_ok || ph_ok || ec_ok || lux_ok || co2_ok;
    return any_ok ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_manager_get_cached_data(sensor_data_t *data)
{
    if (!initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(data, &cached_data, sizeof(sensor_data_t));
    
    xSemaphoreGive(sensor_mutex);
    
    return ESP_OK;
}

esp_err_t sensor_manager_read_ph(float *ph)
{
    if (!initialized || ph == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool ok = read_ph_with_retry(ph);
    if (ok) {
        cached_data.ph = *ph;
        cached_data.valid[SENSOR_INDEX_PH] = true;
        cached_data.timestamp = esp_timer_get_time();
    }
    
    update_stats(SENSOR_TYPE_PH, ok);
    
    xSemaphoreGive(sensor_mutex);
    
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_manager_read_ec(float *ec)
{
    if (!initialized || ec == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool ok = read_ec_with_retry(ec);
    if (ok) {
        cached_data.ec = *ec;
        cached_data.valid[SENSOR_INDEX_EC] = true;
        cached_data.timestamp = esp_timer_get_time();
    }
    
    update_stats(SENSOR_TYPE_EC, ok);
    
    xSemaphoreGive(sensor_mutex);
    
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_manager_read_temp_humidity(float *temperature, float *humidity)
{
    if (!initialized || temperature == NULL || humidity == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool ok = read_sht3x_with_retry(temperature, humidity);
    if (ok) {
        cached_data.temperature = *temperature;
        cached_data.humidity = *humidity;
        cached_data.temp = *temperature;  // Алиас
        cached_data.hum = *humidity;      // Алиас
        cached_data.valid[SENSOR_INDEX_TEMPERATURE] = true;
        cached_data.valid[SENSOR_INDEX_HUMIDITY] = true;
        cached_data.timestamp = esp_timer_get_time();
    }
    
    update_stats(SENSOR_TYPE_TEMPERATURE, ok);
    update_stats(SENSOR_TYPE_HUMIDITY, ok);
    
    xSemaphoreGive(sensor_mutex);
    
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_manager_read_lux(float *lux)
{
    if (!initialized || lux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool ok = read_lux_with_retry(lux);
    if (ok) {
        cached_data.lux = *lux;
        cached_data.valid[SENSOR_INDEX_LUX] = true;
        cached_data.timestamp = esp_timer_get_time();
    }
    
    update_stats(SENSOR_TYPE_LUX, ok);
    
    xSemaphoreGive(sensor_mutex);
    
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t sensor_manager_read_air_quality(float *co2, float *tvoc)
{
    if (!initialized || co2 == NULL || tvoc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool ok = read_ccs811_with_retry(co2, tvoc);
    if (ok) {
        cached_data.co2 = *co2;
        // tvoc не сохраняем, возвращаем через параметр
        cached_data.valid[SENSOR_INDEX_CO2] = true;
        cached_data.timestamp = esp_timer_get_time();
    }
    
    update_stats(SENSOR_TYPE_CO2, ok);
    update_stats(SENSOR_TYPE_TVOC, ok);
    
    xSemaphoreGive(sensor_mutex);
    
    return ok ? ESP_OK : ESP_FAIL;
}

bool sensor_manager_is_sensor_healthy(sensor_type_t sensor)
{
    if (!initialized || sensor >= SENSOR_TYPE_COUNT) {
        return false;
    }
    
    return stats[sensor].is_healthy;
}

esp_err_t sensor_manager_get_stats(sensor_type_t sensor, sensor_stats_t *out_stats)
{
    if (!initialized || sensor >= SENSOR_TYPE_COUNT || out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(out_stats, &stats[sensor], sizeof(sensor_stats_t));
    
    xSemaphoreGive(sensor_mutex);
    
    return ESP_OK;
}

esp_err_t sensor_manager_calibrate_ph(float measured_value, float actual_value)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Простая калибровка через offset
    calibrations[SENSOR_TYPE_PH].offset = actual_value - measured_value;
    calibrations[SENSOR_TYPE_PH].scale = 1.0f;
    calibrations[SENSOR_TYPE_PH].is_calibrated = true;
    calibrations[SENSOR_TYPE_PH].calibration_date = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "pH calibrated: measured=%.2f, actual=%.2f, offset=%.2f",
             measured_value, actual_value, calibrations[SENSOR_TYPE_PH].offset);
    
    xSemaphoreGive(sensor_mutex);
    
    return ESP_OK;
}

esp_err_t sensor_manager_calibrate_ec(float measured_value, float actual_value)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Простая калибровка через offset
    calibrations[SENSOR_TYPE_EC].offset = actual_value - measured_value;
    calibrations[SENSOR_TYPE_EC].scale = 1.0f;
    calibrations[SENSOR_TYPE_EC].is_calibrated = true;
    calibrations[SENSOR_TYPE_EC].calibration_date = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "EC calibrated: measured=%.2f, actual=%.2f, offset=%.2f",
             measured_value, actual_value, calibrations[SENSOR_TYPE_EC].offset);
    
    xSemaphoreGive(sensor_mutex);
    
    return ESP_OK;
}

esp_err_t sensor_manager_get_calibration(sensor_type_t sensor, sensor_calibration_t *calibration)
{
    if (!initialized || sensor >= SENSOR_TYPE_COUNT || calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(calibration, &calibrations[sensor], sizeof(sensor_calibration_t));
    
    xSemaphoreGive(sensor_mutex);
    
    return ESP_OK;
}

esp_err_t sensor_manager_reset_stats(sensor_type_t sensor)
{
    if (!initialized || sensor >= SENSOR_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memset(&stats[sensor], 0, sizeof(sensor_stats_t));
    
    xSemaphoreGive(sensor_mutex);
    
    ESP_LOGI(TAG, "Stats reset for sensor %d", sensor);
    
    return ESP_OK;
}

uint64_t sensor_manager_get_last_update_time(void)
{
    if (!initialized) {
        return 0;
    }
    
    return cached_data.timestamp;
}

esp_err_t sensor_manager_set_auto_update_interval(uint32_t interval_ms)
{
    // TODO: Реализовать если нужна автоматическая задача
    // Сейчас обновление происходит через sensor_task в system_tasks.c
    ESP_LOGW(TAG, "Auto-update not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

