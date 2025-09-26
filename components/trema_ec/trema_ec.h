#ifndef TREMA_EC_H
#define TREMA_EC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Read EC value from the Trema EC sensor
 * @param ec Pointer to store the EC value
 * @return true on success, false on failure
 */
bool trema_ec_read(float *ec);

#ifdef __cplusplus
}
#endif

#endif // TREMA_EC_H