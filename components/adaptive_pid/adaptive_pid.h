/**
 * @file adaptive_pid.h
 * @brief Интеллектуальная адаптивная PID система
 * 
 * Компонент обеспечивает:
 * - Самообучение характеристик раствора (буферная емкость)
 * - Предсказание трендов на 1-3 часа вперед
 * - Адаптивную корректировку PID коэффициентов
 * - Упреждающую коррекцию до выхода за пороги
 * 
 * @author Hydro Team
 * @date 2025-10-15
 */

#ifndef ADAPTIVE_PID_H
#define ADAPTIVE_PID_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * КОНСТАНТЫ
 ******************************************************************************/

#define ADAPTIVE_HISTORY_SIZE           50      // Размер истории (оптимизировано для памяти)
#define MIN_CORRECTIONS_FOR_LEARNING    10      // Минимум коррекций для обучения
#define MIN_CONFIDENCE_FOR_PREDICTION   0.75f   // Минимальная уверенность для действий
#define MAX_PREDICTED_DEVIATION         1.5f    // Макс отклонение прогноза
#define MAX_PREEMPTIVE_PER_DAY          10      // Макс упреждающих коррекций в сутки

/*******************************************************************************
 * СТРУКТУРЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Состояние адаптивного PID для одного насоса
 */
typedef struct {
    // История измерений (ring buffer)
    float history[ADAPTIVE_HISTORY_SIZE];      // История значений
    uint32_t timestamps[ADAPTIVE_HISTORY_SIZE]; // Временные метки (секунды)
    uint8_t history_index;                      // Текущий индекс в ring buffer
    uint8_t history_count;                      // Количество записей в истории
    
    // Характеристики раствора (самообучение)
    float buffer_capacity;                      // мл насоса для изменения на 0.1 единицы
    float response_time_sec;                    // Время отклика раствора (секунды)
    bool buffer_capacity_learned;               // Флаг завершения обучения
    
    // Статистика коррекций
    uint32_t total_corrections;                 // Общее количество коррекций
    uint32_t successful_corrections;            // Успешных коррекций
    float avg_correction_volume_ml;             // Средний объем коррекции
    float effectiveness_ratio;                  // Эффективность (достигнутое/запланированное)
    
    // Предсказание трендов
    float predicted_value_1h;                   // Прогноз через 1 час
    float predicted_value_3h;                   // Прогноз через 3 часа
    float trend_slope;                          // Скорость изменения (единиц/час)
    bool trend_is_stable;                       // Стабилен ли тренд
    float prediction_confidence;                // Уверенность прогноза (0.0-1.0)
    
    // Адаптивные коэффициенты PID
    float kp_adaptive;                          // Адаптивный Kp
    float ki_adaptive;                          // Адаптивный Ki
    float kd_adaptive;                          // Адаптивный Kd
    uint64_t last_adaptation_time;              // Время последней адаптации
    
    // Упреждающая коррекция
    uint32_t preemptive_corrections_today;      // Счетчик упреждающих коррекций за сутки
    uint64_t last_preemptive_time;              // Время последней упреждающей коррекции
    uint64_t daily_reset_time;                  // Время последнего сброса суточного счетчика
    
    // Режимы работы
    bool auto_tuning_enabled;                   // Автонастройка PID
    bool prediction_enabled;                    // Предсказание трендов
    bool learning_mode;                         // Режим обучения характеристик
    bool adaptive_mode;                         // Адаптивные коэффициенты
    
    // Безопасность
    bool safe_mode;                             // Безопасный режим (откат к базовому PID)
    uint32_t failed_predictions;                // Количество неудачных прогнозов
    uint64_t prediction_cooldown_until;         // Время до которого предсказания отключены
} adaptive_pid_state_t;

/**
 * @brief Результат предсказания тренда
 */
typedef struct {
    float current_value;                        // Текущее значение
    float target_value;                         // Целевое значение
    float predicted_value_1h;                   // Прогноз через 1 час
    float predicted_value_3h;                   // Прогноз через 3 часа
    float correction_needed_ml;                 // Предсказание нужной коррекции (мл)
    uint32_t time_to_threshold_sec;             // Секунд до выхода за порог
    bool needs_preemptive_correction;           // Нужна ли упреждающая коррекция
    float confidence;                           // Уверенность прогноза (0.0-1.0)
    const char *recommendation;                 // Рекомендация системы
    const char *trend_description;              // Описание тренда ("растет", "падает", "стабильно")
} prediction_result_t;

/**
 * @brief Данные обучения буферной емкости
 */
typedef struct {
    float value_before;                         // Значение до коррекции
    float value_after;                          // Значение после коррекции
    float dose_ml;                              // Доза в мл
    uint32_t timestamp;                         // Время измерения
} learning_data_point_t;

