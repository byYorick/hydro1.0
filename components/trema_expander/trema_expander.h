#ifndef TREMA_EXPANDER_H
#define TREMA_EXPANDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Default I2C address for the Expander
#define TREMA_EXPANDER_ADDR 0x16

// Register addresses
#define REG_EXP_DIRECTION    0x10  // Direction register (0=input, 1=output)
#define REG_EXP_TYPE         0x11  // Type register (0=digital, 1=analog)
#define REG_EXP_PULL_UP      0x12  // Pull-up register
#define REG_EXP_PULL_DOWN    0x13  // Pull-down register
#define REG_EXP_OUT_MODE     0x14  // Output mode register
#define REG_EXP_DIGITAL      0x15  // Digital value register
#define REG_EXP_WRITE_HIGH   0x16  // Write high register
#define REG_EXP_WRITE_LOW    0x17  // Write low register
#define REG_EXP_ANALOG       0x18  // Analog value register (starts at byte 0)

// Pin mode constants
#define INPUT                0
#define OUTPUT               1
#define DIGITAL              2
#define ANALOG               3
#define SERVO                4
#define OUT_PUSH_PULL        5
#define OUT_OPEN_DRAIN       6
#define PULL_UP              7
#define PULL_DOWN            8
#define PULL_NO              0xFF
#define ALL_PIN              0xFF

/**
 * @brief Initialize the Trema I2C Expander
 * @return true on success, false on failure
 */
bool trema_expander_init(void);

/**
 * @brief Configure pin mode
 * @param pin Pin number (0-7)
 * @param mode Pin mode (INPUT, OUTPUT, etc.)
 * @param type Pin type (DIGITAL, ANALOG, etc.)
 */
void trema_expander_pin_mode(uint8_t pin, uint8_t mode, uint8_t type);

/**
 * @brief Set pull-up/pull-down resistors
 * @param pin Pin number (0-7)
 * @param pull Pull type (PULL_UP, PULL_DOWN, PULL_NO)
 */
void trema_expander_pin_pull(uint8_t pin, uint8_t pull);

/**
 * @brief Set output scheme
 * @param pin Pin number (0-7)
 * @param scheme Output scheme (OUT_PUSH_PULL, OUT_OPEN_DRAIN)
 */
void trema_expander_pin_out_scheme(uint8_t pin, uint8_t scheme);

/**
 * @brief Write digital value to pin
 * @param pin Pin number (0-7)
 * @param value Digital value (0=LOW, 1=HIGH)
 */
void trema_expander_digital_write(uint8_t pin, uint8_t value);

/**
 * @brief Read digital value from pin
 * @param pin Pin number (0-7)
 * @return Digital value (0=LOW, 1=HIGH)
 */
uint8_t trema_expander_digital_read(uint8_t pin);

/**
 * @brief Write analog value to pin (PWM)
 * @param pin Pin number (0-7)
 * @param value Analog value (0-4095)
 */
void trema_expander_analog_write(uint8_t pin, uint16_t value);

/**
 * @brief Read analog value from pin
 * @param pin Pin number (0-7)
 * @return Analog value (0-4095)
 */
uint16_t trema_expander_analog_read(uint8_t pin);

/**
 * @brief Set PWM frequency
 * @param frequency PWM frequency in Hz
 */
void trema_expander_freq_pwm(uint16_t frequency);

/**
 * @brief Check if we're using stub values (expander not connected)
 * @return true if using stub values, false if expander is connected
 */
bool trema_expander_is_using_stub_values(void);

#ifdef __cplusplus
}
#endif

#endif // TREMA_EXPANDER_H