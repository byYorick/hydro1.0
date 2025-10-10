#include "task_scheduler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "TASK_SCHEDULER";

#define MAX_TASKS 30

typedef struct {
    uint32_t task_id;
    task_type_t type;
    uint32_t interval_ms;
    void (*callback)(void *);
    void *arg;
    task_condition_callback_t condition;
    int64_t last_run_time;
    int64_t next_run_time;
    uint8_t daily_hour;
    uint8_t daily_minute;
    bool enabled;
    bool executed_today;
    task_status_t status;
} scheduled_task_t;

static scheduled_task_t tasks[MAX_TASKS];
static int task_count = 0;
static task_event_callback_t event_callback = NULL;
static SemaphoreHandle_t scheduler_mutex = NULL;
static bool scheduler_running = false;

esp_err_t task_scheduler_init(void) {
    ESP_LOGI(TAG, "Initializing task scheduler");
    
    // Создаем мьютекс
    scheduler_mutex = xSemaphoreCreateMutex();
    if (scheduler_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create scheduler mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Инициализируем массив задач
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    scheduler_running = false;
    
    ESP_LOGI(TAG, "Task scheduler initialized successfully");
    return ESP_OK;
}

esp_err_t task_scheduler_start(void) {
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    scheduler_running = true;
    ESP_LOGI(TAG, "Task scheduler started");
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_stop(void) {
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    scheduler_running = false;
    ESP_LOGI(TAG, "Task scheduler stopped");
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_add_task(uint32_t task_id, uint32_t interval_sec, void (*callback)(void *), void *arg) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (task_count >= MAX_TASKS) {
        xSemaphoreGive(scheduler_mutex);
        ESP_LOGE(TAG, "Task list is full");
        return ESP_ERR_NO_MEM;
    }
    
    // Добавляем задачу
    scheduled_task_t *task = &tasks[task_count++];
    task->task_id = task_id;
    task->type = TASK_TYPE_INTERVAL;
    task->interval_ms = interval_sec * 1000;
    task->callback = callback;
    task->arg = arg;
    task->condition = NULL;
    task->last_run_time = 0;
    task->next_run_time = esp_timer_get_time() / 1000;
    task->enabled = true;
    task->executed_today = false;
    task->status = TASK_STATUS_PENDING;
    
    ESP_LOGI(TAG, "Interval task %lu added: %lu sec", 
             (unsigned long)task_id, (unsigned long)interval_sec);
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_add_daily_task(uint32_t task_id, uint8_t hour, uint8_t minute, void (*callback)(void *), void *arg) {
    if (callback == NULL || hour > 23 || minute > 59) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (task_count >= MAX_TASKS) {
        xSemaphoreGive(scheduler_mutex);
        ESP_LOGE(TAG, "Task list is full");
        return ESP_ERR_NO_MEM;
    }
    
    scheduled_task_t *task = &tasks[task_count++];
    task->task_id = task_id;
    task->type = TASK_TYPE_DAILY;
    task->callback = callback;
    task->arg = arg;
    task->condition = NULL;
    task->daily_hour = hour;
    task->daily_minute = minute;
    task->enabled = true;
    task->executed_today = false;
    task->status = TASK_STATUS_PENDING;
    
    ESP_LOGI(TAG, "Daily task %lu added: %02d:%02d", 
             (unsigned long)task_id, hour, minute);
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_add_conditional_task(uint32_t task_id, uint32_t check_interval_sec, 
                                               task_condition_callback_t condition, 
                                               void (*callback)(void *), void *arg) {
    if (callback == NULL || condition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (task_count >= MAX_TASKS) {
        xSemaphoreGive(scheduler_mutex);
        ESP_LOGE(TAG, "Task list is full");
        return ESP_ERR_NO_MEM;
    }
    
    scheduled_task_t *task = &tasks[task_count++];
    task->task_id = task_id;
    task->type = TASK_TYPE_CONDITIONAL;
    task->interval_ms = check_interval_sec * 1000;
    task->callback = callback;
    task->arg = arg;
    task->condition = condition;
    task->next_run_time = esp_timer_get_time() / 1000;
    task->enabled = true;
    task->status = TASK_STATUS_PENDING;
    
    ESP_LOGI(TAG, "Conditional task %lu added: check every %lu sec", 
             (unsigned long)task_id, (unsigned long)check_interval_sec);
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_add_once_task(uint32_t task_id, uint32_t delay_sec, void (*callback)(void *), void *arg) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (task_count >= MAX_TASKS) {
        xSemaphoreGive(scheduler_mutex);
        ESP_LOGE(TAG, "Task list is full");
        return ESP_ERR_NO_MEM;
    }
    
    scheduled_task_t *task = &tasks[task_count++];
    task->task_id = task_id;
    task->type = TASK_TYPE_ONCE;
    task->callback = callback;
    task->arg = arg;
    task->condition = NULL;
    task->next_run_time = esp_timer_get_time() / 1000 + (delay_sec * 1000);
    task->enabled = true;
    task->status = TASK_STATUS_PENDING;
    
    ESP_LOGI(TAG, "Once task %lu added: delay %lu sec", 
             (unsigned long)task_id, (unsigned long)delay_sec);
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_remove_task(uint32_t task_id) {
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].task_id == task_id) {
            // Сдвигаем все задачи после этой
            for (int j = i; j < task_count - 1; j++) {
                tasks[j] = tasks[j + 1];
            }
            task_count--;
            
            ESP_LOGI(TAG, "Task %lu removed", (unsigned long)task_id);
            xSemaphoreGive(scheduler_mutex);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t task_scheduler_enable_task(uint32_t task_id, bool enable) {
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].task_id == task_id) {
            tasks[i].enabled = enable;
            ESP_LOGI(TAG, "Task %lu %s", (unsigned long)task_id, enable ? "enabled" : "disabled");
            xSemaphoreGive(scheduler_mutex);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t task_scheduler_process(void) {
    if (!scheduler_running) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    int64_t current_time = esp_timer_get_time() / 1000; // мс
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    for (int i = 0; i < task_count; i++) {
        scheduled_task_t *task = &tasks[i];
        
        if (!task->enabled) {
            continue;
        }
        
        bool should_execute = false;
        
        switch (task->type) {
            case TASK_TYPE_INTERVAL:
                // Интервальная задача
                if (current_time >= task->next_run_time) {
                    should_execute = true;
                }
                break;
                
            case TASK_TYPE_DAILY:
                // Ежедневная задача в определенное время
                if (timeinfo.tm_hour == task->daily_hour && 
                    timeinfo.tm_min == task->daily_minute &&
                    !task->executed_today) {
                    should_execute = true;
                    task->executed_today = true;
                }
                
                // Сброс флага на следующий день
                if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
                    task->executed_today = false;
                }
                break;
                
            case TASK_TYPE_CONDITIONAL:
                // Условная задача - проверяем условие
                if (current_time >= task->next_run_time) {
                    if (task->condition && task->condition(task->arg)) {
                        should_execute = true;
                    }
                    // Обновляем время следующей проверки
                    task->next_run_time = current_time + task->interval_ms;
                }
                break;
                
            case TASK_TYPE_ONCE:
                // Однократная задача
                if (current_time >= task->next_run_time) {
                    should_execute = true;
                }
                break;
        }
        
        if (should_execute) {
            task->status = TASK_STATUS_RUNNING;
            
            // Уведомляем о начале выполнения
            if (event_callback) {
                event_callback(task->task_id, TASK_STATUS_RUNNING);
            }
            
            // Выполняем callback
            if (task->callback) {
                xSemaphoreGive(scheduler_mutex);
                task->callback(task->arg);
                xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(100));
            }
            
            // Обновляем время для интервальных задач
            if (task->type == TASK_TYPE_INTERVAL) {
                task->last_run_time = current_time;
                task->next_run_time = current_time + task->interval_ms;
            }
            
            // Удаляем однократные задачи после выполнения
            if (task->type == TASK_TYPE_ONCE) {
                task->enabled = false;
            }
            
            task->status = TASK_STATUS_COMPLETED;
            
            // Уведомляем о завершении
            if (event_callback) {
                event_callback(task->task_id, TASK_STATUS_COMPLETED);
            }
        }
    }
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

void task_scheduler_set_event_callback(task_event_callback_t cb) {
    event_callback = cb;
    ESP_LOGI(TAG, "Event callback set");
}

const char* task_scheduler_status_to_string(task_status_t status) {
    switch (status) {
        case TASK_STATUS_COMPLETED: return "COMPLETED";
        case TASK_STATUS_FAILED: return "FAILED";
        case TASK_STATUS_RUNNING: return "RUNNING";
        case TASK_STATUS_PENDING: return "PENDING";
        default: return "UNKNOWN";
    }
}

esp_err_t task_scheduler_get_info(char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    int offset = 0;
    offset += snprintf(buffer + offset, max_len - offset,
                      "Task Scheduler:\n- Running: %s\n- Tasks: %d/%d\n\n",
                      scheduler_running ? "Yes" : "No", task_count, MAX_TASKS);
    
    for (int i = 0; i < task_count && offset < max_len - 100; i++) {
        const char *type_str = "Unknown";
        switch (tasks[i].type) {
            case TASK_TYPE_INTERVAL: type_str = "Interval"; break;
            case TASK_TYPE_DAILY: type_str = "Daily"; break;
            case TASK_TYPE_CONDITIONAL: type_str = "Conditional"; break;
            case TASK_TYPE_ONCE: type_str = "Once"; break;
        }
        
        offset += snprintf(buffer + offset, max_len - offset,
                          "Task %lu: %s [%s]\n",
                          (unsigned long)tasks[i].task_id,
                          type_str,
                          tasks[i].enabled ? "ENABLED" : "DISABLED");
    }
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}