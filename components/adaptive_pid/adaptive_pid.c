/**
 * @file adaptive_pid.c
 * @brief Реализация интеллектуальной адаптивной PID системы
 */

#include "adaptive_pid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "notification_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <math.h>
#include <time.h>

static const char *TAG = "ADAPTIVE_PID";

// Глобальные данные для всех 6 насосов
static adaptive_pid_state_t *g_states = NULL;  // ПЕРЕНЕСЕНО НА PSRAM для экономии 3 KB DRAM
static SemaphoreHandle_t g_mutexes[PUMP_INDEX_COUNT];
static bool g_initialized = false;

// Базовые коэффициенты PID (из конфигурации)
static float g_base_kp[PUMP_INDEX_COUNT];
static float g_base_ki[PUMP_INDEX_COUNT];
static float g_base_kd[PUMP_INDEX_COUNT];

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Получить текущее время в секундах
 */
static inline uint32_t get_time_sec(void) {
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

/**
 * @brief Добавить точку в ring buffer истории
 */
static void add_to_history(adaptive_pid_state_t *state, float value) {
    state->history[state->history_index] = value;
    state->timestamps[state->history_index] = get_time_sec();
    
    state->history_index = (state->history_index + 1) % ADAPTIVE_HISTORY_SIZE;
    
    if (state->history_count < ADAPTIVE_HISTORY_SIZE) {
        state->history_count++;
    }
}

/**
 * @brief Линейная регрессия для предсказания тренда
 * 
 * Метод наименьших квадратов: y = ax + b
 * 
 * @param state Состояние адаптивного PID
 * @param slope Указатель для наклона (a)
 * @param intercept Указатель для пересечения (b)
 * @param r_squared Указатель для R² (может быть NULL)
 * @return true если успешно
 */
static bool calculate_linear_regression(const adaptive_pid_state_t *state, 
                                         float *slope, 
                                         float *intercept,
                                         float *r_squared) {
    if (state->history_count < 5) {
        return false; // Недостаточно данных
    }
    
    uint8_t n = state->history_count;
    
    // Нормализация времени (используем относительное время в часах)
    uint32_t t0 = state->timestamps[0];
    
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
    
    for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = i;
        if (state->history_index > 0 && i >= state->history_index) {
            // Ring buffer wraparound
            idx = (state->history_index + i) % ADAPTIVE_HISTORY_SIZE;
        }
        
        float x = (float)(state->timestamps[idx] - t0) / 3600.0f; // Часы
        float y = state->history[idx];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }
    
    // Расчет коэффициентов
    float denom = (n * sum_x2) - (sum_x * sum_x);
    if (fabsf(denom) < 0.0001f) {
        return false; // Деление на ноль
    }
    
    *slope = ((n * sum_xy) - (sum_x * sum_y)) / denom;
    *intercept = (sum_y - (*slope * sum_x)) / n;
    
    // Расчет R² (коэффициент детерминации)
    if (r_squared != NULL) {
        float mean_y = sum_y / n;
        float ss_tot = sum_y2 - (n * mean_y * mean_y);
        float ss_res = 0;
        
        for (uint8_t i = 0; i < n; i++) {
            uint8_t idx = i;
            if (state->history_index > 0 && i >= state->history_index) {
                idx = (state->history_index + i) % ADAPTIVE_HISTORY_SIZE;
            }
            
            float x = (float)(state->timestamps[idx] - t0) / 3600.0f;
            float y = state->history[idx];
            float y_pred = (*slope * x) + (*intercept);
            ss_res += (y - y_pred) * (y - y_pred);
        }
        
        *r_squared = (ss_tot > 0.001f) ? (1.0f - (ss_res / ss_tot)) : 0.0f;
        if (*r_squared < 0.0f) *r_squared = 0.0f;
        if (*r_squared > 1.0f) *r_squared = 1.0f;
    }
    
    return true;
}

/**
 * @brief Адаптация коэффициентов на основе буферной емкости
 */
