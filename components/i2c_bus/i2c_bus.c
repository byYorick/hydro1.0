#include "i2c_bus.h"
#include "error_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "i2c_bus";

i2c_master_bus_handle_t i2c_bus_handle = NULL;
SemaphoreHandle_t i2c_bus_mutex = NULL;

// Счетчик последовательных ошибок для каждого устройства (защита от спама)
#define MAX_I2C_DEVICES 8
static struct {
    uint8_t addr;
    uint32_t error_count;
    uint32_t last_error_tick;
} i2c_error_tracker[MAX_I2C_DEVICES] = {0};

// Проверка, нужно ли логировать ошибку (не спамить)
static bool should_log_i2c_error(uint8_t dev_addr) {
    uint32_t now = xTaskGetTickCount();
    
    for (int i = 0; i < MAX_I2C_DEVICES; i++) {
        if (i2c_error_tracker[i].addr == dev_addr || i2c_error_tracker[i].addr == 0) {
            if (i2c_error_tracker[i].addr == 0) {
                i2c_error_tracker[i].addr = dev_addr;
            }
            
            // Если прошло меньше 10 секунд (10000 тиков) с последней ошибки
            uint32_t elapsed_ticks = now - i2c_error_tracker[i].last_error_tick;
            if (elapsed_ticks < pdMS_TO_TICKS(10000)) {
                i2c_error_tracker[i].error_count++;
                
                // Логируем только каждую 10-ю ошибку
                if (i2c_error_tracker[i].error_count % 10 == 0) {
                    i2c_error_tracker[i].last_error_tick = now;
                    return true;
                }
                return false;
            } else {
                // Прошло больше 10 секунд - сбрасываем счетчик
                i2c_error_tracker[i].error_count = 1;
                i2c_error_tracker[i].last_error_tick = now;
                return true;
            }
        }
    }
    return true; // По умолчанию логируем
}

esp_err_t i2c_bus_init(void)
{
    // Configure the I2C master bus
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    // Create the I2C master bus
    esp_err_t err = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
    if (err != ESP_OK) {
        ERROR_CRITICAL(ERROR_CATEGORY_I2C, err, TAG, "Не удалось создать шину I2C");
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(err));
        return err;
    }

    // Create mutex for thread safety
    i2c_bus_mutex = xSemaphoreCreateMutex();
    if (i2c_bus_mutex == NULL) {
        ERROR_CRITICAL(ERROR_CATEGORY_I2C, ESP_ERR_NO_MEM, TAG, "Не удалось создать мьютекс I2C");
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "I2C bus initialized successfully on SCL=%d, SDA=%d, Freq=%d Hz", 
             I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO, I2C_MASTER_FREQ_HZ);
    return ESP_OK;
}

esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len)
{
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters: data=%p, len=%d", data, len);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ERROR_WARN(ERROR_CATEGORY_I2C, TAG, "Таймаут ожидания мьютекса I2C");
        ESP_LOGW(TAG, "Timeout waiting for I2C mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Create device handle for this transaction
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ERROR_CHECK_I2C(err, TAG, "Не удалось добавить устройство 0x%02X", dev_addr);
        ESP_LOGW(TAG, "Failed to add device 0x%02X to bus: %s", dev_addr, esp_err_to_name(err));
        xSemaphoreGive(i2c_bus_mutex);
        return err;
    }

    // Perform the write transaction with timeout
    err = i2c_master_transmit(dev_handle, data, len, 1000); // 1 second timeout
    if (err != ESP_OK) {
        // Логируем ошибку только если нужно (защита от спама)
        if (should_log_i2c_error(dev_addr)) {
            ERROR_CHECK_I2C(err, TAG, "Ошибка записи на устройство 0x%02X", dev_addr);
            ESP_LOGW(TAG, "Failed to write to device 0x%02X: %s", dev_addr, esp_err_to_name(err));
        }
    }

    // Remove the device handle
    esp_err_t rm_err = i2c_master_bus_rm_device(dev_handle);
    if (rm_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove device 0x%02X from bus: %s", dev_addr, esp_err_to_name(rm_err));
    }
    
    xSemaphoreGive(i2c_bus_mutex);
    return err;
}

esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t *data, size_t len)
{
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters: data=%p, len=%d", data, len);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout waiting for I2C mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Create device handle for this transaction
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add device 0x%02X to bus: %s", dev_addr, esp_err_to_name(err));
        xSemaphoreGive(i2c_bus_mutex);
        return err;
    }

    // Perform the read transaction with timeout
    err = i2c_master_receive(dev_handle, data, len, 1000); // 1 second timeout
    if (err != ESP_OK) {
        // Логируем ошибку только если нужно (защита от спама)
        if (should_log_i2c_error(dev_addr)) {
            ESP_LOGW(TAG, "Failed to read from device 0x%02X: %s", dev_addr, esp_err_to_name(err));
        }
    }

    // Remove the device handle
    esp_err_t rm_err = i2c_master_bus_rm_device(dev_handle);
    if (rm_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove device 0x%02X from bus: %s", dev_addr, esp_err_to_name(rm_err));
    }
    
    xSemaphoreGive(i2c_bus_mutex);
    return err;
}

esp_err_t i2c_bus_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters: data=%p, len=%d", data, len);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout waiting for I2C mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Create device handle for this transaction
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add device 0x%02X to bus: %s", dev_addr, esp_err_to_name(err));
        xSemaphoreGive(i2c_bus_mutex);
        return err;
    }

    // Perform the write-read transaction (write register address, then read data) with timeout
    err = i2c_master_transmit_receive(dev_handle, &reg, 1, data, len, 1000); // 1 second timeout
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read register 0x%02X from device 0x%02X: %s", reg, dev_addr, esp_err_to_name(err));
    }

    // Remove the device handle
    esp_err_t rm_err = i2c_master_bus_rm_device(dev_handle);
    if (rm_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove device 0x%02X from bus: %s", dev_addr, esp_err_to_name(rm_err));
    }
    
    xSemaphoreGive(i2c_bus_mutex);
    return err;
}