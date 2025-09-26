#ifndef TREMA_PH_H
#define TREMA_PH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Read pH value from the Trema pH sensor
 * @param ph Pointer to store the pH value
 * @return true on success, false on failure
 */
bool trema_ph_read(float *ph);

#ifdef __cplusplus
}
#endif

#endif // TREMA_PH_H