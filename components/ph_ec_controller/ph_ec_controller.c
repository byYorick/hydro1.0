#include "ph_ec_controller.h"
#include "pump_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <string.h>
#include <math.h>
#include "peristaltic_pump.h"
#include "system_interfaces.h"

static const char *TAG = "PH_EC_CTRL";

// GPIO пины для насосов определены в system_config.h

// Глобальные переменные
static pump_config_t g_pump_configs[PUMP_INDEX_COUNT];
static ph_control_params_t g_ph_params;
static ec_control_params_t g_ec_params;
static bool g_ph_auto_mode = false;
static bool g_ec_auto_mode = false;
static SemaphoreHandle_t g_mutex = NULL;
static ph_ec_pump_callback_t g_pump_callback = NULL;
static ph_ec_correction_callback_t g_correction_callback = NULL;
static float g_current_ph = 7.0f;
static float g_current_ec = 1.0f;

// GPIO пины для насосов (один пин на насос - через оптопару)
static const int PUMP_PINS[PUMP_INDEX_COUNT] = {
    PUMP_PH_UP_PIN,
    PUMP_PH_DOWN_PIN,
    PUMP_EC_A_PIN,
    PUMP_EC_B_PIN,
    PUMP_EC_C_PIN,
    PUMP_WATER_PIN
};

static void notify_pump_callback(pump_index_t pump_idx, bool started)
{
    if (g_pump_callback != NULL) {
        g_pump_callback(pump_idx, started);
    }
}

// Устаревшая функция - теперь используется pump_manager
static esp_err_t run_pump_with_interface(pump_index_t pump_idx, uint32_t duration_ms) __attribute__((unused));
static esp_err_t run_pump_with_interface(pump_index_t pump_idx, uint32_t duration_ms)
{
    const actuator_interface_t *actuator = system_interfaces_get_actuator_interface();
    notify_pump_callback(pump_idx, true);

    esp_err_t err = ESP_OK;
    if (actuator != NULL && actuator->run_pump_ms != NULL) {
        err = actuator->run_pump_ms(pump_idx, duration_ms);
    } else {
        pump_run_ms(PUMP_PINS[pump_idx], duration_ms);
    }

    notify_pump_callback(pump_idx, false);
    return err;
}

esp_err_t ph_ec_controller_init(void)
{
    // Создаем mutex
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Инициализируем все насосы
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pump_init(PUMP_PINS[i]);
        
        // Устанавливаем конфигурацию по умолчанию
        g_pump_configs[i].enabled = true;
        g_pump_configs[i].flow_rate_ml_per_sec = 10.0f;
        g_pump_configs[i].min_duration_ms = 100;
        g_pump_configs[i].max_duration_ms = 5000;
        g_pump_configs[i].cooldown_ms = 60000; // 1 минута
        g_pump_configs[i].concentration_factor = 1.0f;
        strncpy(g_pump_configs[i].name, PUMP_NAMES[i], sizeof(g_pump_configs[i].name) - 1);
    }

    // Параметры pH по умолчанию
    g_ph_params.target_ph = 6.5f;
    g_ph_params.deadband = 0.2f;
    g_ph_params.max_correction_step = 0.5f;
    g_ph_params.correction_interval_ms = 300000; // 5 минут

    // Параметры EC по умолчанию
    g_ec_params.target_ec = 1.5f;
    g_ec_params.deadband = 0.1f;
    g_ec_params.max_correction_step = 0.2f;
    g_ec_params.correction_interval_ms = 300000; // 5 минут
    g_ec_params.ratio_a = 0.4f;
    g_ec_params.ratio_b = 0.4f;
    g_ec_params.ratio_c = 0.2f;

    ESP_LOGI(TAG, "pH/EC controller initialized with %d pumps", PUMP_INDEX_COUNT);
    return ESP_OK;
}

esp_err_t ph_ec_controller_set_pump_config(pump_index_t pump_idx,
                                           const pump_config_t *config)
{
    if (pump_idx >= PUMP_INDEX_COUNT || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_pump_configs[pump_idx], config, sizeof(pump_config_t));

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Pump %d config updated", pump_idx);
    return ESP_OK;
}

esp_err_t ph_ec_controller_apply_config(const system_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        g_pump_configs[i] = config->pump_config[i];
    }

    g_ph_params.target_ph = config->sensor_config[SENSOR_INDEX_PH].target_value;
    g_ec_params.target_ec = config->sensor_config[SENSOR_INDEX_EC].target_value;

    g_ph_auto_mode = config->auto_control_enabled;
    g_ec_auto_mode = config->auto_control_enabled;

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Controller config applied (auto mode: %s)",
             config->auto_control_enabled ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t ph_ec_controller_set_ph_params(const ph_control_params_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_ph_params, params, sizeof(ph_control_params_t));

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "pH params updated (target: %.2f)", params->target_ph);
    return ESP_OK;
}

