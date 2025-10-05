#include "ph_ec_controller.h"
#include "peristaltic_pump.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <string.h>
#include <math.h>

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

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP",
    "pH DOWN",
    "EC A",
    "EC B",
    "EC C",
    "Water"
};

// Конфигурация GPIO для насосов
static const struct {
    int ia_pin;
    int ib_pin;
} PUMP_PINS[PUMP_INDEX_COUNT] = {
    {PUMP_PH_UP_IA, PUMP_PH_UP_IB},
    {PUMP_PH_DOWN_IA, PUMP_PH_DOWN_IB},
    {PUMP_EC_A_IA, PUMP_EC_A_IB},
    {PUMP_EC_B_IA, PUMP_EC_B_IB},
    {PUMP_EC_C_IA, PUMP_EC_C_IB},
    {PUMP_WATER_IA, PUMP_WATER_IB}
};

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
        pump_init(PUMP_PINS[i].ia_pin, PUMP_PINS[i].ib_pin);
        
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

    float error = current_ph - g_ph_params.target_ph;
    
    // Проверяем мертвую зону
    if (fabsf(error) < g_ph_params.deadband) {
        xSemaphoreGive(g_mutex);
        return ESP_OK; // В пределах нормы
    }

    // Определяем какой насос использовать
    pump_index_t pump_idx;
    if (error > 0) {
        pump_idx = PUMP_INDEX_PH_DOWN; // pH выше целевого, нужно понизить
    } else {
        pump_idx = PUMP_INDEX_PH_UP; // pH ниже целевого, нужно повысить
    }

    // Вычисляем длительность работы насоса
    float correction = fminf(fabsf(error), g_ph_params.max_correction_step);
    uint32_t duration_ms = (uint32_t)(correction * 1000.0f / 
                                      g_pump_configs[pump_idx].flow_rate_ml_per_sec);
    
    // Ограничиваем длительность
    duration_ms = fmaxf(duration_ms, g_pump_configs[pump_idx].min_duration_ms);
    duration_ms = fminf(duration_ms, g_pump_configs[pump_idx].max_duration_ms);

    xSemaphoreGive(g_mutex);

    // Запускаем насос
    ESP_LOGI(TAG, "Correcting pH: %.2f -> %.2f (pump: %s, duration: %lums)",
             current_ph, g_ph_params.target_ph, PUMP_NAMES[pump_idx], 
             (unsigned long)duration_ms);
    
    pump_run_ms(PUMP_PINS[pump_idx].ia_pin, PUMP_PINS[pump_idx].ib_pin, duration_ms);

    return ESP_OK;
}

esp_err_t ph_ec_controller_correct_ec(float current_ec)
{
    if (!g_ec_auto_mode) {
        return ESP_OK; // Автоматический режим выключен
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    float error = g_ec_params.target_ec - current_ec;

    // Проверяем мертвую зону
    if (fabsf(error) < g_ec_params.deadband) {
        xSemaphoreGive(g_mutex);
        return ESP_OK; // В пределах нормы
    }

    if (error < 0) {
        // EC выше целевого, нужно разбавить водой
        uint32_t duration_ms = (uint32_t)(fabsf(error) * 1000.0f / 
                                          g_pump_configs[PUMP_INDEX_WATER].flow_rate_ml_per_sec);
        duration_ms = fmaxf(duration_ms, g_pump_configs[PUMP_INDEX_WATER].min_duration_ms);
        duration_ms = fminf(duration_ms, g_pump_configs[PUMP_INDEX_WATER].max_duration_ms);

        xSemaphoreGive(g_mutex);

        ESP_LOGI(TAG, "Correcting EC with water: %.2f -> %.2f (duration: %lums)",
                 current_ec, g_ec_params.target_ec, (unsigned long)duration_ms);
        
        pump_run_ms(PUMP_PINS[PUMP_INDEX_WATER].ia_pin, 
                   PUMP_PINS[PUMP_INDEX_WATER].ib_pin, duration_ms);
    } else {
        // EC ниже целевого, нужно добавить питательные вещества
        float correction = fminf(error, g_ec_params.max_correction_step);
        
        // Вычисляем длительность для каждого компонента
        uint32_t duration_a = (uint32_t)(correction * g_ec_params.ratio_a * 1000.0f /
                                         g_pump_configs[PUMP_INDEX_EC_A].flow_rate_ml_per_sec);
        uint32_t duration_b = (uint32_t)(correction * g_ec_params.ratio_b * 1000.0f /
                                         g_pump_configs[PUMP_INDEX_EC_B].flow_rate_ml_per_sec);
        uint32_t duration_c = (uint32_t)(correction * g_ec_params.ratio_c * 1000.0f /
                                         g_pump_configs[PUMP_INDEX_EC_C].flow_rate_ml_per_sec);

        xSemaphoreGive(g_mutex);

        ESP_LOGI(TAG, "Correcting EC with nutrients: %.2f -> %.2f",
                 current_ec, g_ec_params.target_ec);

        // Запускаем насосы последовательно
        if (duration_a > 0) {
            pump_run_ms(PUMP_PINS[PUMP_INDEX_EC_A].ia_pin,
                       PUMP_PINS[PUMP_INDEX_EC_A].ib_pin, duration_a);
            vTaskDelay(pdMS_TO_TICKS(500)); // Небольшая пауза между насосами
        }
        if (duration_b > 0) {
            pump_run_ms(PUMP_PINS[PUMP_INDEX_EC_B].ia_pin,
                       PUMP_PINS[PUMP_INDEX_EC_B].ib_pin, duration_b);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (duration_c > 0) {
            pump_run_ms(PUMP_PINS[PUMP_INDEX_EC_C].ia_pin,
                       PUMP_PINS[PUMP_INDEX_EC_C].ib_pin, duration_c);
        }
    }

    return ESP_OK;
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

