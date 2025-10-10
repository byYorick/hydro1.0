#pragma once

/**
 * @file pid_main_screen.h
 * @brief Главный экран PID контроллеров
 * 
 * Список всех 6 PID контроллеров с основными параметрами
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание главного экрана PID
 * 
 * @param context Контекст (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_main_screen_create(void *context);

/**
 * @brief Callback при показе экрана
 */
esp_err_t pid_main_screen_on_show(lv_obj_t *screen, void *params);

/**
 * @brief Callback при скрытии экрана
 */
esp_err_t pid_main_screen_on_hide(lv_obj_t *screen);

/**
 * @brief Обновление экрана PID
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pid_main_screen_update(void);

#ifdef __cplusplus
}
#endif

