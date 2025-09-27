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
#include "xpt2046.h"
#include "lvgl.h"

// Forward declarations for trema_relay functions (IDE workaround)
bool trema_relay_init(void);
void trema_relay_digital_write(uint8_t channel, uint8_t value);
void trema_relay_auto_switch(bool enable);

// Forward declarations for xpt2046 functions (IDE workaround)
bool xpt2046_init(void);
bool xpt2046_read_touch(uint16_t *x, uint16_t *y);

// Touch input device callback is now declared in lvgl_main.h

/* =============================
 *  PIN CONFIGURATION
 * ============================= */
#define I2C_SCL_PIN         17  // I2C clock pin
#define I2C_SDA_PIN         18  // I2C data pin

#define ENC_A_PIN           1   // CLK - Encoder pin (clock signal)
#define ENC_B_PIN           2   // DT - Encoder pin (data)
#define ENC_SW_PIN          3   // Encoder button

// XPT2046 Touch Panel Pins
#define XPT2046_IRQ         4   // Touch interrupt pin
#define XPT2046_CS          5   // Chip select pin
#define XPT2046_CLK         12  // Clock pin (shared with LCD)
#define XPT2046_MOSI        11  // MOSI pin (shared with LCD)
#define XPT2046_MISO        13  // MISO pin

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
 *  TOUCH TASK
 * ============================= */
// Task for handling touch input
void touch_task(void *pv)
{
    uint16_t touch_x, touch_y;
    static uint32_t touch_count = 0;
    static uint32_t last_touch_time = 0;
    
    ESP_LOGI(TAG, "Touch task started");
    
    // Add a small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        ESP_LOGD(TAG, "Checking for touch input...");
        // Check for touch input
        bool touch_detected = xpt2046_read_touch(&touch_x, &touch_y);
        ESP_LOGD(TAG, "xpt2046_read_touch returned: %s", touch_detected ? "true" : "false");
        
        if (touch_detected) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Log touch event with timestamp and count
            ESP_LOGI(TAG, "Touch #%lu at coordinates: X=%d, Y=%d", 
                     (unsigned long)++touch_count, touch_x, touch_y);
            
            // Log time since last touch (if not the first touch)
            if (last_touch_time > 0) {
                uint32_t time_diff = current_time - last_touch_time;
                ESP_LOGI(TAG, "Time since last touch: %lu ms", (unsigned long)time_diff);
            }
            
            last_touch_time = current_time;
            
            // Additional touch processing can be added here
            // For example, send it to LVGL or handle UI interactions
            
            // Brief delay to avoid multiple detections of the same touch
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            // Log that no touch was detected (at debug level to avoid spam)
            ESP_LOGD(TAG, "No touch detected");
        }
        
        // Check every 50ms for touch input
        vTaskDelay(pdMS_TO_TICKS(50));
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

    // Initialize LCD display FIRST, before touch controller
    ESP_LOGI(TAG, "Initializing LCD display...");
    lv_disp_t* disp = lcd_ili9341_init();
    
    // Verify display initialization
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        return;
    }

    // Initialize touch controller AFTER LCD (so SPI bus is already initialized)
    ESP_LOGI(TAG, "Initializing touch controller...");
    bool touch_initialized = xpt2046_init();
    if (!touch_initialized) {
        ESP_LOGW(TAG, "Failed to initialize touch controller");
        ESP_LOGW(TAG, "Check XPT2046_DEBUGGING.md for troubleshooting steps");
    } else {
        ESP_LOGI(TAG, "Touch controller initialized successfully");
        // Calibrate touch controller with default values
        xpt2046_calibrate(300, 3800, 200, 3900);
    }
    
    // Run diagnostics (uncomment to run diagnostics)
    // run_diagnostics();
    
    // Longer delay to ensure display is ready
    vTaskDelay(pdMS_TO_TICKS(3000));  // Increased delay to 3 seconds
    
    // Create LCD UI using lvgl_main component
    lvgl_main_init();
    
    // Add a small delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(500));  // Increased delay
    
    // Force a display refresh to ensure everything is properly initialized
    if (lvgl_lock(1000)) {  // Increased timeout to 1 second
        lv_obj_invalidate(lv_scr_act());
        lv_timer_handler();
        lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for initial refresh");
    }
    
    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(3000));  // Increased delay to 3 seconds
    
    // Initialize touch input device for LVGL after display is ready
    ESP_LOGI(TAG, "Initializing LVGL touch input device");
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    ESP_LOGI(TAG, "Registering touch input device with LVGL");
    lv_indev_t *touch_indev = lv_indev_drv_register(&indev_drv);
    
    if (touch_indev) {
        ESP_LOGI(TAG, "Touch input device registered successfully");
        // Log the indev pointer for debugging
        ESP_LOGD(TAG, "Touch input device pointer: %p", touch_indev);
    } else {
        ESP_LOGE(TAG, "Failed to register touch input device");
        // Try to get more information about why it failed
        if (!lv_is_initialized()) {
            ESP_LOGE(TAG, "LVGL is not initialized");
        }
        if (!lv_scr_act()) {
            ESP_LOGE(TAG, "No active screen");
        }
    }
    
    // Create touch task for direct logging of touch coordinates
    // Only create if touch controller was initialized successfully
    if (touch_initialized) {
        ESP_LOGI(TAG, "Creating touch task");
        xTaskCreate(touch_task, "touch", 4096, NULL, 3, NULL);
    } else {
        ESP_LOGW(TAG, "Touch controller not initialized, skipping touch task creation");
    }
    
    // Create sensor task with actual sensor readings
    xTaskCreate(sensor_task, "sensors", 4096, NULL, 3, NULL);

    // Keep the main task alive
    while (1) {
        // Periodically refresh the display to ensure consistent rendering
        if (lvgl_lock(10)) {
            lv_timer_handler();
            lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Sleep to reduce CPU usage
    }
}