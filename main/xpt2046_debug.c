#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "xpt2046.h"

static const char *TAG = "xpt2046_debug";

void xpt2046_debug_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting XPT2046 debug test");
    
    // Initialize touch controller
    ESP_LOGI(TAG, "Calling xpt2046_init()");
    if (!xpt2046_init()) {
        ESP_LOGE(TAG, "Failed to initialize XPT2046 touch controller");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "XPT2046 touch controller initialized successfully");
    
    // Test touch detection in a loop
    for (int i = 0; i < 70; i++) {  // Run for about 3.5 seconds
        bool touched = xpt2046_is_touched();
        if (touched) {
            uint16_t x, y;
            if (xpt2046_read_touch(&x, &y)) {
                ESP_LOGI(TAG, "Touch detected at (%d, %d)", x, y);
            }
        } else {
            ESP_LOGD(TAG, "No touch detected");
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "XPT2046 debug test completed");
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

    ESP_LOGI(TAG, "Starting XPT2046 debug application");
    
    // Create debug task
    xTaskCreate(xpt2046_debug_task, "xpt2046_debug", 4096, NULL, 5, NULL);
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}