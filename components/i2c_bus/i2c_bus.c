#include "i2c_bus.h"

SemaphoreHandle_t i2c_bus_mutex = NULL;

esp_err_t i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    i2c_bus_mutex = xSemaphoreCreateMutex();
    return ESP_OK;
}

esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len)
{
    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, data, len, pdMS_TO_TICKS(1000));
    xSemaphoreGive(i2c_bus_mutex);
    return err;
}

esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t *data, size_t len)
{
    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, dev_addr, data, len, pdMS_TO_TICKS(1000));
    xSemaphoreGive(i2c_bus_mutex);
    return err;
}