#pragma once

/**
 * @file sensor_detail_screen.h
 * @brief Шаблон экрана детализации датчика
 * 
 * Универсальный экран для отображения детальной информации о любом датчике.
 * Используется для всех 6 датчиков (pH, EC, Temp, Humidity, Lux, CO2).
 */

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрация всех экранов детализации датчиков
 * 
 * Регистрирует 6 экранов (detail_ph, detail_ec, detail_temp, 
 * detail_humidity, detail_lux, detail_co2) используя единый шаблон.
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sensor_detail_screens_register_all(void);

/**
 * @brief Обновить значение на экране детализации
 * 
 * @param sensor_index Индекс датчика (0-5)
 * @param current_value Текущее значение
 * @param target_value Целевое значение
 * @return ESP_OK при успехе
 */
esp_err_t sensor_detail_screen_update(uint8_t sensor_index, 
                                       float current_value,
                                       float target_value);

#ifdef __cplusplus
}
#endif

