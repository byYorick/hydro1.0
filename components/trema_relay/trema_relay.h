#ifndef TREMA_RELAY_H
#define TREMA_RELAY_H

/**
 * @file trema_relay.h
 * @brief Trema I2C Relay Component for ESP-IDF
 * 
 * This component provides an interface to control the Trema I2C Relay module.
 * It supports both 2-channel and 4-channel relay modules with auto-switching capability.
 * 
 * ## Usage
 * 
 * 1. Include this header in your source file:
 *    ```c
 *    #include "trema_relay.h"
 *    ```
 * 
 * 2. Initialize the relay:
 *    ```c
 *    if (trema_relay_init()) {
 *        // Relay initialized successfully
 *    }
 *    ```
 * 
 * 3. Control relay channels:
 *    ```c
 *    trema_relay_digital_write(0, HIGH);  // Turn on channel 0
 *    trema_relay_digital_write(0, LOW);   // Turn off channel 0
 *    ```
 * 
 * 4. Use auto-switching feature:
 *    ```c
 *    trema_relay_auto_switch(true);   // Start auto-switching
 *    trema_relay_auto_switch(false);  // Stop auto-switching
 *    ```
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Default I2C address for the Relay
#define TREMA_RELAY_ADDR 0x17

// Register addresses
#define REG_REL_DIGITAL_ALL     0x12  // Digital value register for all channels
#define REG_REL_DIGITAL_ONE     0x13  // Digital value register for one channel
#define REG_REL_WDT             0x30  // Watchdog timer register

// Constants
#define LOW                     0x00
#define HIGH                    0x01
#define ALL_CHANNEL             0xFF
#define CURRENT_DISABLE         0x00
#define CURRENT_LIMIT           0xFF

/**
 * @brief Initialize the Trema I2C Relay
 * @return true on success, false on failure
 */
bool trema_relay_init(void);

/**
 * @brief Write digital value to relay channel
 * @param channel Relay channel number (0-3 for 4-channel relay, 0-1 for 2-channel relay)
 * @param value Digital value (0=LOW, 1=HIGH)
 */
void trema_relay_digital_write(uint8_t channel, uint8_t value);

/**
 * @brief Read digital value from relay channel
 * @param channel Relay channel number (0-3 for 4-channel relay, 0-1 for 2-channel relay)
 * @return Digital value (0=LOW, 1=HIGH)
 */
uint8_t trema_relay_digital_read(uint8_t channel);

/**
 * @brief Enable watchdog timer
 * @param timeout Timeout in seconds (1-254)
 * @return true on success, false on failure
 */
bool trema_relay_enable_wdt(uint8_t timeout);

/**
 * @brief Disable watchdog timer
 * @return true on success, false on failure
 */
bool trema_relay_disable_wdt(void);

/**
 * @brief Reset (restart) watchdog timer
 * @return true on success, false on failure
 */
bool trema_relay_reset_wdt(void);

/**
 * @brief Get watchdog timer state
 * @return true if WDT is enabled, false if disabled
 */
bool trema_relay_get_state_wdt(void);

/**
 * @brief Check if we're using stub values (relay not connected)
 * @return true if using stub values, false if relay is connected
 */
bool trema_relay_is_using_stub_values(void);

/**
 * @brief Automatically switch relay channels 1-4 every 2 seconds
 * @param enable true to start auto-switching, false to stop
 */
void trema_relay_auto_switch(bool enable);

#ifdef __cplusplus
}
#endif

#endif // TREMA_RELAY_H