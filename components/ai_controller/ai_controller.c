/**
 * @file ai_controller.c
 * @brief Реализация AI контроллера для коррекции pH/EC
 *
 * Примечание: TensorFlow Lite модель пока не обучена.
 * Используется эвристический PID-подобный алгоритм как временное решение.
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "ai_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "AI_CONTROLLER";

// Глобальные переменные
static bool ai_initialized = false;
static bool model_loaded = false;
static SemaphoreHandle_t ai_mutex = NULL;
static ai_controller_config_t ai_config = {0};

// Статистика
static uint32_t predictions_count = 0;
static uint32_t corrections_applied = 0;

// PID параметры для эвристического алгоритма
#define PH_KP 0.5f
#define PH_KI 0.1f
#define PH_KD 0.05f

#define EC_KP 0.3f
#define EC_KI 0.05f
#define EC_KD 0.02f

/**
 * @brief Инициализация AI контроллера
 */
esp_err_t ai_controller_init(const ai_controller_config_t *config) {
    if (ai_initialized) {
        ESP_LOGW(TAG, "AI контроллер уже инициализирован");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Конфигурация AI не указана");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Создание мьютекса
    if (ai_mutex == NULL) {
        ai_mutex = xSemaphoreCreateMutex();
        if (ai_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(ai_mutex, portMAX_DELAY);
    
    // Сохранение конфигурации
    memcpy(&ai_config, config, sizeof(ai_controller_config_t));
    
    // TODO: Загрузка TensorFlow Lite модели
    // На данный момент используем эвристический алгоритм
    model_loaded = false;
    
    ai_initialized = true;
    
    ESP_LOGI(TAG, "AI контроллер инициализирован (эвристический режим)");
    ESP_LOGW(TAG, "TensorFlow Lite модель не загружена, используется PID-алгоритм");
    
    xSemaphoreGive(ai_mutex);
    return ESP_OK;
}

/**
 * @brief Деинициализация AI контроллера
 */
esp_err_t ai_controller_deinit(void) {
    if (!ai_initialized) {
        return ESP_OK;
    }
    
    xSemaphoreTake(ai_mutex, portMAX_DELAY);
    
    // TODO: Выгрузка TFLite модели
    model_loaded = false;
    ai_initialized = false;
    
    ESP_LOGI(TAG, "AI контроллер деинициализирован");
    
    xSemaphoreGive(ai_mutex);
    
    if (ai_mutex != NULL) {
        vSemaphoreDelete(ai_mutex);
        ai_mutex = NULL;
    }
    
    return ESP_OK;
}

/**
 * @brief Простая эвристическая коррекция (PID-подобная)
 */
esp_err_t ai_heuristic_correction(const ai_system_state_t *state, ai_dosage_prediction_t *prediction) {
    if (state == NULL || prediction == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Инициализация результата
    memset(prediction, 0, sizeof(ai_dosage_prediction_t));
    
    // Рассчитываем ошибки
    float ph_error = state->target_ph - state->current_ph;
    float ec_error = state->target_ec - state->current_ec;
    
    // ========================================================================
    // pH коррекция
    // ========================================================================
    if (fabs(ph_error) > 0.1f) {  // Допуск ±0.1 pH
        // Простой пропорциональный расчет
        float ph_correction = ph_error * PH_KP;
        
        // Учитываем температуру (pH меняется с температурой)
        float temp_factor = 1.0f + (state->temperature - 25.0f) * 0.01f;
        ph_correction *= temp_factor;
        
        if (ph_error > 0) {
            // Нужно повысить pH
            prediction->ph_up_ml = fmin(fabs(ph_correction) * 10.0f, 50.0f);  // Макс 50мл
        } else {
            // Нужно понизить pH
            prediction->ph_down_ml = fmin(fabs(ph_correction) * 10.0f, 50.0f);
        }
    }
    
    // ========================================================================
    // EC коррекция
    // ========================================================================
    if (fabs(ec_error) > 0.1f) {  // Допуск ±0.1 mS/cm
        if (ec_error > 0) {
            // Нужно повысить EC - добавляем питательные вещества
            float ec_correction = ec_error * EC_KP;
            
            // Распределяем между компонентами A, B, C
            prediction->ec_a_ml = fmin(ec_correction * 10.0f * 0.4f, 30.0f);  // 40% на A
            prediction->ec_b_ml = fmin(ec_correction * 10.0f * 0.4f, 30.0f);  // 40% на B
            prediction->ec_c_ml = fmin(ec_correction * 10.0f * 0.2f, 15.0f);  // 20% на C
        } else {
            // EC слишком высокая - нужно разбавить водой
            // Это требует отдельного механизма
            ESP_LOGW(TAG, "EC слишком высокая (%.2f > %.2f), требуется разбавление водой",
                     state->current_ec, state->target_ec);
        }
    }
    
    // Уверенность для эвристического алгоритма
    prediction->confidence = 0.7f;  // Средняя уверенность
    
    ESP_LOGD(TAG, "Эвристическая коррекция: pH_UP=%.1fмл, pH_DOWN=%.1fмл, EC_A=%.1fмл, EC_B=%.1fмл, EC_C=%.1fмл",
             prediction->ph_up_ml, prediction->ph_down_ml,
             prediction->ec_a_ml, prediction->ec_b_ml, prediction->ec_c_ml);
    
    return ESP_OK;
}

/**
 * @brief Прогнозирование коррекции
 */
esp_err_t ai_predict_correction(const ai_system_state_t *state, ai_dosage_prediction_t *prediction) {
    if (!ai_initialized) {
        ESP_LOGE(TAG, "AI контроллер не инициализирован");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (state == NULL || prediction == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(ai_mutex, portMAX_DELAY);
    
    esp_err_t ret = ESP_OK;
    
    // Проверяем минимальный интервал
    if (state->time_since_last_correction < ai_config.min_interval) {
        ESP_LOGD(TAG, "Слишком рано для коррекции (%d < %d сек)",
                 state->time_since_last_correction, ai_config.min_interval);
        memset(prediction, 0, sizeof(ai_dosage_prediction_t));
        goto cleanup;
    }
    
    if (model_loaded) {
        // TODO: Использовать TensorFlow Lite модель
        ESP_LOGW(TAG, "TFLite модель пока не реализована");
        ret = ai_heuristic_correction(state, prediction);
    } else {
        // Fallback на эвристический алгоритм
        ret = ai_heuristic_correction(state, prediction);
    }
    
    predictions_count++;
    
    // Проверяем минимальную уверенность
    if (prediction->confidence < ai_config.min_confidence) {
        ESP_LOGW(TAG, "Уверенность слишком низкая (%.2f < %.2f), коррекция отменена",
                 prediction->confidence, ai_config.min_confidence);
        memset(prediction, 0, sizeof(ai_dosage_prediction_t));
    } else {
        corrections_applied++;
    }
    
cleanup:
    xSemaphoreGive(ai_mutex);
    return ret;
}

/**
 * @brief Анализ трендов
 */
esp_err_t ai_evaluate_trend(const ai_system_state_t *history, size_t count, bool *trend_up) {
    if (history == NULL || trend_up == NULL || count < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Простой анализ: сравниваем первое и последнее значения
    float first_ph = history[0].current_ph;
    float last_ph = history[count - 1].current_ph;
    
    *trend_up = (last_ph > first_ph);
    
    ESP_LOGD(TAG, "Тренд pH: %s (%.2f -> %.2f)",
             *trend_up ? "ВВЕРХ" : "ВНИЗ", first_ph, last_ph);
    
    return ESP_OK;
}

/**
 * @brief Проверка доступности модели
 */
bool ai_is_model_loaded(void) {
    return model_loaded;
}

/**
 * @brief Получение статистики
 */
esp_err_t ai_get_stats(char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(buffer, max_len,
             "AI Controller Stats:\n"
             "- Initialized: %s\n"
             "- Model loaded: %s\n"
             "- Predictions: %lu\n"
             "- Corrections applied: %lu\n"
             "- Success rate: %.1f%%\n",
             ai_initialized ? "Yes" : "No",
             model_loaded ? "Yes" : "No (using heuristic)",
             (unsigned long)predictions_count,
             (unsigned long)corrections_applied,
             predictions_count > 0 ? (corrections_applied * 100.0f / predictions_count) : 0.0f);
    
    return ESP_OK;
}

