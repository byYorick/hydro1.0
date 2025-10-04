#pragma once

#include "ui_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация оптимизированных экранов датчиков
esp_err_t sensor_screens_optimized_init(void);

// Обновление данных датчиков
esp_err_t sensor_screens_update_ph_data(float current_value, float target_value);
esp_err_t sensor_screens_update_ec_data(float current_value, float target_value);
esp_err_t sensor_screens_update_temp_data(float current_value, float target_value);
esp_err_t sensor_screens_update_humidity_data(float current_value, float target_value);
esp_err_t sensor_screens_update_lux_data(float current_value, float target_value);
esp_err_t sensor_screens_update_co2_data(float current_value, float target_value);

// Универсальная функция обновления
esp_err_t sensor_screens_update_data(sensor_type_t sensor_type, float current_value, float target_value);

#ifdef __cplusplus
}
#endif
