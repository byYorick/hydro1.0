#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TASK_STATUS_COMPLETED,
    TASK_STATUS_FAILED,
    TASK_STATUS_RUNNING,
    TASK_STATUS_PENDING
} task_status_t;

typedef void (*task_event_callback_t)(uint32_t task_id, task_status_t status);

// Инициализация планировщика задач
esp_err_t task_scheduler_init(void);

// Запуск планировщика
esp_err_t task_scheduler_start(void);

// Остановка планировщика
esp_err_t task_scheduler_stop(void);

// Добавление задачи
esp_err_t task_scheduler_add_task(uint32_t task_id, uint32_t interval_sec, void (*callback)(void *), void *arg);

// Обработка задач
esp_err_t task_scheduler_process(void);

// Установка callback для событий
void task_scheduler_set_event_callback(task_event_callback_t cb);

// Преобразование статуса задачи в строку
const char* task_scheduler_status_to_string(task_status_t status);

#ifdef __cplusplus
}
#endif

#endif // TASK_SCHEDULER_H
