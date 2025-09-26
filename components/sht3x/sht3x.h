#ifndef SHT3X_H
#define SHT3X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Read temperature and humidity from the SHT3X sensor
 * @param temp Pointer to store the temperature value
 * @param hum Pointer to store the humidity value
 * @return true on success, false on failure
 */
bool sht3x_read(float *temp, float *hum);

#ifdef __cplusplus
}
#endif

#endif // SHT3X_H