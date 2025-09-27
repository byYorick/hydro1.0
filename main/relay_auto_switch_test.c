#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "trema_relay.h"

static const char *TAG = "relay_auto_switch_test";

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

void relay_auto_switch_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "=== Relay Auto-Switch Test Started ===");
    
    // Initialize I2C bus
    i2c_bus_init_custom();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize relay
    ESP_LOGI(TAG, "Initializing relay...");
    if (!trema_relay_init()) {
        ESP_LOGE(TAG, "Failed to initialize relay");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Relay initialized successfully (model 0x%02X)", 0x0E);
    
    // Test manual control first
    ESP_LOGI(TAG, "Testing manual relay control...");
    trema_relay_digital_write(0, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    trema_relay_digital_write(0, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Manual control test completed");
    
    // Start auto-switch mode
    ESP_LOGI(TAG, "Starting auto-switch mode...");
    trema_relay_auto_switch(true);
    
    // Let it run for 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));
    
    // Stop auto-switch mode
    ESP_LOGI(TAG, "Stopping auto-switch mode...");
    trema_relay_auto_switch(false);
    
    ESP_LOGI(TAG, "=== Relay Auto-Switch Test Completed ===");
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

    ESP_LOGI(TAG, "Starting Relay Auto-Switch Test Application");
    
    // Create relay test task
    xTaskCreate(relay_auto_switch_test_task, "relay_test", 4096, NULL, 3, NULL);
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}