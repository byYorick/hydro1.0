#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Encoder event types
 */
typedef enum {
    ENCODER_EVENT_LEFT,     /*!< Encoder rotated left/counterclockwise */
    ENCODER_EVENT_RIGHT,    /*!< Encoder rotated right/clockwise */
    ENCODER_EVENT_BUTTON    /*!< Encoder button pressed */
} encoder_event_t;

/**
 * @brief Encoder configuration structure
 */
typedef struct {
    int a_pin;              /*!< CLK pin */
    int b_pin;              /*!< DT pin */
    int sw_pin;             /*!< Switch pin */
    int high_limit;         /*!< High count limit */
    int low_limit;          /*!< Low count limit */
} encoder_config_t;

/**
 * @brief Callback function type for encoder events
 */
typedef void (*encoder_callback_t)(encoder_event_t event, void* user_ctx);

/**
 * @brief Initialize the rotary encoder with configuration
 * 
 * @param config Pointer to encoder configuration
 * @param callback Callback function for encoder events
 * @param user_ctx User context to pass to callback
 * @return true if successful, false otherwise
 */
bool encoder_init_with_config(const encoder_config_t* config, encoder_callback_t callback, void* user_ctx);

/**
 * @brief Get current encoder count
 * 
 * @return Current encoder count
 */
int encoder_get_count(void);

/**
 * @brief Clear encoder count to zero
 */
void encoder_clear_count(void);

/**
 * @brief Check if encoder button is pressed
 * 
 * @return true if button is pressed, false otherwise
 */
bool encoder_button_pressed(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H