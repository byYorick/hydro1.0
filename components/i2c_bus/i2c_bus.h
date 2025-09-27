#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <driver/i2c_master.h>

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 17
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_FREQ_HZ 100000

// Handle for the I2C master bus
extern i2c_master_bus_handle_t i2c_bus_handle;

// Initialize the I2C bus
esp_err_t i2c_bus_init(void);

// Write data to an I2C device
esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len);

// Read data from an I2C device
esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t *data, size_t len);

// Read data from a specific register of an I2C device
esp_err_t i2c_bus_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len);