static void adapt_coefficients(adaptive_pid_state_t *state, pump_index_t pump_idx) {
    if (!state->adaptive_mode || !state->buffer_capacity_learned) {
        // Использовать базовые коэффициенты
        state->kp_adaptive = g_base_kp[pump_idx];
        state->ki_adaptive = g_base_ki[pump_idx];
        state->kd_adaptive = g_base_kd[pump_idx];
        return;
    }
    
    // Адаптация на основе буферной емкости
    // Если буферная емкость высокая (раствор устойчивый) → увеличить агрессивность
    // Если низкая (раствор чувствительный) → уменьшить агрессивность
    
    float adaptation_factor = 1.0f;
    
    // Определение чувствительности раствора
    if (state->buffer_capacity > 5.0f) {
        // Высокая буферная емкость → раствор устойчивый
        adaptation_factor = 1.2f; // +20% агрессивности
    } else if (state->buffer_capacity < 2.0f) {
        // Низкая буферная емкость → раствор чувствительный
        adaptation_factor = 0.8f; // -20% агрессивности
    }
    
    // Адаптация на основе эффективности
    if (state->effectiveness_ratio > 0.9f) {
        // Очень эффективная система → можно быть консервативнее
        adaptation_factor *= 0.95f;
    } else if (state->effectiveness_ratio < 0.7f) {
        // Низкая эффективность → нужно быть агрессивнее
        adaptation_factor *= 1.1f;
    }
    
    // Применение адаптации
    state->kp_adaptive = g_base_kp[pump_idx] * adaptation_factor;
    state->ki_adaptive = g_base_ki[pump_idx] * adaptation_factor;
    state->kd_adaptive = g_base_kd[pump_idx] * adaptation_factor;
    
    ESP_LOGD(TAG, "Насос %d: адаптация коэф. factor=%.2f Kp=%.2f Ki=%.2f Kd=%.2f",
             pump_idx, adaptation_factor, 
             state->kp_adaptive, state->ki_adaptive, state->kd_adaptive);
}

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

