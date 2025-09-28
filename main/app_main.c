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
#include "trema_relay.h"

/* =============================
 *  PIN CONFIGURATION
 * ============================= */
#define I2C_SCL_PIN         17  // I2C clock pin
#define I2C_SDA_PIN         18  // I2C data pin

#define ENC_A_PIN           1   // CLK - Encoder pin (clock signal)
#define ENC_B_PIN           2   // DT - Encoder pin (data)
#define ENC_SW_PIN          3   // Encoder button

// Add HIGH definition for relay
#ifndef HIGH
#define HIGH 1
#endif

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

static const char *TAG = "app_main";

/* =============================
 *  I2C DRIVER WITH MUTEX
 * ============================= */
// Initialize I2C bus with custom settings
// Uses predefined pins from i2c_bus.h header file
static void i2c_bus_init_custom(void)
{
    // The i2c_bus component uses predefined pins from its header
    // If you need to change them, modify i2c_bus.h or extend the API
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    }
    
    // Test I2C communication with a simple write
    uint8_t test_data[] = {0x01, 0x02, 0x03};
    err = i2c_bus_write(0x21, test_data, sizeof(test_data));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to I2C device: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully wrote to I2C device");
    }
}

/* =============================
 *  APPLICATION INITIALIZATION
 * ============================= */
static void app_init(void)
{
    // Reduce log levels for components that may log from ISR context
    // This prevents crashes due to mutex acquisition in interrupt context
    esp_log_level_set("spi_master", ESP_LOG_INFO);  // Reduce from DEBUG to INFO
    esp_log_level_set("LCD", ESP_LOG_INFO);         // Reduce LCD logging level
    
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // I2C initialization
    i2c_bus_init_custom();
    
    // Add a small delay to ensure I2C is fully initialized
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize relay
    ESP_LOGI(TAG, "Attempting to initialize relay...");
    if (!trema_relay_init()) {
        ESP_LOGW(TAG, "Failed to initialize relay");
        // Let's check if we're using stub values
        if (trema_relay_is_using_stub_values()) {
            ESP_LOGW(TAG, "Relay is using stub values (not connected)");
        }
    } else {
        ESP_LOGI(TAG, "Relay initialized successfully");
        // Turn on channel 0 as an example
        trema_relay_digital_write(0, HIGH);
        ESP_LOGI(TAG, "Channel 0 turned ON");
        // Start auto-switching mode
        trema_relay_auto_switch(true);
        ESP_LOGI(TAG, "Auto-switching mode started");
    }

    // Initialize LCD display
    lv_disp_t* disp = lcd_ili9341_init();
    
    // Verify display initialization
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        return;
    }

    // Run diagnostics (uncomment to run diagnostics)
    // run_diagnostics();
    
    // Longer delay to ensure display is ready
    vTaskDelay(pdMS_TO_TICKS(3000));  // Increased delay to 3 seconds
    
    // Create LCD UI using lvgl_main component
    lvgl_main_init();

    // Add a small delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(500));  // Increased delay

    // Verify LVGL is initialized
    if (!lv_is_initialized()) {
        ESP_LOGE(TAG, "LVGL failed to initialize properly");
        return;
    }

    // Force a display refresh to ensure everything is properly initialized
    ESP_LOGI(TAG, "Attempting to acquire LVGL lock for initial refresh");
    if (lvgl_lock(1000)) {  // Increased timeout to 1 second
        ESP_LOGI(TAG, "LVGL lock acquired for initial refresh");
        lv_obj_invalidate(lv_scr_act());
        lvgl_unlock();
        ESP_LOGI(TAG, "Initial display refresh completed");
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for initial refresh");
    }

    // Verify screen is active
    if (lv_scr_act() == NULL) {
        ESP_LOGE(TAG, "No active screen after initialization");
        return;
    } else {
        ESP_LOGI(TAG, "Active screen verified after initialization");
    }

    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(3000));  // Increased delay to 3 seconds
    
    // Initialize rotary encoder
    encoder_config_t encoder_config = {
        .a_pin = ENC_A_PIN,
        .b_pin = ENC_B_PIN,
        .sw_pin = ENC_SW_PIN,
        .high_limit = 100,
        .low_limit = -100
    };

    // Initialize encoder without callback - LVGL will handle it through the input device
    if (!encoder_init_with_config(&encoder_config, NULL, NULL)) {
        ESP_LOGE(TAG, "Failed to initialize rotary encoder");
    } else {
        ESP_LOGI(TAG, "Rotary encoder initialized successfully");
    }
}

/* =============================
 *  SENSOR TASK
 * ============================= */
