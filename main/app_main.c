#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "sht3x.h"
#include "ccs811.h"
#include "trema_ph.h"
#include "trema_ec.h"
#include "trema_lux.h"
#include "encoder.h"
#include "peristaltic_pump.h"
#include "lcd_ili9341.h"
#include "lvgl_main.h"

/* =============================
 *  PIN CONFIGURATION
 * ============================= */
#define I2C_SCL_PIN         17  // I2C clock pin
#define I2C_SDA_PIN         18  // I2C data pin

#define ENC_A_PIN           1   // CLK - Encoder pin (clock signal)
#define ENC_B_PIN           2   // DT - Encoder pin (data)
#define ENC_SW_PIN          3   // Encoder button

// Pump pin configuration - Using valid GPIO pins for ESP32-S3
// Avoiding pins that might cause issues
#define PUMP_PH_ACID_IA     19  // pH acid pump control pin (IA)
#define PUMP_PH_ACID_IB     20  // pH acid pump control pin (IB)
#define PUMP_PH_BASE_IA     21  // pH base pump control pin (IA)
#define PUMP_PH_BASE_IB     47  // pH base pump control pin (IB)
#define PUMP_EC_A_IA        38  // EC A pump control pin (IA)
#define PUMP_EC_A_IB        39  // EC A pump control pin (IB)
#define PUMP_EC_B_IA        40  // EC B pump control pin (IA)
#define PUMP_EC_B_IB        41  // EC B pump control pin (IB)
#define PUMP_EC_C_IA        26  // EC C pump control pin (IA)
#define PUMP_EC_C_IB        27  // EC C pump control pin (IB)

/* =============================
 *  I2C DRIVER WITH MUTEX
 * ============================= */
// Initialize I2C bus with custom settings
// Uses predefined pins from i2c_bus.h header file
static void i2c_bus_init_custom(void)
{
    // The i2c_bus component uses predefined pins from its header
    // If you need to change them, modify i2c_bus.h or extend the API
    i2c_bus_init();
}

/* =============================
 *  SENSOR TASK
 * ============================= */
// Task for handling sensor data
// Instead of reading real sensors, uses fixed placeholder values
void sensor_task(void *pv)
{
    // Use fixed placeholder values for demonstration
    // These values represent typical conditions for a hydroponic system
    const float placeholder_ph = 6.8f;      // Optimal pH for most hydroponic plants
    const float placeholder_ec = 1.5f;      // Nutrient concentration in mS/cm
    const float placeholder_temp = 24.5f;   // Temperature in Celsius
    const float placeholder_hum = 65.0f;    // Humidity percentage
    const float placeholder_lux = 1200.0f;  // Light intensity in lux
    const float placeholder_co2 = 420.0f;   // CO2 concentration in ppm

    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(3000));

    int update_count = 0;
    while (1) {
        // Update LVGL UI with placeholder values
        lvgl_update_sensor_values(
            placeholder_ph + (update_count * 0.01f),  // Add a small increment to see changes
            placeholder_ec + (update_count * 0.01f),
            placeholder_temp + (update_count * 0.1f),
            placeholder_hum + (update_count * 0.1f),
            placeholder_lux + (update_count * 10.0f),
            placeholder_co2 + (update_count * 1.0f)
        );
        
        update_count++;
        
        // Wait before next update
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* =============================
 *  PUMP INITIALIZATION
 * ============================= */
/*
// Initialize pumps
// Configures all system pumps with corresponding pins
static void pumps_init(void)
{
    ESP_LOGI(TAG, "Initializing pumps...");
    ESP_LOGI(TAG, "PUMP_PH_ACID: IA=%d, IB=%d", PUMP_PH_ACID_IA, PUMP_PH_ACID_IB);
    pump_init(PUMP_PH_ACID_IA, PUMP_PH_ACID_IB);
    
    ESP_LOGI(TAG, "PUMP_PH_BASE: IA=%d, IB=%d", PUMP_PH_BASE_IA, PUMP_PH_BASE_IB);
    pump_init(PUMP_PH_BASE_IA, PUMP_PH_BASE_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_A: IA=%d, IB=%d", PUMP_EC_A_IA, PUMP_EC_A_IB);
    pump_init(PUMP_EC_A_IA, PUMP_EC_A_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_B: IA=%d, IB=%d", PUMP_EC_B_IA, PUMP_EC_B_IB);
    pump_init(PUMP_EC_B_IA, PUMP_EC_B_IB);
    
    ESP_LOGI(TAG, "PUMP_EC_C: IA=%d, IB=%d", PUMP_EC_C_IA, PUMP_EC_C_IB);
    pump_init(PUMP_EC_C_IA, PUMP_EC_C_IB);
    
    ESP_LOGI(TAG, "Pumps initialization completed");
}
*/

/* =============================
 *  MAIN FUNCTION
 * ============================= */
// Main application function
// Initializes all system components and starts tasks
void app_main(void)
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // I2C initialization
    i2c_bus_init_custom();

    // Initialize LCD display
    lv_disp_t* disp = lcd_ili9341_init();
    
    // Verify display initialization
    if (disp == NULL) {
        return;
    }

    // Longer delay to ensure display is ready
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Create LCD UI using lvgl_main component
    lvgl_main_init();
    
    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Create sensor task with placeholder values
    xTaskCreate(sensor_task, "sensors", 4096, NULL, 3, NULL);

    // Keep the main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Sleep to reduce CPU usage
    }
}