esp_err_t adaptive_pid_init(void) {
    if (g_initialized) {
        ESP_LOGW(TAG, "adaptive_pid уже инициализирован");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Инициализация adaptive_pid...");
    
    // Выделяем память для состояний в PSRAM для экономии DRAM
    if (g_states == NULL) {
        g_states = heap_caps_calloc(
            PUMP_INDEX_COUNT,
            sizeof(adaptive_pid_state_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (g_states == NULL) {
            ESP_LOGE(TAG, "Failed to allocate adaptive PID states in PSRAM");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Adaptive PID states allocated in PSRAM: %d bytes (saved 3.0 KB DRAM)",
                 PUMP_INDEX_COUNT * sizeof(adaptive_pid_state_t));
    }
    
    // Создание мьютексов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        g_mutexes[i] = xSemaphoreCreateMutex();
        if (g_mutexes[i] == NULL) {
            ESP_LOGE(TAG, "Не удалось создать мьютекс для насоса %d", i);
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Инициализация состояний
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        memset(&g_states[i], 0, sizeof(adaptive_pid_state_t));
        
        // Базовые коэффициенты (будут загружены из конфигурации)
        g_base_kp[i] = 2.0f;
        g_base_ki[i] = 0.5f;
        g_base_kd[i] = 0.1f;
        
        // pH насосы (индексы 0, 1)
        if (i == PUMP_INDEX_PH_UP || i == PUMP_INDEX_PH_DOWN) {
            g_base_kp[i] = 2.0f;
            g_base_ki[i] = 0.5f;
            g_base_kd[i] = 0.1f;
        }
        // EC насосы (индексы 2, 3, 4)
        else if (i >= PUMP_INDEX_EC_A && i <= PUMP_INDEX_EC_C) {
            g_base_kp[i] = 1.5f;
            g_base_ki[i] = 0.3f;
            g_base_kd[i] = 0.05f;
        }
        // Water насос
        else if (i == PUMP_INDEX_WATER) {
            g_base_kp[i] = 1.0f;
            g_base_ki[i] = 0.2f;
            g_base_kd[i] = 0.0f;
        }
        
        // Инициализация адаптивных коэффициентов
        g_states[i].kp_adaptive = g_base_kp[i];
        g_states[i].ki_adaptive = g_base_ki[i];
        g_states[i].kd_adaptive = g_base_kd[i];
        
        // Режимы по умолчанию (умеренный режим)
        g_states[i].learning_mode = true;
        g_states[i].prediction_enabled = false; // Включить после обучения
        g_states[i].adaptive_mode = true;
        g_states[i].safe_mode = false;
        
        // Начальные значения
        g_states[i].buffer_capacity = 2.5f; // Примерное значение
        g_states[i].buffer_capacity_learned = false;
        g_states[i].effectiveness_ratio = 0.8f;
        g_states[i].prediction_confidence = 0.0f;
        g_states[i].daily_reset_time = get_time_sec();
        
        // Попытка загрузить из NVS
        esp_err_t err = adaptive_pid_load_from_nvs(i);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Насос %d: данные загружены из NVS", i);
        } else {
            ESP_LOGD(TAG, "Насос %d: используются значения по умолчанию", i);
        }
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "adaptive_pid инициализирован успешно");
    
    return ESP_OK;
}

esp_err_t adaptive_pid_update_history(pump_index_t pump_idx, float value) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    // Добавить в историю
    add_to_history(state, value);
    
    // Если достаточно данных - обновить тренд
    if (state->history_count >= 5) {
        float slope, intercept, r_squared;
        
        if (calculate_linear_regression(state, &slope, &intercept, &r_squared)) {
            state->trend_slope = slope;
            
            // Проверка стабильности тренда (R² > 0.7 = хороший fit)
            state->trend_is_stable = (r_squared > 0.7f);
            
            // Уверенность в прогнозе зависит от R²
            state->prediction_confidence = r_squared;
            
            // Вычисление прогнозов
            uint32_t current_time = get_time_sec();
            uint32_t t0 = state->timestamps[0];
            float time_hours = (float)(current_time - t0) / 3600.0f;
            
            // Прогноз на 1 час вперед
            float future_time_1h = time_hours + 1.0f;
            state->predicted_value_1h = (slope * future_time_1h) + intercept;
            
            // Прогноз на 3 часа вперед
            float future_time_3h = time_hours + 3.0f;
            state->predicted_value_3h = (slope * future_time_3h) + intercept;
            
            ESP_LOGD(TAG, "Насос %d: тренд slope=%.3f, R²=%.2f, прогноз_1h=%.2f",
                     pump_idx, slope, r_squared, state->predicted_value_1h);
        }
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_predict(pump_index_t pump_idx, float current, float target, prediction_result_t *result) {
    if (pump_idx >= PUMP_INDEX_COUNT || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    memset(result, 0, sizeof(prediction_result_t));
    
    // Заполнение базовой информации
    result->current_value = current;
    result->target_value = target;
    
    // Проверка режимов
    if (state->safe_mode || !state->prediction_enabled) {
        result->recommendation = "Предсказания отключены";
        result->trend_description = "Неизвестно";
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK;
    }
    
    // Проверка cooldown после неудачного прогноза
    uint32_t now = get_time_sec();
    if (now < state->prediction_cooldown_until) {
        result->recommendation = "Прогнозы временно отключены (cooldown)";
        result->trend_description = "Ожидание";
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK;
    }
    
    // Проверка достаточности данных
    if (state->history_count < 10) {
        result->recommendation = "Недостаточно данных для прогноза";
        result->trend_description = "Сбор данных...";
        result->confidence = 0.0f;
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK;
    }
    
    // Копирование прогнозов
    result->predicted_value_1h = state->predicted_value_1h;
    result->predicted_value_3h = state->predicted_value_3h;
    result->confidence = state->prediction_confidence;
    
    // Описание тренда
    if (fabsf(state->trend_slope) < 0.01f) {
        result->trend_description = "Стабильно";
    } else if (state->trend_slope > 0) {
        result->trend_description = "Растет";
    } else {
        result->trend_description = "Падает";
    }
    
    // Проверка: нужна ли упреждающая коррекция?
    float error_current = fabsf(current - target);
    float error_predicted_1h = fabsf(result->predicted_value_1h - target);
    
    // Если прогноз показывает ухудшение
    if (error_predicted_1h > error_current * 1.5f && 
        result->confidence > MIN_CONFIDENCE_FOR_PREDICTION) {
        
        // Проверка лимита упреждающих коррекций
        if (state->preemptive_corrections_today < MAX_PREEMPTIVE_PER_DAY) {
            result->needs_preemptive_correction = true;
            
            // Расчет времени до выхода за порог
            if (fabsf(state->trend_slope) > 0.001f) {
                float threshold_distance = error_current;
                result->time_to_threshold_sec = (uint32_t)(threshold_distance / fabsf(state->trend_slope) * 3600.0f);
            } else {
                result->time_to_threshold_sec = 0;
            }
            
            // Расчет необходимой коррекции
            if (state->buffer_capacity_learned && state->buffer_capacity > 0.1f) {
                float correction_needed = error_predicted_1h * 10.0f; // Перевод в десятые доли
                result->correction_needed_ml = correction_needed * state->buffer_capacity;
                
                char rec_buf[128];
                snprintf(rec_buf, sizeof(rec_buf), 
                        "Рекомендуется упреждающая коррекция %.1f мл", 
                        result->correction_needed_ml);
                result->recommendation = rec_buf; // ВНИМАНИЕ: временный указатель!
            } else {
                result->recommendation = "Упреждающая коррекция рекомендуется";
            }
        } else {
            result->needs_preemptive_correction = false;
            result->recommendation = "Лимит упреждающих коррекций достигнут";
        }
    } else {
        result->needs_preemptive_correction = false;
        result->recommendation = "Прогноз в пределах нормы";
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_learn_buffer_capacity(pump_index_t pump_idx, 
                                              float value_before, 
                                              float value_after, 
                                              float dose_ml) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    if (!state->learning_mode) {
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK; // Обучение отключено
    }
    
    // Расчет изменения
    float value_change = fabsf(value_after - value_before);
    
    // Проверка значимости изменения (мин 0.01)
    if (value_change < 0.01f || dose_ml < 0.1f) {
        ESP_LOGD(TAG, "Насос %d: изменение слишком мало для обучения", pump_idx);
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK;
    }
    
    // Расчет буферной емкости: сколько мл нужно для изменения на 0.1 единицы
    float measured_capacity = dose_ml / (value_change * 10.0f);
    
    // Фильтрация аномальных значений
    if (measured_capacity < 0.1f || measured_capacity > 50.0f) {
        ESP_LOGW(TAG, "Насос %d: аномальная буферная емкость %.2f, игнорируем", 
                 pump_idx, measured_capacity);
        xSemaphoreGive(g_mutexes[pump_idx]);
        return ESP_OK;
    }
    
    // Скользящее среднее для сглаживания
    if (!state->buffer_capacity_learned) {
        // Первое измерение
        state->buffer_capacity = measured_capacity;
        state->buffer_capacity_learned = true;
        
        // ЭТАП 10: Сохраняем в NVS после первого обучения
        xSemaphoreGive(g_mutexes[pump_idx]);  // Временно освобождаем для NVS операции
        adaptive_pid_save_to_nvs(pump_idx);
        if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    } else {
        // Экспоненциальное сглаживание (alpha = 0.3)
        state->buffer_capacity = state->buffer_capacity * 0.7f + measured_capacity * 0.3f;
    }
    
    state->total_corrections++;
    
    ESP_LOGD(TAG, "Насос %d: обучение - изменение=%.2f, доза=%.1fмл, емкость=%.2fмл/0.1",
             pump_idx, value_change, dose_ml, state->buffer_capacity);
    
    // ЭТАП 10: Периодическое сохранение каждые 10 коррекций
    if (state->total_corrections % 10 == 0) {
        xSemaphoreGive(g_mutexes[pump_idx]);  // Временно освобождаем для NVS
        adaptive_pid_save_to_nvs(pump_idx);
        if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Адаптация коэффициентов на основе новых данных
    adapt_coefficients(state, pump_idx);
    
    // Сохранение каждые 5 коррекций
    if (state->total_corrections % 5 == 0) {
        adaptive_pid_save_to_nvs(pump_idx);
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_get_coefficients(pump_index_t pump_idx, float *kp, float *ki, float *kd) {
    if (pump_idx >= PUMP_INDEX_COUNT || kp == NULL || ki == NULL || kd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    if (state->safe_mode) {
        // Безопасный режим - возврат к базовым коэффициентам
        *kp = g_base_kp[pump_idx];
        *ki = g_base_ki[pump_idx];
        *kd = g_base_kd[pump_idx];
    } else {
        // Адаптивные коэффициенты
        *kp = state->kp_adaptive;
        *ki = state->ki_adaptive;
        *kd = state->kd_adaptive;
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

const adaptive_pid_state_t* adaptive_pid_get_state(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return NULL;
    }
    
    return &g_states[pump_idx]; // Только для чтения!
}

esp_err_t adaptive_pid_set_learning_mode(pump_index_t pump_idx, bool enable) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_states[pump_idx].learning_mode = enable;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Насос %d: режим обучения %s", pump_idx, enable ? "ВКЛ" : "ВЫКЛ");
    
    return ESP_OK;
}

esp_err_t adaptive_pid_set_prediction_mode(pump_index_t pump_idx, bool enable) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_states[pump_idx].prediction_enabled = enable;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Насос %d: предсказания %s", pump_idx, enable ? "ВКЛ" : "ВЫКЛ");
    
    return ESP_OK;
}

esp_err_t adaptive_pid_set_adaptive_mode(pump_index_t pump_idx, bool enable) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_states[pump_idx].adaptive_mode = enable;
    
    // Пересчитать коэффициенты
    adapt_coefficients(&g_states[pump_idx], pump_idx);
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Насос %d: адаптивные коэффициенты %s", pump_idx, enable ? "ВКЛ" : "ВЫКЛ");
    
    return ESP_OK;
}

esp_err_t adaptive_pid_reset_learning(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    // Очистка истории
    memset(state->history, 0, sizeof(state->history));
    memset(state->timestamps, 0, sizeof(state->timestamps));
    state->history_index = 0;
    state->history_count = 0;
    
    // Сброс обучения
    state->buffer_capacity = 2.5f;
    state->buffer_capacity_learned = false;
    state->total_corrections = 0;
    state->successful_corrections = 0;
    state->effectiveness_ratio = 0.8f;
    state->prediction_confidence = 0.0f;
    
    // Сброс трендов
    state->predicted_value_1h = 0;
    state->predicted_value_3h = 0;
    state->trend_slope = 0;
    state->trend_is_stable = false;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Насос %d: обучение сброшено", pump_idx);
    
    // Уведомление
    char msg[64];
    snprintf(msg, sizeof(msg), "Обучение для %s сброшено", PUMP_NAMES[pump_idx]);
    notification_create(NOTIF_TYPE_INFO, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SYSTEM, msg);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_set_safe_mode(pump_index_t pump_idx, bool enable) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_states[pump_idx].safe_mode = enable;
    
    if (enable) {
        // Отключить все адаптивные функции
        g_states[pump_idx].prediction_enabled = false;
        g_states[pump_idx].adaptive_mode = false;
        g_states[pump_idx].auto_tuning_enabled = false;
        
        ESP_LOGW(TAG, "Насос %d: БЕЗОПАСНЫЙ РЕЖИМ включен (базовый PID)", pump_idx);
        
        char msg[64];
        snprintf(msg, sizeof(msg), "Безопасный режим: %s", PUMP_NAMES[pump_idx]);
        notification_create(NOTIF_TYPE_WARNING, NOTIF_PRIORITY_HIGH, NOTIF_SOURCE_SYSTEM, msg);
    } else {
        ESP_LOGI(TAG, "Насос %d: возврат к адаптивному режиму", pump_idx);
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_calculate_dose(pump_index_t pump_idx, 
                                       float current, 
                                       float target, 
                                       float *dose_ml) {
    if (pump_idx >= PUMP_INDEX_COUNT || dose_ml == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    float error = fabsf(target - current);
    
    if (state->buffer_capacity_learned && state->buffer_capacity > 0.1f) {
        // Использовать изученную буферную емкость
        *dose_ml = error * 10.0f * state->buffer_capacity;
        
        ESP_LOGD(TAG, "Насос %d: расчет дозы по буферной емкости: %.2f мл", 
                 pump_idx, *dose_ml);
    } else {
        // Использовать примерное значение
        *dose_ml = error * 10.0f * 2.5f;
        
        ESP_LOGD(TAG, "Насос %d: расчет дозы по умолчанию: %.2f мл", 
                 pump_idx, *dose_ml);
    }
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_handle_failed_prediction(pump_index_t pump_idx, 
                                                 float predicted, 
                                                 float actual) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    float deviation = fabsf(predicted - actual);
    
    ESP_LOGW(TAG, "Насос %d: неудачный прогноз! Предсказано=%.2f Фактически=%.2f Откл=%.2f",
             pump_idx, predicted, actual, deviation);
    
    state->failed_predictions++;
    
    // Снижение уверенности
    state->prediction_confidence *= 0.8f;
    
    // Отключение прогнозов на 3 часа
    state->prediction_cooldown_until = get_time_sec() + (3 * 3600);
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    // Уведомление пользователя
    char msg[96];
    snprintf(msg, sizeof(msg), "Прогноз %s был неточен (откл=%.1f)", 
             PUMP_NAMES[pump_idx], deviation);
    notification_create(NOTIF_TYPE_WARNING, NOTIF_PRIORITY_NORMAL, NOTIF_SOURCE_SYSTEM, msg);
    
    return ESP_OK;
}

// ЭТАП 10: Структура для NVS хранения
#define ADAPTIVE_NVS_NAMESPACE "adaptive_pid"

typedef struct {
    float buffer_capacity;
    float response_time_sec;
    bool buffer_capacity_learned;
    uint32_t total_corrections;
    uint32_t successful_corrections;
    float effectiveness_ratio;
    float kp_adaptive;
    float ki_adaptive;
    float kd_adaptive;
} __attribute__((packed)) adaptive_pid_nvs_data_t;

esp_err_t adaptive_pid_save_to_nvs(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    adaptive_pid_nvs_data_t nvs_data = {
        .buffer_capacity = state->buffer_capacity,
        .response_time_sec = state->response_time_sec,
        .buffer_capacity_learned = state->buffer_capacity_learned,
        .total_corrections = state->total_corrections,
        .successful_corrections = state->successful_corrections,
        .effectiveness_ratio = state->effectiveness_ratio,
        .kp_adaptive = state->kp_adaptive,
        .ki_adaptive = state->ki_adaptive,
        .kd_adaptive = state->kd_adaptive,
    };
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ADAPTIVE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось открыть NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    char key[16];
    snprintf(key, sizeof(key), "pump%d", pump_idx);
    
    err = nvs_set_blob(nvs_handle, key, &nvs_data, sizeof(nvs_data));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Параметры %s сохранены в NVS (buffer: %.3f, corrections: %lu)", 
                 PUMP_NAMES[pump_idx], nvs_data.buffer_capacity, 
                 (unsigned long)nvs_data.total_corrections);
    }
    
    return err;
}

esp_err_t adaptive_pid_load_from_nvs(pump_index_t pump_idx) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ADAPTIVE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS не открыт (первый запуск): %s", esp_err_to_name(err));
        return err;
    }
    
    char key[16];
    snprintf(key, sizeof(key), "pump%d", pump_idx);
    
    adaptive_pid_nvs_data_t nvs_data;
    size_t size = sizeof(nvs_data);
    
    err = nvs_get_blob(nvs_handle, key, &nvs_data, &size);
    nvs_close(nvs_handle);
    
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Ошибка чтения из NVS для %s: %s", 
                     PUMP_NAMES[pump_idx], esp_err_to_name(err));
        }
        return err;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    state->buffer_capacity = nvs_data.buffer_capacity;
    state->response_time_sec = nvs_data.response_time_sec;
    state->buffer_capacity_learned = nvs_data.buffer_capacity_learned;
    state->total_corrections = nvs_data.total_corrections;
    state->successful_corrections = nvs_data.successful_corrections;
    state->effectiveness_ratio = nvs_data.effectiveness_ratio;
    state->kp_adaptive = nvs_data.kp_adaptive;
    state->ki_adaptive = nvs_data.ki_adaptive;
    state->kd_adaptive = nvs_data.kd_adaptive;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    ESP_LOGI(TAG, "Параметры %s загружены из NVS (buffer: %.3f, corrections: %lu)", 
             PUMP_NAMES[pump_idx], nvs_data.buffer_capacity, 
             (unsigned long)nvs_data.total_corrections);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_process(void) {
    time_t now_time;
    struct tm timeinfo;
    time(&now_time);
    localtime_r(&now_time, &timeinfo);
    
    // Проверка полуночи для сброса суточных счетчиков
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
        ESP_LOGI(TAG, "Полночь: сброс суточных счетчиков упреждающих коррекций");
        
        for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
            if (xSemaphoreTake(g_mutexes[i], pdMS_TO_TICKS(1000)) == pdTRUE) {
                g_states[i].preemptive_corrections_today = 0;
                g_states[i].daily_reset_time = get_time_sec();
                xSemaphoreGive(g_mutexes[i]);
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t adaptive_pid_get_stats(pump_index_t pump_idx, 
                                  uint32_t *total_corrections,
                                  float *effectiveness,
                                  float *confidence) {
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_mutexes[pump_idx], pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    adaptive_pid_state_t *state = &g_states[pump_idx];
    
    if (total_corrections) *total_corrections = state->total_corrections;
    if (effectiveness) *effectiveness = state->effectiveness_ratio;
    if (confidence) *confidence = state->prediction_confidence;
    
    xSemaphoreGive(g_mutexes[pump_idx]);
    
    return ESP_OK;
}

esp_err_t adaptive_pid_save_all(void) {
    ESP_LOGI(TAG, "Сохранение всех адаптивных параметров в NVS...");
    
    int saved = 0;
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        if (adaptive_pid_save_to_nvs(i) == ESP_OK) {
            saved++;
        }
    }
    
    ESP_LOGI(TAG, "Сохранено параметров для %d/%d насосов", saved, PUMP_INDEX_COUNT);
    
    return (saved == PUMP_INDEX_COUNT) ? ESP_OK : ESP_FAIL;
}

esp_err_t adaptive_pid_load_all(void) {
    ESP_LOGI(TAG, "Загрузка всех адаптивных параметров из NVS...");
    
    int loaded = 0;
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        if (adaptive_pid_load_from_nvs(i) == ESP_OK) {
            loaded++;
        }
    }
    
    ESP_LOGI(TAG, "Загружено параметров для %d/%d насосов", loaded, PUMP_INDEX_COUNT);
    
    return ESP_OK;  // Не ошибка, если нет сохраненных данных (первый запуск)
}

