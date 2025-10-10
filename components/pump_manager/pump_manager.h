/**
 * @file pump_manager.h
 * @brief Менеджер насосов с PID контроллером
 * 
 * Управление 6 перистальтическими насосами с PID регулированием,
 * статистикой, safety checks и логированием.
 */

#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Структура PID контроллера
 */
typedef struct {
    float kp;                    // Пропорциональный коэффициент
    float ki;                    // Интегральный коэффициент
    float kd;                    // Дифференциальный коэффициент
    float setpoint;              // Целевое значение
    float integral;              // Интегральная составляющая
    float prev_error;            // Предыдущая ошибка для D
    float prev_derivative;       // Предыдущая производная для фильтра
    float output_min;            // Минимальный выход (мл)
    float output_max;            // Максимальный выход (мл)
    uint64_t last_time_us;       // Время последнего расчета (мкс)
    bool enabled;                // Включен ли контроллер
} pid_controller_t;

/**
 * @brief Статистика насоса
 */
typedef struct {
    uint32_t total_runs;         // Общее количество запусков
    float total_volume_ml;       // Общий объем дозированный (мл)
    uint64_t total_time_ms;      // Общее время работы (мс)
    uint64_t last_run_time;      // Время последнего запуска (timestamp)
    float daily_volume_ml;       // Суточный объем (мл)
    uint64_t daily_reset_time;   // Время последнего сброса суточного счетчика
} pump_stats_t;

/**
 * @brief Выходные данные PID контроллера
 */
typedef struct {
    float output;                // Выходное значение (мл)
    float p_term;                // P-компонента
    float i_term;                // I-компонента
    float d_term;                // D-компонента
    float error;                 // Текущая ошибка
} pid_output_t;

/**
 * @brief Инициализация менеджера насосов
 * 
 * Создает мьютексы, инициализирует PID контроллеры и запускает задачу мониторинга
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_init(void);

/**
 * @brief Установка параметров PID
 * 
 * @param pump_idx Индекс насоса
 * @param kp Пропорциональный коэффициент
 * @param ki Интегральный коэффициент
 * @param kd Дифференциальный коэффициент
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_set_pid_tunings(pump_index_t pump_idx, float kp, float ki, float kd);

/**
 * @brief Расчет PID и выполнение коррекции
 * 
 * Вычисляет необходимую дозу и запускает насос с 3 попытками при ошибке
 * 
 * @param pump_idx Индекс насоса
 * @param current Текущее значение параметра
 * @param target Целевое значение параметра
 * @return ESP_OK при успехе, ESP_FAIL при ошибке
 */
esp_err_t pump_manager_compute_and_execute(pump_index_t pump_idx, float current, float target);

/**
 * @brief Только расчет PID без выполнения
 * 
 * @param pump_idx Индекс насоса
 * @param current Текущее значение
 * @param target Целевое значение
 * @param output Выходные данные PID
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_compute_pid(pump_index_t pump_idx, float current, float target, pid_output_t *output);

/**
 * @brief Получение статистики насоса
 * 
 * @param pump_idx Индекс насоса
 * @param stats Указатель на структуру для статистики
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_get_stats(pump_index_t pump_idx, pump_stats_t *stats);

/**
 * @brief Сброс интегральной составляющей PID
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_reset_pid(pump_index_t pump_idx);

/**
 * @brief Ручной сброс суточного счетчика
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_reset_daily_counter(pump_index_t pump_idx);

/**
 * @brief Получение текущего суточного объема
 * 
 * @param pump_idx Индекс насоса
 * @param volume Указатель на переменную для объема
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_get_daily_volume(pump_index_t pump_idx, float *volume);

/**
 * @brief Применение конфигурации PID из system_config
 * 
 * @param config Указатель на системную конфигурацию
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_apply_config(const system_config_t *config);

/**
 * @brief Прямой запуск насоса (для калибровки и тестов)
 * 
 * Запускает насос на указанное время, игнорируя PID и большинство лимитов
 * 
 * @param pump_idx Индекс насоса
 * @param duration_ms Длительность работы (мс)
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_run_direct(pump_index_t pump_idx, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // PUMP_MANAGER_H

