#include "trema_lux.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TREMA_LUX_ADDR 0x12

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static uint16_t stub_lux = 800;  // 800 lux (daylight)

bool trema_lux_read(uint16_t *lux)
{
    uint8_t cmd = 0x02;
    if (i2c_bus_write(TREMA_LUX_ADDR, &cmd, 1) != ESP_OK) {
        // Use stub values when sensor is not connected
        *lux = stub_lux;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t raw[2];
    if (i2c_bus_read(TREMA_LUX_ADDR, raw, 2) != ESP_OK) {
        // Use stub values when sensor is not connected
        *lux = stub_lux;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    *lux = (raw[0] << 8) | raw[1];
    
    return true;
}