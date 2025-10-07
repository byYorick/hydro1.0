#include "trema_ec.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "trema_ec";

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_ec = 1.2f;  // 1.2 mS/cm
static uint16_t stub_tds = 800;  // 800 ppm

// Buffer for I2C communication
static uint8_t data[4];

// Sensor initialization flag
static bool sensor_initialized = false;

bool trema_ec_init(void)
{
    ESP_LOGI(TAG, "Initializing EC sensor at address 0x%02X...", TREMA_EC_ADDR);
    
    // Try to communicate with the sensor
    // Read the model register to verify sensor presence
    data[0] = 0x04; // REG_MODEL
    if (i2c_bus_write(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to EC sensor (addr=0x%02X, reg=0x04)", TREMA_EC_ADDR);
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from EC sensor (addr=0x%02X)", TREMA_EC_ADDR);
        return false;
    }
    
    ESP_LOGI(TAG, "EC sensor model ID read: 0x%02X (expected 0x19)", data[0]);
    
    // Check if we got a valid response
    // For iarduino TDS sensor, model ID should be 0x19
    if (data[0] != 0x19) {
        ESP_LOGW(TAG, "Invalid EC sensor model ID: 0x%02X (expected 0x19)", data[0]);
        ESP_LOGI(TAG, "Sensor may not be connected or has different model ID");
        return false;
    }
    
    sensor_initialized = true;
    ESP_LOGI(TAG, "✓ EC sensor initialized successfully (model 0x19)");
    return true;
}

bool trema_ec_read(float *ec)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        if (!trema_ec_init()) {
            ESP_LOGD(TAG, "EC sensor not connected, using stub values");
            // Use stub values when sensor is not connected
            *ec = stub_ec;
            use_stub_values = true;
            return true; // Return true to indicate success with stub values
        }
    }
    
    // Request EC measurement
    data[0] = REG_TDS_EC; // Register address for EC measurement (0x22)
    if (i2c_bus_write(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "EC sensor write failed (addr=0x%02X, reg=0x%02X), using stub", TREMA_EC_ADDR, REG_TDS_EC);
        // Use stub values when sensor communication fails
        *ec = stub_ec;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    vTaskDelay(pdMS_TO_TICKS(50)); // Увеличена задержка для стабильности чтения
    
    // Read the EC value (2 bytes)
    if (i2c_bus_read(TREMA_EC_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "EC sensor read failed, using stub values");
        // Use stub values when sensor communication fails
        *ec = stub_ec;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    // Convert the 2-byte value to float
    // EC value is stored as integer in thousandths (multiply by 0.001)
    // The sensor returns value in mS/cm (milliSiemens per centimeter)
    uint16_t ec_raw = ((uint16_t)data[1] << 8) | data[0];
    
    ESP_LOGI(TAG, "EC raw bytes: [0]=0x%02X [1]=0x%02X, raw_value=%u", data[0], data[1], ec_raw);
    
    *ec = (float)ec_raw * 0.001f;
    
    ESP_LOGI(TAG, "EC converted: %.3f mS/cm", *ec);
    
    // Validate EC range (по спецификации: 0–20,000 мСм/см = 0-20.0 mS/cm)
    if (*ec < 0.0f || *ec > 20.0f) {
        ESP_LOGW(TAG, "Invalid EC value: %.3f mS/cm (raw=%u), using stub value", *ec, ec_raw);
        *ec = stub_ec;
        use_stub_values = true;
    } else {
        use_stub_values = false;
    }
    
    return true;
}

bool trema_ec_calibrate(uint8_t stage, uint16_t known_tds)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        ESP_LOGW(TAG, "Sensor not initialized");
        return false;
    }
    
    // Validate parameters
    if ((stage != 1 && stage != 2) || known_tds > 10000) {
        ESP_LOGW(TAG, "Invalid calibration parameters");
        return false;
    }
    
    // Write known TDS value
    data[0] = REG_TDS_KNOWN_TDS;
    data[1] = known_tds & 0x00FF;       // LSB
    data[2] = (known_tds >> 8) & 0x00FF; // MSB
    
    if (i2c_bus_write(TREMA_EC_ADDR, data, 3) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write known TDS value");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Send calibration command
    data[0] = REG_TDS_CALIBRATION;
    data[1] = (stage == 1 ? TDS_BIT_CALC_1 : TDS_BIT_CALC_2) | TDS_CODE_CALC_SAVE;
    
    if (i2c_bus_write(TREMA_EC_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send calibration command");
        return false;
    }
    
    ESP_LOGI(TAG, "Calibration stage %d started with TDS %u ppm", stage, known_tds);
    return true;
}

uint8_t trema_ec_get_calibration_status(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return 0;
    }
    
    // Read calibration status
    data[0] = REG_TDS_CALIBRATION;
    if (i2c_bus_write(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration status");
        return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read calibration status");
        return 0;
    }
    
    if (data[0] & 0x40) { // TDS_FLG_STATUS_1
        return 1;
    } else if (data[0] & 0x80) { // TDS_FLG_STATUS_2
        return 2;
    }
    
    return 0;
}

bool trema_ec_set_temperature(float temperature)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        ESP_LOGW(TAG, "Sensor not initialized");
        return false;
    }
    
    // Validate temperature range (0 - 63.75 °C)
    if (temperature < 0.0f || temperature > 63.75f) {
        ESP_LOGW(TAG, "Invalid temperature: %.2f °C", temperature);
        return false;
    }
    
    // Convert temperature to register format (0.25°C steps)
    uint8_t temp_reg = (uint8_t)(temperature * 4.0f);
    
    // Write temperature to register
    data[0] = 0x19; // REG_TDS_t
    data[1] = temp_reg;
    
    if (i2c_bus_write(TREMA_EC_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set temperature");
        return false;
    }
    
    ESP_LOGD(TAG, "Temperature set to %.2f °C", temperature);
    return true;
}

uint16_t trema_ec_get_tds(void)
{
    // Check if sensor is initialized
    if (!sensor_initialized) {
        return stub_tds;
    }
    
    // Request TDS measurement
    data[0] = REG_TDS_TDS; // Register address for TDS measurement
    if (i2c_bus_write(TREMA_EC_ADDR, data, 1) != ESP_OK) {
        ESP_LOGD(TAG, "TDS sensor read failed, using stub values");
        return stub_tds;
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // Read the TDS value (2 bytes)
    if (i2c_bus_read(TREMA_EC_ADDR, data, 2) != ESP_OK) {
        ESP_LOGD(TAG, "TDS sensor read failed, using stub values");
        return stub_tds;
    }
    
    // Convert the 2-byte value to uint16_t
    return ((uint16_t)data[1] << 8) | data[0];
}

float trema_ec_get_conductivity(void)
{
    float ec;
    if (trema_ec_read(&ec)) {
        return ec;
    }
    return stub_ec;
}

bool trema_ec_is_using_stub_values(void)
{
    return use_stub_values;
}