#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "lcd_ili9341.h"
#include "lvgl_main.h"
#include "xpt2046.h"

static const char *TAG = "lvgl_touch_test";

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

// Main application function
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

    // Initialize touch controller
    ESP_LOGI(TAG, "Initializing touch controller...");
    if (!xpt2046_init()) {
        ESP_LOGW(TAG, "Failed to initialize touch controller");
    } else {
        ESP_LOGI(TAG, "Touch controller initialized successfully");
    }

    // Initialize LCD display
    lv_disp_t* disp = lcd_ili9341_init();
    
    // Verify display initialization
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LCD display");
        return;
    }

    // Longer delay to ensure display is ready
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Create LCD UI using lvgl_main component
    lvgl_main_init();
    
    // Add a small delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Force a display refresh to ensure everything is properly initialized
    if (lvgl_lock(1000)) {
        lv_obj_invalidate(lv_scr_act());
        lv_timer_handler();
        lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for initial refresh");
    }
    
    // Longer delay to ensure UI is fully initialized
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "LVGL with touch support initialized. Touch the screen to test.");

    // Keep the main task alive
    while (1) {
        // Periodically refresh the display to ensure consistent rendering
        if (lvgl_lock(10)) {
            lv_timer_handler();
            lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Faster refresh for better touch response
    }
}