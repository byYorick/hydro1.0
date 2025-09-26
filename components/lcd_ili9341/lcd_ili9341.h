#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Initialize the ILI9341 LCD display with LVGL support
 * 
 * @return lv_disp_t* Pointer to the LVGL display driver
 */
lv_disp_t* lcd_ili9341_init(void);

/**
 * @brief Set the display brightness
 * 
 * @param brightness Brightness level (0-100)
 */
void lcd_ili9341_set_brightness(uint8_t brightness);

/**
 * @brief Update sensor values on the display (deprecated)
 * 
 * @param ph pH value
 * @param ec EC value
 * @param temp Temperature value
 * @param hum Humidity value
 * @param lux Lux value
 * @param co2 CO2 value
 */
void lcd_ili9341_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2);

/**
 * @brief Lock the LVGL mutex
 * 
 * @param timeout_ms Timeout in milliseconds (-1 for infinite wait)
 * @return true if lock acquired, false otherwise
 */
bool lvgl_lock(int timeout_ms);

/**
 * @brief Unlock the LVGL mutex
 */
void lvgl_unlock(void);

#ifdef __cplusplus
}
#endif