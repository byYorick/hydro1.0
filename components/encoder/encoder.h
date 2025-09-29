#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Encoder event types
typedef enum {
    ENCODER_EVENT_ROTATE_CW,      // Clockwise rotation
    ENCODER_EVENT_ROTATE_CCW,     // Counter-clockwise rotation
    ENCODER_EVENT_BUTTON_PRESS,   // Short button press
    ENCODER_EVENT_BUTTON_LONG_PRESS, // Long button press
    ENCODER_EVENT_BUTTON_RELEASE  // Button release
} encoder_event_type_t;

// Encoder event structure
typedef struct {
    encoder_event_type_t type;
    int32_t value;  // Additional value (e.g., rotation count)
} encoder_event_t;

/**
 * @brief Set encoder pin configuration
 * @param a_pin CLK pin
 * @param b_pin DT pin
 * @param sw_pin Switch pin
 */
void encoder_set_pins(int a_pin, int b_pin, int sw_pin);

/**
 * @brief Initialize the encoder with interrupt handling
 */
void encoder_init(void);

/**
 * @brief Get the encoder event queue handle
 * @return Queue handle for encoder events
 */
QueueHandle_t encoder_get_event_queue(void);

/**
 * @brief Set long press duration in milliseconds
 * @param duration_ms Long press duration in milliseconds
 */
void encoder_set_long_press_duration(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H