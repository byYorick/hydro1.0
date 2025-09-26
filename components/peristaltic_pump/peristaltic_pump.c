#include "peristaltic_pump.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void pump_init(int ia, int ib)
{
    // Validate pin numbers
    if (ia < 0 || ia >= GPIO_NUM_MAX || ib < 0 || ib >= GPIO_NUM_MAX) {
        return;
    }
    
    esp_err_t err = gpio_set_direction(ia, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        return;
    }
    
    err = gpio_set_direction(ib, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        return;
    }
    
    gpio_set_level(ia, 0);
    gpio_set_level(ib, 0);
}

void pump_run_ms(int ia, int ib, uint32_t ms)
{
    // Validate pin numbers
    if (ia < 0 || ia >= GPIO_NUM_MAX || ib < 0 || ib >= GPIO_NUM_MAX) {
        return;
    }
    
    gpio_set_level(ia, 1);
    gpio_set_level(ib, 0);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_set_level(ia, 0);
    gpio_set_level(ib, 0);
}