/**
 * @file settings_main_screen.h
 * @brief Главное меню настроек системы
 */

#ifndef SETTINGS_MAIN_SCREEN_H
#define SETTINGS_MAIN_SCREEN_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация главного экрана настроек
 */
esp_err_t settings_main_screen_init(void);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_MAIN_SCREEN_H

