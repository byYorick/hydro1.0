/**
 * @file pump_pid_manager.h
 * @brief Менеджер PID контроллеров для 6 насосов
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef PUMP_PID_MANAGER_H
#define PUMP_PID_MANAGER_H

#include "esp_err.h"
#include "pid_controller.h"
#include "pump_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Индексы PID контроллеров насосов
 */
typedef enum {
    PUMP_PID_PH_UP,
    PUMP_PID_PH_DOWN,
    PUMP_PID_EC_A,
    PUMP_PID_EC_B,
    PUMP_PID_EC_C,
    PUMP_PID_WATER,
    PUMP_PID_COUNT
} pump_pid_index_t;

/**
 * @brief Экземпляр PID насоса
 */
typedef struct {
    pid_controller_t pid;           ///< PID контроллер
    pump_stats_t pump_stats;        ///< Статистика насоса
    uint32_t last_compute_time;     ///< Время последнего расчета
    bool enabled;                   ///< Включен ли
    bool auto_mode;                 ///< Автоматический режим
    float last_measured_value;      ///< Последнее измеренное значение
} pump_pid_instance_t;

/**
 * @brief Инициализация менеджера PID
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_manager_init(void);

/**
 * @brief Вычисление и выполнение PID коррекции
 * 
 * @param pump_idx Индекс насоса
 * @param measured_value Текущее значение
 * @param target_value Целевое значение
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_compute_and_execute(pump_pid_index_t pump_idx, float measured_value, float target_value);

/**
 * @brief Только вычисление без выполнения
 * 
 * @param pump_idx Индекс насоса
 * @param measured_value Текущее значение
 * @param target_value Целевое значение
 * @param output Выход PID
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_compute(pump_pid_index_t pump_idx, float measured_value, float target_value, pid_output_t *output);

/**
 * @brief Выполнение дозы
 * 
 * @param pump_idx Индекс насоса
 * @param dose_ml Доза в мл
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_execute(pump_pid_index_t pump_idx, float dose_ml);

/**
 * @brief Получение выхода PID
 * 
 * @param pump_idx Индекс насоса
 * @return Последний выход PID
 */
pid_output_t pump_pid_get_output(pump_pid_index_t pump_idx);

/**
 * @brief Получение статистики
 * 
 * @param pump_idx Индекс насоса
 * @param stats Статистика
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_get_stats(pump_pid_index_t pump_idx, pump_stats_t *stats);

/**
 * @brief Сброс интеграла PID
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_reset(pump_pid_index_t pump_idx);

/**
 * @brief Установка режима (авто/ручной)
 * 
 * @param pump_idx Индекс насоса
 * @param auto_mode true для автоматического режима
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_set_mode(pump_pid_index_t pump_idx, bool auto_mode);

/**
 * @brief Установка коэффициентов PID
 * 
 * @param pump_idx Индекс насоса
 * @param kp Пропорциональный
 * @param ki Интегральный
 * @param kd Дифференциальный
 * @return ESP_OK при успехе
 */
esp_err_t pump_pid_set_tunings(pump_pid_index_t pump_idx, float kp, float ki, float kd);

/**
 * @brief Получение экземпляра PID (для отладки/UI)
 * 
 * @param pump_idx Индекс насоса
 * @return Указатель на экземпляр или NULL
 */
pump_pid_instance_t* pump_pid_get_instance(pump_pid_index_t pump_idx);

#ifdef __cplusplus
}
#endif

#endif // PUMP_PID_MANAGER_H

