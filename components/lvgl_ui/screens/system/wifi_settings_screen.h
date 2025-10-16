/**
 * @file wifi_settings_screen.h
 * @brief Экран настроек WiFi с полным функционалом
 */

#ifndef WIFI_SETTINGS_SCREEN_H
#define WIFI_SETTINGS_SCREEN_H

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана настроек WiFi
 * @param params Параметры (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* wifi_settings_screen_create(void *params);

/**
 * @brief Callback при показе экрана
 */
esp_err_t wifi_settings_screen_on_show(lv_obj_t *screen_obj, void *params);

/**
 * @brief Callback при скрытии экрана
 */
esp_err_t wifi_settings_screen_on_hide(lv_obj_t *screen_obj, void *params);

#ifdef __cplusplus
}
#endif

#endif // WIFI_SETTINGS_SCREEN_H

