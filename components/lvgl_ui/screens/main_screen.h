#pragma once

/**
 * @file main_screen.h
 * @brief Главный экран гидропонной системы
 * 
 * Отображает карточки всех 6 датчиков и кнопку системных настроек.
 * Корневой экран системы.
 * 
 * @author Hydroponics Monitor Team
 * @date 2025-10-08
 * @version 1.0
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация и регистрация главного экрана
 * 
 * Регистрирует главный экран в Screen Manager как корневой (is_root=true).
 * Должна быть вызвана после screen_manager_init().
 * 
 * @return ESP_OK при успехе
 */
esp_err_t main_screen_init(void);

/**
 * @brief Обновить значения датчиков на главном экране
 * 
 * @param sensor_index Индекс датчика (0-5)
 * @param value Новое значение
 * @return ESP_OK при успехе
 */
esp_err_t main_screen_update_sensor(uint8_t sensor_index, float value);

#ifdef __cplusplus
}
#endif

