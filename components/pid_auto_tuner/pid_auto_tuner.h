/**
 * @file pid_auto_tuner.h
 * @brief Автоматическая настройка PID контроллеров
 * 
 * Компонент обеспечивает автоматический подбор оптимальных PID коэффициентов
 * с использованием различных методов:
 * - Relay feedback (Ziegler-Nichols)
 * - Анализ переходной характеристики
 * - Адаптивная подстройка
 * 
 * @author Hydro Team
 * @date 2025-10-15
 */

#ifndef PID_AUTO_TUNER_H
#define PID_AUTO_TUNER_H

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

#define AUTO_TUNE_MAX_DURATION_SEC      1200    // Макс 20 минут
#define AUTO_TUNE_MIN_OSCILLATIONS      3       // Минимум осцилляций для анализа
#define AUTO_TUNE_RELAY_AMPLITUDE       0.3f    // Амплитуда relay (±0.3 pH/EC)

/*******************************************************************************
 * ТИПЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Методы автонастройки
 */
typedef enum {
    TUNING_METHOD_RELAY,            // Relay feedback (Ziegler-Nichols)
    TUNING_METHOD_STEP_RESPONSE,    // Анализ переходной характеристики
    TUNING_METHOD_ADAPTIVE          // Адаптивная подстройка
} tuning_method_t;

/**
 * @brief Состояние процесса автонастройки
 */
typedef enum {
    TUNING_STATE_IDLE,              // Не запущена
    TUNING_STATE_INIT,              // Инициализация
    TUNING_STATE_RELAY_TEST,        // Relay тест
    TUNING_STATE_ANALYZING,         // Анализ данных
    TUNING_STATE_CALCULATING,       // Расчет коэффициентов
    TUNING_STATE_COMPLETED,         // Завершено успешно
    TUNING_STATE_FAILED,            // Ошибка
    TUNING_STATE_CANCELLED          // Отменено пользователем
} tuning_state_t;

/**
 * @brief Результат автонастройки
 */
typedef struct {
    tuning_method_t method;             // Использованный метод
    tuning_state_t state;               // Состояние процесса
    
    // Найденные параметры
    float ultimate_gain;                // Ku (критическое усиление)
    float ultimate_period_sec;          // Tu (период колебаний)
    
    // Рассчитанные коэффициенты PID
    float kp_calculated;
    float ki_calculated;
    float kd_calculated;
    
    // Метрики качества
    bool tuning_successful;
    uint8_t progress_percent;           // 0-100%
    uint32_t tuning_duration_sec;       // Длительность процесса
    uint8_t oscillations_detected;      // Количество обнаруженных осцилляций
    
    // Сообщения
    char status_message[128];           // Текущий статус для UI
    char error_message[128];            // Сообщение об ошибке (если есть)
    
    // Старые коэффициенты (для сравнения)
    float kp_old;
    float ki_old;
    float kd_old;
} tuning_result_t;

/*******************************************************************************
 * API ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Инициализация модуля автонастройки
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tuner_init(void);

/**
 * @brief Запуск автонастройки (неблокирующая задача)
 * 
 * Создает отдельную задачу FreeRTOS для процесса автонастройки.
 * Процесс может занять 15-20 минут.
 * 
 * @param pump_idx Индекс насоса для настройки
 * @param method Метод автонастройки
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если уже запущена
 */
esp_err_t pid_auto_tuner_start(pump_index_t pump_idx, tuning_method_t method);

/**
 * @brief Проверка выполняется ли автонастройка
 * 
 * @param pump_idx Индекс насоса
 * @return true если процесс идет
 */
bool pid_auto_tuner_is_running(pump_index_t pump_idx);

/**
 * @brief Получение прогресса автонастройки
 * 
 * @param pump_idx Индекс насоса
 * @return Прогресс в процентах (0-100)
 */
uint8_t pid_auto_tuner_get_progress(pump_index_t pump_idx);

/**
 * @brief Получение результата автонастройки
 * 
 * @param pump_idx Индекс насоса
 * @param result Указатель на структуру для результата
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tuner_get_result(pump_index_t pump_idx, tuning_result_t *result);

/**
 * @brief Отмена процесса автонастройки
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tuner_cancel(pump_index_t pump_idx);

/**
 * @brief Применение найденных коэффициентов
 * 
 * Сохраняет рассчитанные Kp/Ki/Kd в конфигурацию и pump_manager
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tuner_apply_result(pump_index_t pump_idx);

#ifdef __cplusplus
}
#endif

#endif // PID_AUTO_TUNER_H

