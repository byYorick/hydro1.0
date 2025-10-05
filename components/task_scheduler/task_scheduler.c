#include "task_scheduler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "TASK_SCHEDULER";

#define MAX_TASKS 20

typedef struct {
    uint32_t task_id;
    uint32_t interval_ms;
    void (*callback)(void *);
    void *arg;
    int64_t last_run_time;
    int64_t next_run_time;
    bool enabled;
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
    task->interval_ms = interval_sec * 1000;
    task->callback = callback;
    task->arg = arg;
    task->last_run_time = 0;
    task->next_run_time = esp_timer_get_time() / 1000; // Запускаем сразу
    task->enabled = true;
    task->status = TASK_STATUS_PENDING;
    
    ESP_LOGI(TAG, "Task %lu added with interval %lu sec", 
             (unsigned long)task_id, (unsigned long)interval_sec);
    
    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

esp_err_t task_scheduler_process(void) {
    if (!scheduler_running) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    int64_t current_time = esp_timer_get_time() / 1000; // мс
    
    for (int i = 0; i < task_count; i++) {
        scheduled_task_t *task = &tasks[i];
        
        if (!task->enabled) {
            continue;
        }
        
        // Проверяем, пора ли запускать задачу
        if (current_time >= task->next_run_time) {
            task->status = TASK_STATUS_RUNNING;
            
            // Уведомляем о начале выполнения
            if (event_callback) {
                event_callback(task->task_id, TASK_STATUS_RUNNING);
            }
            
            // Выполняем callback
            if (task->callback) {
                xSemaphoreGive(scheduler_mutex); // Отпускаем мьютекс перед callback
                task->callback(task->arg);
                xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(100)); // Берем обратно
            }
            
            // Обновляем время следующего запуска
            task->last_run_time = current_time;
            task->next_run_time = current_time + task->interval_ms;
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