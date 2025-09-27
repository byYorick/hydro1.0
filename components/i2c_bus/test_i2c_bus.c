#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "test_i2c_bus";

void test_i2c_bus(void)
{
    // Initialize I2C bus
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "I2C bus initialized successfully");
    
    // Test write operation
    uint8_t test_data[] = {0x01, 0x02, 0x03};
    err = i2c_bus_write(0x21, test_data, sizeof(test_data));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to I2C device: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully wrote to I2C device");
    }
    
    // Test read operation
    uint8_t read_data[2];
    err = i2c_bus_read(0x21, read_data, sizeof(read_data));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from I2C device: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully read from I2C device: 0x%02X 0x%02X", read_data[0], read_data[1]);
    }
    
    // Test read register operation
    uint8_t reg_data[1];
    err = i2c_bus_read_reg(0x21, 0x01, reg_data, sizeof(reg_data));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read register from I2C device: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully read register from I2C device: 0x%02X", reg_data[0]);
    }
}