esp_err_t ph_ec_controller_set_ec_params(const ec_control_params_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_ec_params, params, sizeof(ec_control_params_t));

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "EC params updated (target: %.2f)", params->target_ec);
    return ESP_OK;
}

esp_err_t ph_ec_controller_correct_ph(float current_ph)
{
    if (!g_ph_auto_mode) {
        return ESP_OK; // Автоматический режим выключен
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    float target_ph = g_ph_params.target_ph;
    
    // Определяем какой насос использовать
    pump_index_t pump_idx;
    if (current_ph > target_ph) {
        pump_idx = PUMP_INDEX_PH_DOWN; // pH выше целевого, нужно понизить
    } else {
        pump_idx = PUMP_INDEX_PH_UP; // pH ниже целевого, нужно повысить
    }

    xSemaphoreGive(g_mutex);

    // АДАПТИВНАЯ PID коррекция с предсказанием и обучением
    ESP_LOGD(TAG, "Адаптивная коррекция pH: текущ=%.2f цель=%.2f насос=%s",
             current_ph, target_ph, PUMP_NAMES[pump_idx]);
    
    return pump_manager_compute_and_execute_adaptive(pump_idx, current_ph, target_ph);
}

esp_err_t ph_ec_controller_correct_ec(float current_ec)
{
    if (!g_ec_auto_mode) {
        return ESP_OK; // Автоматический режим выключен
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    float target_ec = g_ec_params.target_ec;
    float error = target_ec - current_ec;

    xSemaphoreGive(g_mutex);

    if (error < 0) {
        // EC выше целевого, нужно разбавить водой
        ESP_LOGD(TAG, "Адаптивная коррекция EC водой: текущ=%.2f цель=%.2f",
                 current_ec, target_ec);
        
        return pump_manager_compute_and_execute_adaptive(PUMP_INDEX_WATER, current_ec, target_ec);
    } else {
        // EC ниже целевого, нужно добавить питательные вещества
        // Запускаем все три насоса EC последовательно через адаптивный PID
        ESP_LOGD(TAG, "Адаптивная коррекция EC питательными веществами: текущ=%.2f цель=%.2f",
                 current_ec, target_ec);

        esp_err_t result = ESP_OK;
        
        // EC A
        esp_err_t err_a = pump_manager_compute_and_execute_adaptive(PUMP_INDEX_EC_A, current_ec, target_ec);
        if (err_a != ESP_OK) result = err_a;
        vTaskDelay(pdMS_TO_TICKS(500)); // Небольшая пауза между насосами
        
        // EC B
        esp_err_t err_b = pump_manager_compute_and_execute_adaptive(PUMP_INDEX_EC_B, current_ec, target_ec);
        if (err_b != ESP_OK) result = err_b;
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // EC C
        esp_err_t err_c = pump_manager_compute_and_execute_adaptive(PUMP_INDEX_EC_C, current_ec, target_ec);
        if (err_c != ESP_OK) result = err_c;

        return result;
    }
}

esp_err_t ph_ec_controller_set_auto_mode(bool ph_auto, bool ec_auto)
{
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_ph_auto_mode = ph_auto;
    g_ec_auto_mode = ec_auto;

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Auto mode: pH=%s, EC=%s",
             ph_auto ? "ON" : "OFF",
             ec_auto ? "ON" : "OFF");

    return ESP_OK;
}

const char* ph_ec_controller_get_pump_name(pump_index_t pump_idx)
{
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return "Unknown";
    }
    return PUMP_NAMES[pump_idx];
}

// Установка callback для насосов
esp_err_t ph_ec_controller_set_pump_callback(ph_ec_pump_callback_t callback)
{
    g_pump_callback = callback;
    ESP_LOGI(TAG, "Pump callback set");
    return ESP_OK;
}

// Установка callback для коррекции
esp_err_t ph_ec_controller_set_correction_callback(ph_ec_correction_callback_t callback)
{
    g_correction_callback = callback;
    ESP_LOGI(TAG, "Correction callback set");
    return ESP_OK;
}

// Обработка контроллера pH/EC
esp_err_t ph_ec_controller_process(void)
{
    // В данной реализации обработка не требуется
    return ESP_OK;
}

// Обновление значений датчиков
esp_err_t ph_ec_controller_update_values(float ph_value, float ec_value)
{
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_current_ph = ph_value;
    g_current_ec = ec_value;

    xSemaphoreGive(g_mutex);

    ESP_LOGD(TAG, "Values updated: pH=%.2f, EC=%.2f", ph_value, ec_value);
    return ESP_OK;
}

