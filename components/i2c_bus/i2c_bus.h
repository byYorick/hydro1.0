#pragma once
#include <driver/i2c.h>
#include "freertos/semphr.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 17
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_FREQ_HZ 100000

extern SemaphoreHandle_t i2c_bus_mutex;
esp_err_t i2c_bus_init(void);
esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len);
esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t *data, size_t len);