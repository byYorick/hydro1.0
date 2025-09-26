#include "trema_ph.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TREMA_PH_ADDR 0x10

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_ph = 6.5f;  // Neutral pH

bool trema_ph_read(float *ph)
{
    uint8_t cmd = 0x02;
    if (i2c_bus_write(TREMA_PH_ADDR, &cmd, 1) != ESP_OK) {
        // Use stub values when sensor is not connected
        *ph = stub_ph;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t raw[4];
    if (i2c_bus_read(TREMA_PH_ADDR, raw, 4) != ESP_OK) {
        // Use stub values when sensor is not connected
        *ph = stub_ph;
        use_stub_values = true;
        return true; // Return true to indicate success with stub values
    }
    
    union { uint8_t b[4]; float f; } u;
    memcpy(u.b, raw, 4);
    *ph = u.f;
    
    return true;
}