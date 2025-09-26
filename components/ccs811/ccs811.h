#ifndef CCS811_H
#define CCS811_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the CCS811 sensor
 * @return true on success, false on failure
 */
bool ccs811_init(void);

/**
 * @brief Read eCO2 value from the CCS811 sensor
 * @param eco2 Pointer to store the eCO2 value
 * @return true on success, false on failure
 */
bool ccs811_read(float *eco2);

#ifdef __cplusplus
}
#endif

#endif // CCS811_H