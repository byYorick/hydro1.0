/**
 * @file pid_auto_tune_screen.h
 * @brief Экран автонастройки PID контроллера (Ziegler-Nichols)
 * 
 * Позволяет:
 * - Выбрать насос для настройки
 * - Запустить процесс автонастройки
 * - Отслеживать прогресс в реальном времени
 * - Применить или отменить результат
 */

#ifndef PID_AUTO_TUNE_SCREEN_H
#define PID_AUTO_TUNE_SCREEN_H

#include "lvgl.h"
#include "system_config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана автонастройки PID
 * @param params pump_index_t переданный как (void*)(intptr_t)pump_idx (опционально)
 * @return LVGL объект экрана или NULL при ошибке
 */
lv_obj_t* pid_auto_tune_screen_create(void *params);

/**
 * @brief Callback при показе экрана
 * @param screen_obj LVGL объект экрана
 * @param params pump_index_t переданный как (void*)(intptr_t)pump_idx (опционально)
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tune_screen_on_show(lv_obj_t *screen_obj, void *params);

/**
 * @brief Callback при скрытии экрана
 * @param screen_obj LVGL объект экрана
 * @return ESP_OK при успехе
 */
esp_err_t pid_auto_tune_screen_on_hide(lv_obj_t *screen_obj);

#ifdef __cplusplus
}
#endif

#endif // PID_AUTO_TUNE_SCREEN_H

