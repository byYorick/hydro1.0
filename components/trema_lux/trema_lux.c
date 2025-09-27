#include "trema_lux.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "trema_lux";

// Default I2C address for the DSL sensor
#define DSL_DEFAULT_ADDR 0x21

// Register addresses
#define REG_MODEL               0x04
#define REG_VERSION             0x05
#define REG_ADDRESS             0x06
#define REG_CHIP_ID             0x07
#define REG_DSL_AVERAGING       0x08
#define REG_DSL_FLG             0x10
#define REG_DSL_LUX_L           0x11
#define REG_DSL_LUX_CHANGE      0x13
#define REG_DSL_COEFFICIENT     0x14
#define REG_DSL_PROXIMITY_L     0x15

// Model and chip ID constants
#define DEF_MODEL_DSL           0x06
#define DEF_CHIP_ID_FLASH       0x3C
#define DEF_CHIP_ID_METRO       0xC3

// Flags
#define DSL_GET_CHANGED         0x01

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static uint16_t stub_lux = 800;  // 800 lux (daylight)
static bool dsl_initialized = false;
static uint8_t dsl_address = DSL_DEFAULT_ADDR;

// Forward declarations for iarduino functions
static bool iarduino_dsl_init(void);
static uint16_t iarduino_dsl_get_lux(void);

bool trema_lux_init(void)
{
    // Try to initialize the iarduino DSL sensor
    if (iarduino_dsl_init()) {
        ESP_LOGI(TAG, "Trema LUX sensor (iarduino DSL) initialized successfully");
        use_stub_values = false;
        return true;
    }
    
    // Fall back to the original method
    ESP_LOGD(TAG, "Failed to initialize iarduino DSL sensor, trying original method");
    uint8_t cmd = 0x02; // TREMA_LUX_CMD_READ_LUX
    esp_err_t err = i2c_bus_write(0x21, &cmd, 1);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to communicate with LUX sensor, using stub values");
        use_stub_values = true;
        return true; // Still return true as we'll use stub values
    }
    
    ESP_LOGI(TAG, "LUX sensor initialized successfully with original method");
    use_stub_values = false;
    return true;
}

bool trema_lux_read(uint16_t *lux)
{
    if (lux == NULL) {
        return false;
    }
    
    // If we're using stub values, return the stub value immediately
    if (use_stub_values) {
        *lux = stub_lux;
        return true;
    }
    
    // Try to read from the iarduino DSL sensor
    *lux = iarduino_dsl_get_lux();
    
    // Check if we got a valid reading
    if (*lux > 0) {
        return true;
    }
    
    // Fall back to the original method
    uint8_t cmd = 0x02; // TREMA_LUX_CMD_READ_LUX
    if (i2c_bus_write(0x21, &cmd, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send read command to LUX sensor, using stub values");
        *lux = stub_lux;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    // Wait for conversion
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t raw[2];
    if (i2c_bus_read(0x21, raw, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read LUX value from sensor, using stub values");
        *lux = stub_lux;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    *lux = (raw[0] << 8) | raw[1];
    
    return true;
}

bool trema_lux_read_float(float *lux)
{
    if (lux == NULL) {
        return false;
    }
    
    uint16_t lux_raw;
    if (!trema_lux_read(&lux_raw)) {
        return false;
    }
    
    *lux = (float)lux_raw;
    return true;
}

void trema_lux_set_stub_value(uint16_t lux_value)
{
    stub_lux = lux_value;
}

bool trema_lux_is_using_stub_values(void)
{
    return use_stub_values;
}

// iarduino DSL sensor implementation
static bool iarduino_dsl_init(void)
{
    uint8_t data[4];
    esp_err_t err;
    
    // Read model and chip ID registers to verify sensor presence
    err = i2c_bus_read_reg(dsl_address, REG_MODEL, data, 4);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Failed to communicate with DSL sensor: %s", esp_err_to_name(err));
        return false;
    }
    
    // Check if the sensor is the correct model
    if (data[0] != DEF_MODEL_DSL) {
        ESP_LOGD(TAG, "Incorrect sensor model. Expected: 0x%02X, Got: 0x%02X", DEF_MODEL_DSL, data[0]);
        return false;
    }
    
    // Check chip ID
    if (data[3] != DEF_CHIP_ID_FLASH && data[3] != DEF_CHIP_ID_METRO) {
        ESP_LOGD(TAG, "Incorrect chip ID. Expected: 0x%02X or 0x%02X, Got: 0x%02X", 
                 DEF_CHIP_ID_FLASH, DEF_CHIP_ID_METRO, data[3]);
        return false;
    }
    
    ESP_LOGI(TAG, "DSL sensor initialized successfully. Model: 0x%02X, Version: 0x%02X, Chip ID: 0x%02X", 
             data[0], data[1], data[3]);
    
    dsl_initialized = true;
    return true;
}

static uint16_t iarduino_dsl_get_lux(void)
{
    if (!dsl_initialized) {
        return 0;
    }
    
    uint8_t data[2];
    esp_err_t err;
    
    // Read lux value (2 bytes) from REG_DSL_LUX_L register
    err = i2c_bus_read_reg(dsl_address, REG_DSL_LUX_L, data, 2);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Failed to read lux value: %s", esp_err_to_name(err));
        return 0;
    }
    
    return (((uint16_t)data[1]) << 8) | data[0];
}
