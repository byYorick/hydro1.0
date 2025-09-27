#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "lcd_ili9341.h"
#include "xpt2046.h"
#include "trema_relay.h"

static const char *TAG = "comprehensive_debug";

// I2C initialization
static void i2c_bus_init_custom(void)
{
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    }
}

void comprehensive_debug_task(void *pvParameters)
{
    ESP_LOGI(TAG, "=== Comprehensive Debug Test ===");
    
    // Initialize I2C bus
    i2c_bus_init_custom();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Test relay
    ESP_LOGI(TAG, "1. Testing Relay...");
    if (trema_relay_init()) {
        ESP_LOGI(TAG, "✓ Relay initialized successfully (model 0x%02X)", 0x0E); // We know it's 0x0E
        trema_relay_digital_write(0, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        trema_relay_digital_write(0, 0);
        ESP_LOGI(TAG, "✓ Relay channel 0 test completed");
    } else {
        ESP_LOGW(TAG, "✗ Relay initialization failed");
        if (trema_relay_is_using_stub_values()) {
            ESP_LOGW(TAG, "  Relay using stub values (not connected or not responding)");
        }
    }
    
    // Test LCD initialization
    ESP_LOGI(TAG, "2. Testing LCD...");
    lv_disp_t* disp = lcd_ili9341_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "✗ LCD initialization failed");
    } else {
        ESP_LOGI(TAG, "✓ LCD initialized successfully");
        
        // Test touch controller AFTER LCD
        ESP_LOGI(TAG, "3. Testing Touch Controller...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Give LCD time to settle
        
        if (xpt2046_init()) {
            ESP_LOGI(TAG, "✓ Touch controller initialized successfully");
            
            // Test touch detection
            for (int i = 0; i < 20; i++) {
                bool touched = xpt2046_is_touched();
                if (touched) {
                    uint16_t x, y;
                    if (xpt2046_read_touch(&x, &y)) {
                        ESP_LOGI(TAG, "  Touch detected at (%d, %d)", x, y);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else {
            ESP_LOGE(TAG, "✗ Touch controller initialization failed");
        }
    }
    
    ESP_LOGI(TAG, "=== Debug Test Completed ===");
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_LOGI(TAG, "Starting Comprehensive Debug Application");
    
    // Create debug task
    xTaskCreate(comprehensive_debug_task, "comprehensive_debug", 4096, NULL, 5, NULL);
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}