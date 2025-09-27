#ifndef XPT2046_H
#define XPT2046_H

/**
 * @file xpt2046.h
 * @brief XPT2046 Touch Controller Component for ESP-IDF
 * 
 * This component provides an interface to control the XPT2046 touch controller
 * commonly used with ILI9341 displays.
 * 
 * ## Usage
 * 
 * 1. Include this header in your source file:
 *    ```c
 *    #include "xpt2046.h"
 *    ```
 * 
 * 2. Initialize the touch controller:
 *    ```c
 *    if (xpt2046_init()) {
 *        // Touch controller initialized successfully
 *    }
 *    ```
 * 
 * 3. Read touch coordinates:
 *    ```c
 *    uint16_t x, y;
 *    if (xpt2046_read_touch(&x, &y)) {
 *        // Touch detected at (x, y)
 *    }
 *    ```
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Touch controller configuration
#define XPT2046_CLOCK_SPEED_HZ   1000000  // 1MHz SPI clock speed

// XPT2046 commands
#define XPT2046_CMD_XPOS         0xD0  // Read X position
#define XPT2046_CMD_YPOS         0x90  // Read Y position
#define XPT2046_CMD_Z1POS        0xB0  // Read Z1 position
#define XPT2046_CMD_Z2POS        0xC0  // Read Z2 position

// Touch calibration values (will need to be adjusted for your specific display)
#define XPT2046_MIN_RAW_X        300
#define XPT2046_MAX_RAW_X        3800
#define XPT2046_MIN_RAW_Y        200
#define XPT2046_MAX_RAW_Y        3900

/**
 * @brief Initialize the XPT2046 touch controller
 * @return true on success, false on failure
 */
bool xpt2046_init(void);

/**
 * @brief Deinitialize the XPT2046 touch controller
 */
void xpt2046_deinit(void);

/**
 * @brief Read touch position
 * @param x X coordinate (0-320)
 * @param y Y coordinate (0-240)
 * @return true if touch detected, false otherwise
 */
bool xpt2046_read_touch(uint16_t *x, uint16_t *y);

/**
 * @brief Check if touch is detected
 * @return true if touch is detected, false otherwise
 */
bool xpt2046_is_touched(void);

/**
 * @brief Calibrate touch controller
 * @param min_x Minimum raw X value
 * @param max_x Maximum raw X value
 * @param min_y Minimum raw Y value
 * @param max_y Maximum raw Y value
 */
void xpt2046_calibrate(uint16_t min_x, uint16_t max_x, uint16_t min_y, uint16_t max_y);

#ifdef __cplusplus
}
#endif

#endif // XPT2046_H