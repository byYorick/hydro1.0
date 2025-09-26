#ifndef PERISTALTIC_PUMP_H
#define PERISTALTIC_PUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize a peristaltic pump
 * @param ia IA pin
 * @param ib IB pin
 */
void pump_init(int ia, int ib);

/**
 * @brief Run a peristaltic pump for specified milliseconds
 * @param ia IA pin
 * @param ib IB pin
 * @param ms Duration in milliseconds
 */
void pump_run_ms(int ia, int ib, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif // PERISTALTIC_PUMP_H