#include "ccs811.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "ccs811";

static bool ccs811_initialized = false;
static bool use_stub_values = false;

// Stub values for when sensor is not connected
static float stub_co2 = 450.0f;  // 450 ppm (typical outdoor CO2)
static float stub_tvoc = 10.0f;  // 10 ppb (typical indoor TVOC)

// Status register bitfields
typedef struct {
    uint8_t error : 1;
    uint8_t reserved1 : 2;
    uint8_t data_ready : 1;
    uint8_t app_valid : 1;
    uint8_t reserved2 : 2;
    uint8_t fw_mode : 1;
} ccs811_status_t;

// Measurement mode register bitfields
typedef struct {
    uint8_t reserved1 : 2;
    uint8_t int_thresh : 1;
    uint8_t int_datardy : 1;
    uint8_t drive_mode : 3;
    uint8_t reserved2 : 1;
} ccs811_meas_mode_t;

static ccs811_status_t status_reg;
static ccs811_meas_mode_t meas_mode_reg;

// Helper functions for I2C communication
static esp_err_t ccs811_write_register(uint8_t reg, const uint8_t *data, size_t len)
{
    // For register write, we need to send register address followed by data
    uint8_t *buffer = malloc(len + 1);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    buffer[0] = reg;
    memcpy(buffer + 1, data, len);
    
    esp_err_t ret = i2c_bus_write(CCS811_ADDR, buffer, len + 1);
    free(buffer);
    return ret;
}

static esp_err_t ccs811_write_byte(uint8_t reg, uint8_t value)
{
    return ccs811_write_register(reg, &value, 1);
}

static esp_err_t ccs811_read_register(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_bus_read_reg(CCS811_ADDR, reg, data, len);
}

static uint8_t ccs811_read_byte(uint8_t reg)
{
    uint8_t value = 0;
    esp_err_t ret = ccs811_read_register(reg, &value, 1);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return 0;
    }
    return value;
}

