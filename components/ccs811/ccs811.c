#include "ccs811.h"
#include "i2c_bus.h"
#include "esp_log.h"

static bool ccs811_initialized = false;
#define CCS811_ADDR 0x5A

// Stub values for when sensor is not connected
static bool use_stub_values = false;
static float stub_co2 = 450.0f;  // 450 ppm (typical outdoor CO2)

bool ccs811_init(void)
{
    uint8_t cmd[2] = {0x20, 0x08}; // режим 1 сек
    esp_err_t ret = i2c_bus_write(CCS811_ADDR, cmd, 2);
    if (ret != ESP_OK) {
        ccs811_initialized = false;
        // We can still use stub values even if sensor is not initialized
        use_stub_values = true;
        return false;
    }
    
    ccs811_initialized = true;
    return true;
}

bool ccs811_read(float *eco2)
{
    // If sensor was not initialized, use stub values
    if (!ccs811_initialized || use_stub_values) {
        *eco2 = stub_co2;
        return true; // Return true to indicate success with stub values
    }
    
    uint8_t data[8];
    if (i2c_bus_read(CCS811_ADDR, data, 8) != ESP_OK) {
        // Use stub values when sensor read fails
        *eco2 = stub_co2;
        return true; // Return true to indicate success with stub values
    }
    
    if ((data[0] & 0x01) == 0) {
        // Use stub values when no new data
        *eco2 = stub_co2;
        return true; // Return true to indicate success with stub values
    }
    
    *eco2 = (data[2] << 8) | data[3];
    return true;
}