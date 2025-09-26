#ifndef TREMA_LUX_H
#define TREMA_LUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Read LUX value from the Trema LUX sensor
 * @param lux Pointer to store the LUX value
 * @return true on success, false on failure
 */
bool trema_lux_read(uint16_t *lux);

#ifdef __cplusplus
}
#endif

#endif // TREMA_LUX_H