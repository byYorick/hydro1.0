/**
 * @file pid_controller.h
 * @brief Классический PID контроллер для гидропонной системы
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация PID
 */
typedef struct {
    float kp;           ///< Пропорциональный коэффициент
    float ki;           ///< Интегральный коэффициент
    float kd;           ///< Дифференциальный коэффициент
    float setpoint;     ///< Целевое значение
    float output_min;   ///< Минимальный выход (мл)
    float output_max;   ///< Максимальный выход (мл)
    float integral_min; ///< Anti-windup min
    float integral_max; ///< Anti-windup max
} pid_config_t;

/**
 * @brief Выход PID контроллера
 */
typedef struct {
    float p_term;       ///< P вклад
    float i_term;       ///< I вклад
    float d_term;       ///< D вклад
    float output;       ///< Итоговый выход
    float error;        ///< Текущая ошибка
} pid_output_t;

/**
 * @brief История значений для D-компоненты
 */
typedef struct {
    float values[10];   ///< Последние 10 значений
    uint8_t index;      ///< Текущий индекс
    uint8_t count;      ///< Количество значений
} pid_history_t;

/**
 * @brief Экземпляр PID контроллера
 */
typedef struct {
    pid_config_t config;        ///< Конфигурация
    float integral;             ///< Накопленный интеграл
    float prev_error;           ///< Предыдущая ошибка для D
    pid_history_t history;      ///< История для фильтрации D
    pid_output_t last_output;   ///< Последний выход
    uint32_t sample_count;      ///< Количество вычислений
    bool initialized;           ///< Инициализирован ли
} pid_controller_t;

/**
 * @brief Инициализация PID контроллера
 * 
 * @param pid Указатель на экземпляр PID
 * @param config Конфигурация
 * @return ESP_OK при успехе
 */
esp_err_t pid_init(pid_controller_t *pid, const pid_config_t *config);

/**
 * @brief Вычисление выхода PID
 * 
 * @param pid Указатель на экземпляр PID
 * @param measured_value Текущее измеренное значение
 * @param dt Время с последнего вычисления (секунды)
 * @param output Выходные данные PID
 * @return ESP_OK при успехе
 */
esp_err_t pid_compute(pid_controller_t *pid, float measured_value, float dt, pid_output_t *output);

/**
 * @brief Сброс интегральной компоненты
 * 
 * @param pid Указатель на экземпляр PID
 * @return ESP_OK при успехе
 */
esp_err_t pid_reset(pid_controller_t *pid);

/**
 * @brief Установка коэффициентов PID
 * 
 * @param pid Указатель на экземпляр PID
 * @param kp Пропорциональный коэффициент
 * @param ki Интегральный коэффициент
 * @param kd Дифференциальный коэффициент
 * @return ESP_OK при успехе
 */
esp_err_t pid_set_tunings(pid_controller_t *pid, float kp, float ki, float kd);

/**
 * @brief Установка целевого значения
 * 
 * @param pid Указатель на экземпляр PID
 * @param setpoint Новое целевое значение
 * @return ESP_OK при успехе
 */
esp_err_t pid_set_setpoint(pid_controller_t *pid, float setpoint);

/**
 * @brief Установка ограничений выхода
 * 
 * @param pid Указатель на экземпляр PID
 * @param min Минимум
 * @param max Максимум
 * @return ESP_OK при успехе
 */
esp_err_t pid_set_output_limits(pid_controller_t *pid, float min, float max);

/**
 * @brief Получение последнего выхода
 * 
 * @param pid Указатель на экземпляр PID
 * @return Последний выход
 */
pid_output_t pid_get_last_output(const pid_controller_t *pid);

#ifdef __cplusplus
}
#endif

#endif // PID_CONTROLLER_H

