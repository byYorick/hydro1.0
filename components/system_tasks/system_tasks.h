/**
 * @file system_tasks.h
 * @brief Управление задачами FreeRTOS системы гидропоники
 * 
 * Этот модуль содержит все задачи системы и функции для их создания
 * и управления. Вынесено из app_main.c для упрощения архитектуры.
 * 
 * @author Hydroponics Monitor Team
 * @date 2025
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

/*******************************************************************************
 * СТРУКТУРЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Дескрипторы задач системы
 * 
 * Примечание: encoder_task создается в lvgl_main.c
 */
typedef struct {
    TaskHandle_t sensor_task;
    TaskHandle_t display_task;
    TaskHandle_t notification_task;
    TaskHandle_t data_logger_task;
    TaskHandle_t scheduler_task;
    TaskHandle_t ph_ec_task;
} system_task_handles_t;

/**
 * @brief Контекст системы задач
 * 
 * Примечание: encoder_queue создается в encoder.c
 */
typedef struct {
    QueueHandle_t sensor_data_queue;
    SemaphoreHandle_t sensor_data_mutex;
    sensor_data_t last_sensor_data;
    bool sensor_data_valid;
} system_tasks_context_t;

/*******************************************************************************
 * ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
 ******************************************************************************/

/**
 * @brief Инициализация контекста задач
 * 
 * Создает очереди и мьютексы, необходимые для работы задач.
 * 
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t system_tasks_init_context(void);

/**
 * @brief Создание всех задач системы
 * 
 * Создает 6 основных задач FreeRTOS:
 * - sensor_task
 * - display_task
 * - notification_task
 * - data_logger_task
 * - scheduler_task
 * - ph_ec_task
 * 
 * Примечание: encoder_task создается в lvgl_main.c
 * 
 * @param handles Указатель на структуру для сохранения дескрипторов задач
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t system_tasks_create_all(system_task_handles_t *handles);

/**
 * @brief Получение контекста задач
 * 
 * @return Указатель на контекст задач
 */
system_tasks_context_t* system_tasks_get_context(void);

/**
 * @brief Получение дескрипторов задач
 * 
 * @return Указатель на дескрипторы задач
 */
system_task_handles_t* system_tasks_get_handles(void);

/*******************************************************************************
 * ФУНКЦИИ УПРАВЛЕНИЯ ЗАДАЧАМИ
 ******************************************************************************/

/**
 * @brief Остановка всех задач
 * 
 * @return ESP_OK при успехе
 */
esp_err_t system_tasks_stop_all(void);

/**
 * @brief Получение статистики задач
 * 
 * @param buffer Буфер для записи статистики
 * @param size Размер буфера
 * @return ESP_OK при успехе
 */
esp_err_t system_tasks_get_stats(char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

