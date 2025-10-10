/**
 * @file pump_manager.c
 * @brief Реализация менеджера насосов
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "pump_manager.h"
#include "peristaltic_pump.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "PUMP_MANAGER";

// Пины насосов (из system_config.h)
static const struct {
    int ia;
    int ib;
} pump_pins[PUMP_INDEX_COUNT] = {
    {PUMP_PH_UP_IA, PUMP_PH_UP_IB},         // PUMP_INDEX_PH_UP
    {PUMP_PH_DOWN_IA, PUMP_PH_DOWN_IB},     // PUMP_INDEX_PH_DOWN
    {PUMP_EC_A_IA, PUMP_EC_A_IB},           // PUMP_INDEX_EC_A
    {PUMP_EC_B_IA, PUMP_EC_B_IB},           // PUMP_INDEX_EC_B
    {PUMP_EC_C_IA, PUMP_EC_C_IB},           // PUMP_INDEX_EC_C
    {PUMP_WATER_IA, PUMP_WATER_IB},         // PUMP_INDEX_WATER
};

// Статистика насосов
static pump_stats_t pump_stats[PUMP_INDEX_COUNT] = {0};
static SemaphoreHandle_t pump_mutex = NULL;
static bool manager_initialized = false;

// Ограничения безопасности
#define PUMP_MIN_DOSE_ML 0.1f
#define PUMP_MAX_DOSE_ML 100.0f
#define PUMP_MIN_INTERVAL_SEC 10
#define PUMP_MAX_DOSES_PER_HOUR 20

/**
 * @brief Обновление счетчика доз в час
 */
static void update_hourly_counter(pump_index_t pump_idx) {
    uint32_t current_time = esp_timer_get_time() / 1000000; // секунды
    uint32_t hour_ago = current_time - 3600;
    
    // Сбрасываем счетчик если прошло более часа
    if (pump_stats[pump_idx].last_dose_timestamp < hour_ago) {
        pump_stats[pump_idx].doses_in_last_hour = 0;
    }
}

/**
 * @brief Проверка ограничений безопасности
 */
static bool check_safety_limits(pump_index_t pump_idx, float volume_ml) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return false;
    }
    
    // Проверка объема
    if (volume_ml < PUMP_MIN_DOSE_ML || volume_ml > PUMP_MAX_DOSE_ML) {
        ESP_LOGW(TAG, "Pump %d: dose %.2f ml out of range [%.1f-%.1f]", 
                 pump_idx, volume_ml, PUMP_MIN_DOSE_ML, PUMP_MAX_DOSE_ML);
        return false;
    }
    
    update_hourly_counter(pump_idx);
    
    // Проверка количества доз в час
    if (pump_stats[pump_idx].doses_in_last_hour >= PUMP_MAX_DOSES_PER_HOUR) {
        ESP_LOGW(TAG, "Pump %d: max doses per hour reached (%d)", 
                 pump_idx, PUMP_MAX_DOSES_PER_HOUR);
        return false;
    }
    
    // Проверка минимального интервала
    uint32_t current_time = esp_timer_get_time() / 1000000;
    uint32_t time_since_last = current_time - pump_stats[pump_idx].last_dose_timestamp;
    
    if (time_since_last < PUMP_MIN_INTERVAL_SEC && pump_stats[pump_idx].total_doses > 0) {
        ESP_LOGW(TAG, "Pump %d: min interval not met (%d < %d sec)", 
                 pump_idx, time_since_last, PUMP_MIN_INTERVAL_SEC);
        return false;
    }
    
    // Проверка статуса
    if (pump_stats[pump_idx].status == PUMP_STATUS_ERROR) {
        ESP_LOGW(TAG, "Pump %d: in error state", pump_idx);
        return false;
    }
    
    return true;
}

/**
 * @brief Инициализация менеджера
 */
