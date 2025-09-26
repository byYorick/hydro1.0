#pragma once

/**
 * @brief Initialize the LVGL UI
 */
void lvgl_main_init(void);

/**
 * @brief Update sensor values on the display
 * 
 * @param ph pH value
 * @param ec EC value
 * @param temp Temperature value
 * @param hum Humidity value
 * @param lux Lux value
 * @param co2 CO2 value
 */
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2);