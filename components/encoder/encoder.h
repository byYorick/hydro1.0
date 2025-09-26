#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Set encoder pin configuration
 * @param a_pin CLK pin
 * @param b_pin DT pin
 * @param sw_pin Switch pin
 */
void encoder_set_pins(int a_pin, int b_pin, int sw_pin);

/**
 * @brief Initialize the encoder
 */
void encoder_init(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H