esp_err_t pump_manager_init(void) {
    if (manager_initialized) {
        ESP_LOGW(TAG, "Pump manager already initialized");
        return ESP_OK;
    }
    
    // Создание мьютекса
    if (pump_mutex == NULL) {
        pump_mutex = xSemaphoreCreateMutex();
        if (pump_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    
    // Инициализация всех насосов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pump_init(pump_pins[i].ia, pump_pins[i].ib);
        pump_stats[i].status = PUMP_STATUS_IDLE;
        ESP_LOGI(TAG, "Pump %d initialized (IA=%d, IB=%d)", 
                 i, pump_pins[i].ia, pump_pins[i].ib);
    }
    
    manager_initialized = true;
    ESP_LOGI(TAG, "Pump manager initialized (%d pumps)", PUMP_INDEX_COUNT);
    
    xSemaphoreGive(pump_mutex);
    return ESP_OK;
}

/**
 * @brief Дозирование
 */
esp_err_t pump_manager_dose(pump_index_t pump_idx, float volume_ml) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    
    // Проверка безопасности
    if (!check_safety_limits(pump_idx, volume_ml)) {
        xSemaphoreGive(pump_mutex);
        return ESP_FAIL;
    }
    
    // Загрузка конфигурации
    const system_config_t *config = config_manager_get_cached();
    if (config == NULL) {
        xSemaphoreGive(pump_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    const pump_config_t *pump_cfg = &config->pump_config[pump_idx];
    
    if (!pump_cfg->enabled) {
        ESP_LOGW(TAG, "Pump %d disabled in config", pump_idx);
        xSemaphoreGive(pump_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Расчет времени работы
    float flow_rate = pump_cfg->flow_rate_ml_per_sec;
    if (flow_rate <= 0.0f) {
        ESP_LOGE(TAG, "Invalid flow rate for pump %d: %.3f", pump_idx, flow_rate);
        xSemaphoreGive(pump_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t duration_ms = (uint32_t)((volume_ml / flow_rate) * 1000.0f);
    
    // Проверка длительности
    if (duration_ms < pump_cfg->min_duration_ms) {
        duration_ms = pump_cfg->min_duration_ms;
    }
    if (duration_ms > pump_cfg->max_duration_ms) {
        ESP_LOGW(TAG, "Duration %d ms exceeds max %d ms", duration_ms, pump_cfg->max_duration_ms);
        duration_ms = pump_cfg->max_duration_ms;
    }
    
    // Обновление статуса
    pump_stats[pump_idx].status = PUMP_STATUS_RUNNING;
    
    ESP_LOGI(TAG, "Pump %d: dosing %.2f ml (%d ms)", pump_idx, volume_ml, duration_ms);
    
    // Физическое управление насосом
    pump_run_ms(pump_pins[pump_idx].ia, pump_pins[pump_idx].ib, duration_ms);
    
    // Обновление статистики
    uint32_t current_time = esp_timer_get_time() / 1000000;
    pump_stats[pump_idx].total_doses++;
    pump_stats[pump_idx].total_ml_dispensed += volume_ml;
    pump_stats[pump_idx].last_dose_timestamp = current_time;
    pump_stats[pump_idx].doses_in_last_hour++;
    pump_stats[pump_idx].total_runtime_sec += duration_ms / 1000;
    pump_stats[pump_idx].status = PUMP_STATUS_COOLDOWN;
    
    xSemaphoreGive(pump_mutex);
    
    // Cooldown
    vTaskDelay(pdMS_TO_TICKS(pump_cfg->cooldown_ms));
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    pump_stats[pump_idx].status = PUMP_STATUS_IDLE;
    xSemaphoreGive(pump_mutex);
    
    return ESP_OK;
}

/**
 * @brief Получение статуса
 */
pump_status_t pump_manager_get_status(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return PUMP_STATUS_ERROR;
    }
    return pump_stats[pump_idx].status;
}

/**
 * @brief Получение статистики
 */
esp_err_t pump_manager_get_stats(pump_index_t pump_idx, pump_stats_t *stats) {
    if (pump_idx >= PUMP_INDEX_COUNT || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    *stats = pump_stats[pump_idx];
    xSemaphoreGive(pump_mutex);
    
    return ESP_OK;
}

/**
 * @brief Сброс статистики
 */
esp_err_t pump_manager_reset_stats(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    
    pump_status_t saved_status = pump_stats[pump_idx].status;
    memset(&pump_stats[pump_idx], 0, sizeof(pump_stats_t));
    pump_stats[pump_idx].status = saved_status;
    
    ESP_LOGI(TAG, "Pump %d stats reset", pump_idx);
    
    xSemaphoreGive(pump_mutex);
    return ESP_OK;
}

/**
 * @brief Экстренная остановка
 */
esp_err_t pump_manager_emergency_stop(void) {
    ESP_LOGW(TAG, "EMERGENCY STOP - all pumps");
    
    // Останавливаем все насосы
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        gpio_set_level(pump_pins[i].ia, 0);
        gpio_set_level(pump_pins[i].ib, 0);
        pump_stats[i].status = PUMP_STATUS_IDLE;
    }
    
    return ESP_OK;
}

/**
 * @brief Проверка возможности дозирования
 */
bool pump_manager_can_dose(pump_index_t pump_idx, float volume_ml) {
    if (!manager_initialized || pump_idx >= PUMP_INDEX_COUNT) {
        return false;
    }
    
    xSemaphoreTake(pump_mutex, portMAX_DELAY);
    bool can_dose = check_safety_limits(pump_idx, volume_ml);
    xSemaphoreGive(pump_mutex);
    
    return can_dose;
}

/**
 * @brief Тестовый запуск
 */
esp_err_t pump_manager_test(pump_index_t pump_idx, uint32_t duration_ms) {
    if (!manager_initialized || pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Test pump %d for %d ms", pump_idx, duration_ms);
    
    pump_run_ms(pump_pins[pump_idx].ia, pump_pins[pump_idx].ib, duration_ms);
    
    return ESP_OK;
}

