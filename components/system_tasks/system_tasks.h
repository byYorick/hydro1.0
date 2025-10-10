/**
 * @file system_tasks.h
 * @brief Управление задачами FreeRTOS системы гидропоники
 *
 * Этот модуль содержит все задачи системы и функции для их создания
 * и управления. Вынесено из app_main.c для упрощения архитектуры.
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TaskHandle_t sensor_task;
    TaskHandle_t display_task;
    TaskHandle_t notification_task;
    TaskHandle_t data_logger_task;
    TaskHandle_t scheduler_task;
    TaskHandle_t ph_ec_task;
    TaskHandle_t mqtt_publish_task;
    TaskHandle_t telegram_task;
    TaskHandle_t sd_logging_task;
    TaskHandle_t ai_correction_task;
    TaskHandle_t mesh_heartbeat_task;
} system_task_handles_t;

typedef struct {
    uint32_t execution_count;
    uint32_t failure_count;
    uint32_t last_duration_ms;
    uint32_t max_duration_ms;
    uint32_t missed_deadlines;
} task_runtime_stats_t;

typedef struct {
    QueueHandle_t sensor_data_queue;
    SemaphoreHandle_t sensor_data_mutex;
    sensor_data_t last_sensor_data;
    bool sensor_data_valid;
    system_config_t active_config;
    bool config_valid;
    uint32_t sensor_failure_streak[SENSOR_COUNT];
    uint32_t sensor_failure_total[SENSOR_COUNT];
    bool sensor_fault_active[SENSOR_COUNT];
    task_runtime_stats_t sensor_stats;
    task_runtime_stats_t data_logger_stats;
    task_runtime_stats_t notification_stats;
} system_tasks_context_t;

esp_err_t system_tasks_init_context(void);
esp_err_t system_tasks_create_all(system_task_handles_t *handles);

system_tasks_context_t* system_tasks_get_context(void);
system_task_handles_t* system_tasks_get_handles(void);

esp_err_t system_tasks_set_config(const system_config_t *config);

esp_err_t system_tasks_stop_all(void);
esp_err_t system_tasks_get_stats(char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

