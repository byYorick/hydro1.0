#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "xpt2046.h"
#include "lcd_ili9341.h"
#include "lvgl.h"

static const char *TAG = "touch_debug_test";

void touch_debug_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting touch debug test...");
    
    // Initialize LCD display first
    ESP_LOGI(TAG, "Initializing LCD display...");
    lv_disp_t* disp = lcd_ili9341_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        vTaskDelete(NULL);
        return;
    }
    
    // Initialize touch controller
    ESP_LOGI(TAG, "Initializing XPT2046 touch controller...");
    if (!xpt2046_init()) {
        ESP_LOGE(TAG, "Failed to initialize XPT2046 touch controller");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "XPT2046 touch controller initialized successfully");
    ESP_LOGI(TAG, "Touch the screen to see coordinates in the logs");
    
    uint16_t touch_x, touch_y;
    uint32_t touch_count = 0;
    
    while (1) {
        // Check for touch input
        ESP_LOGD(TAG, "Checking for touch...");
        if (xpt2046_read_touch(&touch_x, &touch_y)) {
            ESP_LOGI(TAG, "Touch #%lu at coordinates: X=%d, Y=%d", 
                     (unsigned long)++touch_count, touch_x, touch_y);
            
            // Brief delay to avoid multiple detections of the same touch
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            ESP_LOGD(TAG, "No touch detected");
        }
        
        // Check every 50ms for touch input
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // Create touch debug task
    xTaskCreate(touch_debug_task, "touch_debug", 4096, NULL, 5, NULL);
    
    // Keep main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}