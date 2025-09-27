#ifndef CCS811_H
#define CCS811_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// I2C address
#define CCS811_ADDR                 0x5A

// Hardware ID code
#define CCS811_HW_ID_CODE           0x81

// Registers
#define CCS811_STATUS               0x00
#define CCS811_MEAS_MODE            0x01
#define CCS811_ALG_RESULT_DATA      0x02
#define CCS811_RAW_DATA             0x03
#define CCS811_ENV_DATA             0x05
#define CCS811_NTC                  0x06
#define CCS811_THRESHOLDS           0x10
#define CCS811_BASELINE             0x11
#define CCS811_HW_ID                0x20
#define CCS811_HW_VERSION           0x21
#define CCS811_FW_BOOT_VERSION      0x23
#define CCS811_FW_APP_VERSION       0x24
#define CCS811_ERROR_ID             0xE0
#define CCS811_SW_RESET             0xFF

// Bootloader registers
#define CCS811_BOOTLOADER_APP_ERASE     0xF1
#define CCS811_BOOTLOADER_APP_DATA      0xF2
#define CCS811_BOOTLOADER_APP_VERIFY    0xF3
#define CCS811_BOOTLOADER_APP_START     0xF4

// Drive modes
#define CCS811_DRIVE_MODE_IDLE      0x00
#define CCS811_DRIVE_MODE_1SEC      0x01
#define CCS811_DRIVE_MODE_10SEC     0x02
#define CCS811_DRIVE_MODE_60SEC     0x03
#define CCS811_DRIVE_MODE_250MS     0x04

/**
 * @brief Initialize the CCS811 sensor
 * @return true on success, false on failure
 */
bool ccs811_init(void);

/**
 * @brief Check if new data is available
 * @return true if data is ready, false otherwise
 */
bool ccs811_data_ready(void);

/**
 * @brief Read eCO2 and TVOC values from the CCS811 sensor
 * @param eco2 Pointer to store the eCO2 value (ppm)
 * @param tvoc Pointer to store the TVOC value (ppb)
 * @return true on success, false on failure
 */
bool ccs811_read_data(float *eco2, float *tvoc);

/**
 * @brief Read only eCO2 value from the CCS811 sensor
 * @param eco2 Pointer to store the eCO2 value (ppm)
 * @return true on success, false on failure
 */
bool ccs811_read_eco2(float *eco2);

/**
 * @brief Read only TVOC value from the CCS811 sensor
 * @param tvoc Pointer to store the TVOC value (ppb)
 * @return true on success, false on failure
 */
bool ccs811_read_tvoc(float *tvoc);

/**
 * @brief Set the drive mode for periodic measurements
 * @param mode One of CCS811_DRIVE_MODE_* constants
 */
void ccs811_set_drive_mode(uint8_t mode);

/**
 * @brief Enable interrupt when new data is ready
 */
void ccs811_enable_interrupt(void);

/**
 * @brief Disable interrupt when new data is ready
 */
void ccs811_disable_interrupt(void);

/**
 * @brief Check if the sensor is in error state
 * @return true if error, false otherwise
 */
bool ccs811_check_error(void);

/**
 * @brief Software reset the sensor
 */
void ccs811_software_reset(void);

/**
 * @brief Set environmental data for compensation
 * @param humidity Relative humidity in % (0-100)
 * @param temperature Temperature in Celsius
 */
void ccs811_set_environmental_data(uint8_t humidity, float temperature);

#ifdef __cplusplus
}
#endif

#endif // CCS811_H