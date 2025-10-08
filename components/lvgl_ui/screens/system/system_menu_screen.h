#pragma once

/**
 * @file system_menu_screen.h
 * @brief Экран системного меню
 * 
 * Главное меню системных настроек с доступом ко всем подменю.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация и регистрация системного меню
 * 
 * @return ESP_OK при успехе
 */
esp_err_t system_menu_screen_init(void);

#ifdef __cplusplus
}
#endif

