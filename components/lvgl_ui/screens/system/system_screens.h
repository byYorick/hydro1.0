#pragma once

/**
 * @file system_screens.h
 * @brief Все системные экраны (подменю настроек)
 * 
 * Включает экраны:
 * - Auto Control
 * - WiFi Settings
 * - Display Settings
 * - Data Logger
 * - System Info
 * - Reset Confirm
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрация всех системных экранов
 * 
 * @return ESP_OK при успехе
 */
esp_err_t system_screens_register_all(void);

#ifdef __cplusplus
}
#endif