// Task for handling sensor data
void sensor_task(void *pv)
{
    // Sensor values
    float ph_value, ec_value, temp_value, hum_value, lux_value, co2_value, tvoc_value;
    
    // Initialize sensors
    if (!trema_lux_init()) {
        ESP_LOGW(TAG, "Failed to initialize LUX sensor");
    } else {
        ESP_LOGI(TAG, "LUX sensor initialized successfully");
    }
    
    // Initialize pH sensor
    if (!trema_ph_init()) {
        ESP_LOGW(TAG, "Failed to initialize pH sensor");
    } else {
        ESP_LOGI(TAG, "pH sensor initialized successfully");
    }
    
    // Initialize CCS811 sensor
    if (!ccs811_init()) {
        ESP_LOGW(TAG, "Failed to initialize CCS811 sensor");
    } else {
        ESP_LOGI(TAG, "CCS811 sensor initialized successfully");
    }
    
    // Initialize EC sensor
    if (!trema_ec_init()) {
        ESP_LOGW(TAG, "Failed to initialize EC sensor");
    } else {
        ESP_LOGI(TAG, "EC sensor initialized successfully");
    }

    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(3000));

    int update_count = 0;
    while (1) {
        // Read pH sensor
        if (!trema_ph_read(&ph_value)) {
            ESP_LOGW(TAG, "Failed to read pH sensor");
            ph_value = 6.8f; // Default value
        } else {
            // Check measurement stability
            if (!trema_ph_get_stability()) {
                ESP_LOGW(TAG, "pH measurement is not stable");
                // Try to wait for a stable reading for up to 1 second
                if (trema_ph_wait_for_stable_reading(1000)) {
                    // If we got a stable reading, read the value again
                    if (trema_ph_read(&ph_value)) {
                        ESP_LOGI(TAG, "pH measurement is now stable: %.2f", ph_value);
                    }
                } else {
                    ESP_LOGW(TAG, "pH measurement still unstable after waiting, using last reading: %.2f", ph_value);
                }
            } else {
                ESP_LOGD(TAG, "pH measurement is stable: %.2f", ph_value);
            }
        }

        // Read EC sensor
        if (!trema_ec_read(&ec_value)) {
            ESP_LOGW(TAG, "Failed to read EC sensor");
            ec_value = 1.5f; // Default value
        } else {
            // Optionally get TDS value as well
            uint16_t tds_value = trema_ec_get_tds();
            ESP_LOGD(TAG, "EC: %.2f mS/cm, TDS: %u ppm", ec_value, tds_value);
        }

        // Read temperature and humidity sensor
        if (!sht3x_read(&temp_value, &hum_value)) {
            ESP_LOGW(TAG, "Failed to read SHT3x sensor");
            temp_value = 24.5f; // Default value
            hum_value = 65.0f;  // Default value
        }

        // Read LUX sensor
        if (!trema_lux_read_float(&lux_value)) {
            ESP_LOGW(TAG, "Failed to read LUX sensor");
            lux_value = 1200.0f; // Default value
        }

        // Read CO2 and TVOC from CCS811 sensor
        if (!ccs811_read_data(&co2_value, &tvoc_value)) {
            ESP_LOGW(TAG, "Failed to read CCS811 sensor");
            co2_value = 450.0f;  // Default value
            tvoc_value = 10.0f;  // Default value
        }

        // Update LVGL UI with sensor values
        ESP_LOGI(TAG, "Updating LVGL UI with sensor values: pH=%.2f, EC=%.2f, Temp=%.1f", 
                 ph_value, ec_value, temp_value);
        lvgl_update_sensor_values(
            ph_value,
            ec_value,
            temp_value,
            hum_value,
            lux_value,
            co2_value
        );
        
        update_count++;
        
        // Log sensor values for debugging
        if (update_count % 10 == 0) { // Log every 10 updates (instead of 5)
            ESP_LOGI(TAG, "Sensor readings - pH: %.2f, EC: %.2f, Temp: %.1f, Hum: %.1f, Lux: %.0f, CO2: %.0f, TVOC: %.0f", 
                     ph_value, ec_value, temp_value, hum_value, lux_value, co2_value, tvoc_value);
            
            // Check if we're using stub values for sensors
            if (trema_lux_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for LUX sensor");
            }
            
            if (trema_ph_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for pH sensor");
            }
            
            if (trema_ec_is_using_stub_values()) {
                ESP_LOGD(TAG, "Using stub values for EC sensor");
            }
        }
        
        // Wait before next update
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* =============================
 *  MAIN FUNCTION
 * ============================= */
// Main application function
// Initializes all system components and starts tasks
void app_main(void)
{
    // Initialize application components with proper log levels
    app_init();
    
    // Create sensor task with actual sensor readings
    xTaskCreate(sensor_task, "sensors", 4096, NULL, 3, NULL);

    // For testing purposes, periodically call the test function
    // This can be removed in production
    static uint32_t test_count = 0;

    // Keep the main task alive
    static uint32_t screen_check_count = 0;
    while (1) {
        // Check if LVGL is still initialized
        if (!lv_is_initialized()) {
            ESP_LOGE(TAG, "LVGL is no longer initialized");
            vTaskDelay(pdMS_TO_TICKS(5000)); // Increased delay to 5 seconds to reduce CPU usage and mutex contention
            continue;
        }
        
        // Periodically check if the main screen is active (every 50 cycles)
        if (++screen_check_count % 50 == 0) {
            lv_obj_t* current_screen = lv_scr_act();
            // We can't easily check the screen pointer from here, so just log for debugging
            ESP_LOGD(TAG, "Current screen check: %p", current_screen);
        }
        
        // For testing purposes, periodically call the test function
        // This can be removed in production
        if (++test_count % 15 == 0) { // Call test function every 30 seconds
            lvgl_test_sensor_updates();
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Sleep to reduce CPU usage
    }
}