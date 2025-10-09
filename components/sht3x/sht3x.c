#include "sht3x.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "sht3x";

#define SHT3X_ADDR 0x44

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_temperature = 25.0f;  // 25°C
static float stub_humidity = 60.0f;    // 60% RH

bool sht3x_read(float *temp, float *hum)
{
    uint8_t cmd[2] = {0x2C, 0x06};
    if (i2c_bus_write(SHT3X_ADDR, cmd, 2) != ESP_OK) {
        ESP_LOGD(TAG, "SHT3x sensor not connected, returning NAN");
        *temp = NAN;
        *hum = NAN;
        use_stub_values = true;
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6];
    if (i2c_bus_read(SHT3X_ADDR, data, 6) != ESP_OK) {
        ESP_LOGD(TAG, "SHT3x sensor read failed, returning NAN");
        *temp = NAN;
        *hum = NAN;
        use_stub_values = true;
        return false;
    }
    
    uint16_t t_raw = (data[0] << 8) | data[1];
    uint16_t h_raw = (data[3] << 8) | data[4];
    *temp = -45.0f + 175.0f * (t_raw / 65535.0f);
    *hum = 100.0f * (h_raw / 65535.0f);
    
    return true;
}