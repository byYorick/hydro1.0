#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "lcd_ili9341.h"
#include "xpt2046.h"

static const char *TAG = "touch_log_test";

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

void touch_log_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "=== Touch Log Test Started ===");
    
    // Initialize I2C bus
    i2c_bus_init_custom();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize LCD display
    ESP_LOGI(TAG, "Initializing LCD display...");
    lv_disp_t* disp = lcd_ili9341_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "LCD display initialized successfully");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Initialize touch controller
    ESP_LOGI(TAG, "Initializing touch controller...");
    if (!xpt2046_init()) {
        ESP_LOGE(TAG, "Failed to initialize touch controller");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Touch controller initialized successfully");
    ESP_LOGI(TAG, "Touch the screen to see coordinates in the logs");
    ESP_LOGI(TAG, "Logs will show both raw and calibrated coordinates");
    
    // Main loop to read touch input
    uint32_t touch_count = 0;
    while (1) {
        uint16_t touch_x, touch_y;
        // Check for touch input
        if (xpt2046_read_touch(&touch_x, &touch_y)) {
            ESP_LOGI(TAG, "Touch #%lu at coordinates: X=%d, Y=%d", 
                     (unsigned long)++touch_count, touch_x, touch_y);
        }
        
        // Check every 50ms for touch input
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_LOGI(TAG, "Starting Touch Log Test Application");
    
    // Create touch test task
    xTaskCreate(touch_log_test_task, "touch_log_test", 4096, NULL, 5, NULL);
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}