#pragma once

/**
 * @file sensor_settings_screen.h
 * @brief Шаблон экрана настроек датчика
 * 
 * Универсальный экран настроек для всех датчиков.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрация всех экранов настроек датчиков
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sensor_settings_screens_register_all(void);

#ifdef __cplusplus
}
#endif

