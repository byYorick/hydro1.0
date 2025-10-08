#pragma once

/**
 * @file screen_init.h
 * @brief Централизованная инициализация всех экранов
 * 
 * Регистрирует все экраны системы в Screen Manager.
 * Вызывается один раз при старте приложения.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация Screen Manager и регистрация всех экранов
 * 
 * Выполняет:
 * 1. Инициализацию Screen Manager
 * 2. Регистрацию главного экрана
 * 3. Регистрацию экранов датчиков (12 экранов)
 * 4. Регистрацию системных экранов (7 экранов)
 * 5. Показ главного экрана
 * 
 * @return ESP_OK при успехе
 */
esp_err_t screen_system_init_all(void);

#ifdef __cplusplus
}
#endif

