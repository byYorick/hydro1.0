#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TASK_STATUS_COMPLETED,
    TASK_STATUS_FAILED,
    TASK_STATUS_RUNNING,
    TASK_STATUS_PENDING
} task_status_t;

typedef enum {
    TASK_TYPE_INTERVAL,      ///< Интервальная задача
    TASK_TYPE_DAILY,         ///< Ежедневная задача (в определенное время)
    TASK_TYPE_CONDITIONAL,   ///< Условная задача
    TASK_TYPE_ONCE           ///< Однократная задача
} task_type_t;

typedef void (*task_event_callback_t)(uint32_t task_id, task_status_t status);
typedef bool (*task_condition_callback_t)(void *arg);

/**
 * @brief Расписание для ежедневных задач
 */
typedef struct {
    uint8_t hour;            ///< Час (0-23)
    uint8_t minute;          ///< Минута (0-59)
    bool enabled;            ///< Включено ли это время
} daily_schedule_t;

// Инициализация планировщика задач
esp_err_t task_scheduler_init(void);

// Запуск планировщика
esp_err_t task_scheduler_start(void);

// Остановка планировщика
esp_err_t task_scheduler_stop(void);

// Добавление интервальной задачи
esp_err_t task_scheduler_add_task(uint32_t task_id, uint32_t interval_sec, void (*callback)(void *), void *arg);

// Добавление ежедневной задачи (cron-like)
esp_err_t task_scheduler_add_daily_task(uint32_t task_id, uint8_t hour, uint8_t minute, void (*callback)(void *), void *arg);

// Добавление условной задачи (выполняется при выполнении условия)
esp_err_t task_scheduler_add_conditional_task(uint32_t task_id, uint32_t check_interval_sec, 
                                               task_condition_callback_t condition, 
                                               void (*callback)(void *), void *arg);

// Добавление однократной задачи (выполняется один раз через delay)
esp_err_t task_scheduler_add_once_task(uint32_t task_id, uint32_t delay_sec, void (*callback)(void *), void *arg);

// Удаление задачи
esp_err_t task_scheduler_remove_task(uint32_t task_id);

// Включение/выключение задачи
esp_err_t task_scheduler_enable_task(uint32_t task_id, bool enable);

// Обработка задач
esp_err_t task_scheduler_process(void);

// Установка callback для событий
void task_scheduler_set_event_callback(task_event_callback_t cb);

// Преобразование статуса задачи в строку
const char* task_scheduler_status_to_string(task_status_t status);

// Получение информации о задачах
esp_err_t task_scheduler_get_info(char *buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // TASK_SCHEDULER_H
