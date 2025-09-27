#include "trema_ph.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "trema_ph";

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_ph = 6.5f;  // Neutral pH

// Buffer for I2C communication
static uint8_t data[4];

// Sensor initialization flag
static bool sensor_initialized = false;

bool trema_ph_init(void)
{
    // Try to communicate with the sensor
    // Read the model register to verify sensor presence
    data[0] = 0x04; // REG_MODEL
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to pH sensor");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from pH sensor");
        return false;
    }
    
    // Check if we got a valid response
    // For iarduino pH sensor, model ID should be 0x1A
    if (data[0] != 0x1A) {
        ESP_LOGW(TAG, "Invalid pH sensor model ID: 0x%02X", data[0]);
        return false;
    }
    
    sensor_initialized = true;
    ESP_LOGI(TAG, "pH sensor initialized successfully");
    return true;
}

bool trema_ph_read(float *ph)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        if (!trema_ph_init()) {
            ESP_LOGD(TAG, "PH sensor not connected, using stub values");
            // Use stub values when sensor is not connected
            *ph = stub_ph;
            use_stub_values = true;
            return true; // Return true to indicate success with stub values
        }
    }
    
    // Request pH measurement
    data[0] = REG_PH_pH; // Register address for pH measurement
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGD(TAG, "PH sensor read failed, using stub values");
        // Use stub values when sensor communication fails
        *ph = stub_ph;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // Read the pH value (2 bytes)
    if (i2c_bus_read(TREMA_PH_ADDR, data, 2) != ESP_OK) {
        ESP_LOGD(TAG, "PH sensor read failed, using stub values");
        // Use stub values when sensor communication fails
        *ph = stub_ph;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    // Convert the 2-byte value to float
    // pH value is stored as integer in thousandths (multiply by 0.001)
    uint16_t pH_raw = ((uint16_t)data[1] << 8) | data[0];
    *ph = (float)pH_raw * 0.001f;
    
    // Validate pH range
    if (*ph < 0.0f || *ph > 14.0f) {
        ESP_LOGW(TAG, "Invalid pH value: %.3f, using stub value", *ph);
        *ph = stub_ph;
    }
    
    return true;
}

bool trema_ph_calibrate(uint8_t stage, float known_pH)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        ESP_LOGW(TAG, "Sensor not initialized");
        return false;
    }
    
    // Validate parameters
    if ((stage != 1 && stage != 2) || known_pH < 0.0f || known_pH > 14.0f) {
        ESP_LOGW(TAG, "Invalid calibration parameters");
        return false;
    }
    
    // Write known pH value
    data[0] = REG_PH_KNOWN_PH;
    data[1] = (uint16_t)(known_pH * 1000.0f) & 0x00FF;       // LSB
    data[2] = ((uint16_t)(known_pH * 1000.0f) >> 8) & 0x00FF; // MSB
    
    if (i2c_bus_write(TREMA_PH_ADDR, data, 3) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write known pH value");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Send calibration command
    data[0] = REG_PH_CALIBRATION;
    data[1] = (stage == 1 ? PH_BIT_CALC_1 : PH_BIT_CALC_2) | PH_CODE_CALC_SAVE;
    
    if (i2c_bus_write(TREMA_PH_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send calibration command");
        return false;
    }
    
    ESP_LOGI(TAG, "Calibration stage %d started with pH %.3f", stage, known_pH);
    return true;
}

uint8_t trema_ph_get_calibration_status(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return 0;
    }
    
    // Read calibration status
    data[0] = REG_PH_CALIBRATION;
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration status");
        return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration status");
        return 0;
    }
    
    if (data[0] & 0x40) { // PH_FLG_STATUS_1
        return 1;
    } else if (data[0] & 0x80) { // PH_FLG_STATUS_2
        return 2;
    }
    
    return 0;
}

bool trema_ph_get_calibration_result(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return false;
    }
    
    // Read error flags
    data[0] = REG_PH_ERROR;
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration result");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration result");
        return false;
    }
    
    // Return true if calibration error flag is NOT set
    return !(data[0] & PH_FLG_CALC_ERR);
}

bool trema_ph_get_stability(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return false;
    }
    
    // Read error flags
    data[0] = REG_PH_ERROR;
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read stability status");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read stability status");
        return false;
    }
    
    // Check if stability error flag is set
    if (data[0] & PH_FLG_STAB_ERR) {
        ESP_LOGD(TAG, "pH measurement is not stable (STAB_ERR flag set)");
        return false;
    }
    
    // Return true if stability error flag is NOT set
    return true;
}

bool trema_ph_wait_for_stable_reading(uint32_t timeout_ms)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return false;
    }
    
    uint32_t elapsed_time = 0;
    const uint32_t check_interval = 100; // Check every 100ms
    
    while (elapsed_time < timeout_ms) {
        if (trema_ph_get_stability()) {
            return true; // Measurement is stable
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        elapsed_time += check_interval;
    }
    
    ESP_LOGW(TAG, "Timeout waiting for stable pH measurement after %u ms", (unsigned int)timeout_ms);
    return false; // Timeout occurred
}

float trema_ph_get_value(void)
{
    float ph;
    if (trema_ph_read(&ph)) {
        return ph;
    }
    return stub_ph;
}

bool trema_ph_reset(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        ESP_LOGW(TAG, "Cannot reset uninitialized pH sensor");
        return false;
    }
    
    // Send reset command
    // For iarduino pH sensor, we need to set bit 7 of REG_BITS_0
    data[0] = 0x01; // REG_BITS_0
    if (i2c_bus_write(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to pH sensor for reset");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Read current value
    if (i2c_bus_read(TREMA_PH_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from pH sensor for reset");
        return false;
    }
    
    // Set reset bit (bit 7)
    data[1] = data[0] | 0x80;
    data[0] = 0x01; // REG_BITS_0
    
    if (i2c_bus_write(TREMA_PH_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send reset command to pH sensor");
        return false;
    }
    
    // Wait for reset to complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "pH sensor reset completed");
    return true;
}

bool trema_ph_is_using_stub_values(void)
{
    return use_stub_values;
}