/*******************************************************************************
 * API ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Инициализация адаптивной PID системы
 * 
 * Создает мьютексы, инициализирует структуры данных для всех 6 насосов,
 * загружает сохраненные данные обучения из NVS
 * 
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_init(void);

/**
 * @brief Обновление истории измерений
 * 
 * Добавляет новую точку данных в ring buffer истории.
 * Автоматически вычисляет тренд и обновляет прогнозы.
 * 
 * @param pump_idx Индекс насоса
 * @param value Новое значение параметра
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_update_history(pump_index_t pump_idx, float value);

/**
 * @brief Получение предсказания тренда
 * 
 * Анализирует историю и возвращает прогноз на 1-3 часа вперед.
 * Включает рекомендации по упреждающей коррекции.
 * 
 * @param pump_idx Индекс насоса
 * @param current Текущее значение
 * @param target Целевое значение
 * @param result Указатель на структуру для результата
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_predict(pump_index_t pump_idx, float current, float target, prediction_result_t *result);

/**
 * @brief Обучение буферной емкости раствора
 * 
 * Анализирует результат коррекции и обновляет буферную емкость.
 * Вызывается автоматически через 5 минут после коррекции.
 * 
 * @param pump_idx Индекс насоса
 * @param value_before Значение до коррекции
 * @param value_after Значение после коррекции (через 5 мин)
 * @param dose_ml Доза в мл
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_learn_buffer_capacity(pump_index_t pump_idx, 
                                              float value_before, 
                                              float value_after, 
                                              float dose_ml);

/**
 * @brief Получение адаптивных PID коэффициентов
 * 
 * Возвращает текущие адаптивные коэффициенты, рассчитанные на основе
 * характеристик раствора и эффективности предыдущих коррекций.
 * 
 * @param pump_idx Индекс насоса
 * @param kp Указатель для Kp
 * @param ki Указатель для Ki
 * @param kd Указатель для Kd
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_get_coefficients(pump_index_t pump_idx, float *kp, float *ki, float *kd);

/**
 * @brief Получение состояния для отображения в UI
 * 
 * @param pump_idx Индекс насоса
 * @return Указатель на структуру состояния (только для чтения!)
 */
const adaptive_pid_state_t* adaptive_pid_get_state(pump_index_t pump_idx);

/**
 * @brief Включение/выключение режима обучения
 * 
 * @param pump_idx Индекс насоса
 * @param enable true - включить, false - выключить
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_set_learning_mode(pump_index_t pump_idx, bool enable);

/**
 * @brief Включение/выключение предсказаний
 * 
 * @param pump_idx Индекс насоса
 * @param enable true - включить, false - выключить
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_set_prediction_mode(pump_index_t pump_idx, bool enable);

/**
 * @brief Включение/выключение адаптивных коэффициентов
 * 
 * @param pump_idx Индекс насоса
 * @param enable true - включить, false - выключить
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_set_adaptive_mode(pump_index_t pump_idx, bool enable);

/**
 * @brief Сброс обучения (очистка истории и статистики)
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_reset_learning(pump_index_t pump_idx);

/**
 * @brief Включение безопасного режима (откат к базовому PID)
 * 
 * Отключает все адаптивные функции, возвращает к фиксированным коэффициентам
 * 
 * @param pump_idx Индекс насоса
 * @param enable true - включить safe mode, false - вернуться к adaptive
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_set_safe_mode(pump_index_t pump_idx, bool enable);

/**
 * @brief Расчет оптимальной дозы на основе буферной емкости
 * 
 * Использует изученную буферную емкость для точного расчета дозы
 * 
 * @param pump_idx Индекс насоса
 * @param current Текущее значение
 * @param target Целевое значение
 * @param dose_ml Указатель для результата (мл)
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_calculate_dose(pump_index_t pump_idx, 
                                       float current, 
                                       float target, 
                                       float *dose_ml);

/**
 * @brief Обработка неудачного прогноза
 * 
 * Вызывается когда прогноз оказался неточным.
 * Снижает уверенность, отключает предсказания на время cooldown.
 * 
 * @param pump_idx Индекс насоса
 * @param predicted Что было предсказано
 * @param actual Что произошло на самом деле
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_handle_failed_prediction(pump_index_t pump_idx, 
                                                 float predicted, 
                                                 float actual);

/**
 * @brief Сохранение состояния в NVS
 * 
 * Сохраняет буферную емкость, статистику и режимы работы
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_save_to_nvs(pump_index_t pump_idx);

/**
 * @brief Загрузка состояния из NVS
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе, ESP_ERR_NOT_FOUND если нет данных
 */
esp_err_t adaptive_pid_load_from_nvs(pump_index_t pump_idx);

/**
 * @brief Периодическая обработка (вызывается каждую минуту)
 * 
 * Выполняет:
 * - Сброс суточных счетчиков в полночь
 * - Адаптацию коэффициентов по времени суток (опционально)
 * - Проверку cooldown для предсказаний
 * 
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_process(void);

/**
 * @brief Получение статистики для UI
 * 
 * @param pump_idx Индекс насоса
 * @param total_corrections Общее количество коррекций (может быть NULL)
 * @param effectiveness Эффективность 0.0-1.0 (может быть NULL)
 * @param confidence Уверенность обучения 0.0-1.0 (может быть NULL)
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_get_stats(pump_index_t pump_idx, 
                                  uint32_t *total_corrections,
                                  float *effectiveness,
                                  float *confidence);

/*******************************************************************************
 * NVS ХРАНЕНИЕ
 ******************************************************************************/

/**
 * @brief Сохранение выученных параметров в NVS
 * 
 * Сохраняет:
 * - Буферную емкость
 * - Адаптивные коэффициенты
 * - Статистику обучения
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_save_to_nvs(pump_index_t pump_idx);

/**
 * @brief Загрузка выученных параметров из NVS
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе, ESP_ERR_NOT_FOUND если данных нет
 */
esp_err_t adaptive_pid_load_from_nvs(pump_index_t pump_idx);

/**
 * @brief Сохранение всех адаптивных параметров (6 насосов)
 * 
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_save_all(void);

/**
 * @brief Загрузка всех адаптивных параметров (6 насосов)
 * 
 * @return ESP_OK при успехе
 */
esp_err_t adaptive_pid_load_all(void);

#ifdef __cplusplus
}
#endif

#endif // ADAPTIVE_PID_H