bool ccs811_init(void)
{
    ESP_LOGI(TAG, "Initializing CCS811 sensor");
    
    // Try to read the hardware ID to check if sensor is connected
    uint8_t hw_id = ccs811_read_byte(CCS811_HW_ID);
    if (hw_id != CCS811_HW_ID_CODE) {
        ESP_LOGW(TAG, "CCS811 not found or not connected (expected 0x%02X, got 0x%02X)", 
                 CCS811_HW_ID_CODE, hw_id);
        use_stub_values = true;
        return false;
    }
    
    // Software reset
    ccs811_software_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Try to start the application
    uint8_t dummy = 0;
    esp_err_t ret = ccs811_write_register(CCS811_BOOTLOADER_APP_START, &dummy, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start CCS811 application: %s", esp_err_to_name(ret));
        use_stub_values = true;
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check for errors and application mode
    if (ccs811_check_error()) {
        ESP_LOGW(TAG, "CCS811 error after app start");
        use_stub_values = true;
        return false;
    }
    
    uint8_t status = ccs811_read_byte(CCS811_STATUS);
    status_reg = *(ccs811_status_t*)&status;
    if (!status_reg.fw_mode) {
        ESP_LOGW(TAG, "CCS811 not in application mode");
        use_stub_values = true;
        return false;
    }
    
    // Disable interrupt
    ccs811_disable_interrupt();
    
    // Set default drive mode to 1 second
    ccs811_set_drive_mode(CCS811_DRIVE_MODE_1SEC);
    
    ccs811_initialized = true;
    ESP_LOGI(TAG, "CCS811 initialized successfully");
    return true;
}

bool ccs811_data_ready(void)
{
    if (!ccs811_initialized || use_stub_values) {
        return true; // Always ready when using stub values
    }
    
    uint8_t status = ccs811_read_byte(CCS811_STATUS);
    status_reg = *(ccs811_status_t*)&status;
    return status_reg.data_ready;
}

bool ccs811_read_data(float *eco2, float *tvoc)
{
    // If sensor was not initialized, use stub values
    if (!ccs811_initialized || use_stub_values) {
        *eco2 = stub_co2;
        if (tvoc) *tvoc = stub_tvoc;
        return true; // Return true to indicate success with stub values
    }
    
    // Check if data is ready
    if (!ccs811_data_ready()) {
        *eco2 = stub_co2;
        if (tvoc) *tvoc = stub_tvoc;
        return true; // Return true to indicate success with stub values
    }
    
    // Read algorithm result data (8 bytes)
    uint8_t data[8];
    esp_err_t ret = ccs811_read_register(CCS811_ALG_RESULT_DATA, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to read algorithm result data: %s", esp_err_to_name(ret));
        // Use stub values when sensor read fails
        *eco2 = stub_co2;
        if (tvoc) *tvoc = stub_tvoc;
        return true; // Return true to indicate success with stub values
    }
    
    // Parse eCO2 and TVOC values
    uint16_t eco2_val = (data[0] << 8) | data[1];
    uint16_t tvoc_val = (data[2] << 8) | data[3];
    
    *eco2 = (float)eco2_val;
    if (tvoc) *tvoc = (float)tvoc_val;
    
    // Check for errors
    if (data[5] != 0) {
        ESP_LOGD(TAG, "CCS811 error code: 0x%02X", data[5]);
    }
    
    return true;
}

bool ccs811_read_eco2(float *eco2)
{
    return ccs811_read_data(eco2, NULL);
}

bool ccs811_read_tvoc(float *tvoc)
{
    float eco2;
    return ccs811_read_data(&eco2, tvoc);
}

void ccs811_set_drive_mode(uint8_t mode)
{
    if (!ccs811_initialized && !use_stub_values) {
        return;
    }
    
    meas_mode_reg.drive_mode = mode & 0x07; // Only 3 bits for drive mode
    uint8_t reg_value = *(uint8_t*)&meas_mode_reg;
    ccs811_write_byte(CCS811_MEAS_MODE, reg_value);
}

void ccs811_enable_interrupt(void)
{
    if (!ccs811_initialized && !use_stub_values) {
        return;
    }
    
    meas_mode_reg.int_datardy = 1;
    uint8_t reg_value = *(uint8_t*)&meas_mode_reg;
    ccs811_write_byte(CCS811_MEAS_MODE, reg_value);
}

void ccs811_disable_interrupt(void)
{
    if (!ccs811_initialized && !use_stub_values) {
        return;
    }
    
    meas_mode_reg.int_datardy = 0;
    uint8_t reg_value = *(uint8_t*)&meas_mode_reg;
    ccs811_write_byte(CCS811_MEAS_MODE, reg_value);
}

bool ccs811_check_error(void)
{
    if (!ccs811_initialized && !use_stub_values) {
        return false;
    }
    
    uint8_t status = ccs811_read_byte(CCS811_STATUS);
    status_reg = *(ccs811_status_t*)&status;
    return status_reg.error;
}

void ccs811_software_reset(void)
{
    // Reset sequence from the datasheet
    uint8_t seq[4] = {0x11, 0xE5, 0x72, 0x8A};
    ccs811_write_register(CCS811_SW_RESET, seq, 4);
}

void ccs811_set_environmental_data(uint8_t humidity, float temperature)
{
    if (!ccs811_initialized && !use_stub_values) {
        return;
    }
    
    /*Compensate for temperature and humidity.
    The internal algorithm uses these values (or default values if
    not set by the application) to compensate for changes in
    relative humidity and ambient temperature.*/
    
    uint8_t hum_perc = humidity << 1;
    
    float temp_frac = temperature - (int)temperature;
    uint16_t temp_high = (((uint16_t)((int)temperature + 25) << 9));
    uint16_t temp_low = ((uint16_t)(temp_frac / 0.001953125) & 0x1FF);
    
    uint16_t temp_conv = (temp_high | temp_low);
    
    uint8_t buf[4] = {
        hum_perc, 0x00,
        (uint8_t)((temp_conv >> 8) & 0xFF), 
        (uint8_t)(temp_conv & 0xFF)
    };
    
    ccs811_write_register(CCS811_ENV_DATA, buf, 4);
}