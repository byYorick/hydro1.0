#ifndef TREMA_LUX_H
#define TREMA_LUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the Trema LUX sensor
 * @return true on success, false on failure
 */
bool trema_lux_init(void);

/**
 * @brief Read LUX value from the Trema LUX sensor
 * @param lux Pointer to store the LUX value
 * @return true on success, false on failure
 */
bool trema_lux_read(uint16_t *lux);

/**
 * @brief Read LUX value as float from the Trema LUX sensor
 * @param lux Pointer to store the LUX value as float
 * @return true on success, false on failure
 */
bool trema_lux_read_float(float *lux);

/**
 * @brief Set the stub LUX value for when sensor is not connected
 * @param lux_value The LUX value to use as stub
 */
void trema_lux_set_stub_value(uint16_t lux_value);

/**
 * @brief Check if the sensor is using stub values
 * @return true if using stub values, false if reading from actual sensor
 */
bool trema_lux_is_using_stub_values(void);

#ifdef __cplusplus
}
#endif

#endif // TREMA_LUX_H