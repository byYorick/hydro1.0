/**
 * @file ai_controller.h
 * @brief AI контроллер для автоматической коррекции pH/EC
 *
 * Использует TensorFlow Lite Micro для прогнозирования оптимальных дозировок
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Состояние системы для AI
 */
typedef struct {
    float current_ph;        ///< Текущий pH
    float current_ec;        ///< Текущий EC
    float target_ph;         ///< Целевой pH
    float target_ec;         ///< Целевой EC
    float temperature;       ///< Температура воды
    uint32_t time_since_last_correction; ///< Время с последней коррекции (сек)
} ai_system_state_t;

/**
 * @brief Рекомендуемые дозировки от AI
 */
typedef struct {
    float ph_up_ml;          ///< pH UP (мл)
    float ph_down_ml;        ///< pH DOWN (мл)
    float ec_a_ml;           ///< EC A (мл)
    float ec_b_ml;           ///< EC B (мл)
    float ec_c_ml;           ///< EC C (мл)
    float confidence;        ///< Уверенность модели (0.0-1.0)
} ai_dosage_prediction_t;

/**
 * @brief Конфигурация AI контроллера
 */
typedef struct {
    bool enabled;            ///< Включен ли AI
    float min_confidence;    ///< Минимальная уверенность для применения (0.0-1.0)
    uint32_t min_interval;   ///< Минимальный интервал между коррекциями (сек)
} ai_controller_config_t;

/**
 * @brief Инициализация AI контроллера
 * 
 * @param config Конфигурация
 * @return ESP_OK при успехе
 */
esp_err_t ai_controller_init(const ai_controller_config_t *config);

/**
 * @brief Деинициализация AI контроллера
 * 
 * @return ESP_OK при успехе
 */
esp_err_t ai_controller_deinit(void);

/**
 * @brief Прогнозирование коррекции
 * 
 * @param state Текущее состояние системы
 * @param prediction Выходные данные - рекомендуемые дозировки
 * @return ESP_OK при успехе
 */
esp_err_t ai_predict_correction(const ai_system_state_t *state, ai_dosage_prediction_t *prediction);

/**
 * @brief Простая эвристическая коррекция (без AI модели)
 * 
 * Использует PID-подобную логику как fallback
 * 
 * @param state Текущее состояние
 * @param prediction Выходные данные
 * @return ESP_OK при успехе
 */
esp_err_t ai_heuristic_correction(const ai_system_state_t *state, ai_dosage_prediction_t *prediction);

/**
 * @brief Анализ трендов на основе истории
 * 
 * @param history Массив исторических состояний
 * @param count Количество записей
 * @param trend_up true если тренд вверх
 * @return ESP_OK при успехе
 */
esp_err_t ai_evaluate_trend(const ai_system_state_t *history, size_t count, bool *trend_up);

/**
 * @brief Проверка доступности AI модели
 * 
 * @return true если модель загружена
 */
bool ai_is_model_loaded(void);

/**
 * @brief Получение статистики AI
 * 
 * @param buffer Буфер для текста
 * @param max_len Максимальная длина
 * @return ESP_OK при успехе
 */
esp_err_t ai_get_stats(char *buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // AI_CONTROLLER_H

