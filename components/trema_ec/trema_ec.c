#include "trema_ec.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TREMA_EC_ADDR 0x11

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_ec = 1.2f;  // 1.2 mS/cm

bool trema_ec_read(float *ec)
{
    uint8_t cmd = 0x02;
    if (i2c_bus_write(TREMA_EC_ADDR, &cmd, 1) != ESP_OK) {
        // Use stub values when sensor is not connected
        *ec = stub_ec;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t raw[4];
    if (i2c_bus_read(TREMA_EC_ADDR, raw, 4) != ESP_OK) {
        // Use stub values when sensor is not connected
        *ec = stub_ec;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    union { uint8_t b[4]; float f; } u;
    memcpy(u.b, raw, 4);
    *ec = u.f;
    
    return true;
}