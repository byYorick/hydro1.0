#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/**
 * @brief Initialize the main LVGL UI
 */
void lvgl_main_init(void);

/**
 * @brief Touch input device callback function
 */
void touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data);

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

#ifdef __cplusplus
}
#endif