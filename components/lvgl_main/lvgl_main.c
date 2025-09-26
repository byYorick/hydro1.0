#include "lvgl_main.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Screen and UI elements
static lv_obj_t *screen_main;
static lv_obj_t *label_title;
static lv_obj_t *label_ph;
static lv_obj_t *label_ec;
static lv_obj_t *label_temp;
static lv_obj_t *label_hum;
static lv_obj_t *label_lux;
static lv_obj_t *label_co2;

// Queue for sensor data updates
static QueueHandle_t sensor_data_queue = NULL;
#define SENSOR_DATA_QUEUE_SIZE 10

// Sensor data structure
typedef struct {
    float ph;
    float ec;
    float temp;
    float hum;
    float lux;
    float co2;
} sensor_data_t;

// Function to create the main UI
static void create_main_ui(void);

// Display update task
static void display_update_task(void *pvParameters);

// Function to create the main user interface
static void create_main_ui(void)
{
    // Get the active screen
    screen_main = lv_disp_get_scr_act(NULL);
    if (screen_main == NULL) {
        return;
    }
    
    // Clean the screen
    lv_obj_clean(screen_main);
    
    // Set background color to white for better text visibility
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0xFFFFFF), 0);
    
    // Create title
    label_title = lv_label_create(screen_main);
    if (label_title == NULL) {
        return;
    }
    lv_label_set_text(label_title, "Hydroponics System");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, 0);
    
    // Create sensor value labels with better styling
    static lv_style_t style_value;
    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, lv_color_hex(0x000000));
    lv_style_set_text_font(&style_value, &lv_font_montserrat_14);
    
    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, lv_color_hex(0x0066CC));
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
    
    // pH section
    lv_obj_t *label_ph_title = lv_label_create(screen_main);
    if (label_ph_title == NULL) {
        return;
    }
    lv_label_set_text(label_ph_title, "pH:");
    lv_obj_add_style(label_ph_title, &style_label, 0);
    lv_obj_align(label_ph_title, LV_ALIGN_TOP_LEFT, 10, 40);
    
    label_ph = lv_label_create(screen_main);
    if (label_ph == NULL) {
        return;
    }
    lv_label_set_text(label_ph, "6.80");
    lv_obj_add_style(label_ph, &style_value, 0);
    lv_obj_align(label_ph, LV_ALIGN_TOP_LEFT, 60, 40);
    
    // EC section
    lv_obj_t *label_ec_title = lv_label_create(screen_main);
    lv_label_set_text(label_ec_title, "EC:");
    lv_obj_add_style(label_ec_title, &style_label, 0);
    lv_obj_align(label_ec_title, LV_ALIGN_TOP_LEFT, 10, 70);
    
    label_ec = lv_label_create(screen_main);
    lv_label_set_text(label_ec, "1.50 mS/cm");
    lv_obj_add_style(label_ec, &style_value, 0);
    lv_obj_align(label_ec, LV_ALIGN_TOP_LEFT, 60, 70);
    
    // Temperature section
    lv_obj_t *label_temp_title = lv_label_create(screen_main);
    lv_label_set_text(label_temp_title, "Temp:");
    lv_obj_add_style(label_temp_title, &style_label, 0);
    lv_obj_align(label_temp_title, LV_ALIGN_TOP_LEFT, 10, 100);
    
    label_temp = lv_label_create(screen_main);
    lv_label_set_text(label_temp, "24.5 C");
    lv_obj_add_style(label_temp, &style_value, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_LEFT, 60, 100);
    
    // Humidity section
    lv_obj_t *label_hum_title = lv_label_create(screen_main);
    lv_label_set_text(label_hum_title, "Humidity:");
    lv_obj_add_style(label_hum_title, &style_label, 0);
    lv_obj_align(label_hum_title, LV_ALIGN_TOP_LEFT, 10, 130);
    
    label_hum = lv_label_create(screen_main);
    lv_label_set_text(label_hum, "65.0 %");
    lv_obj_add_style(label_hum, &style_value, 0);
    lv_obj_align(label_hum, LV_ALIGN_TOP_LEFT, 60, 130);
    
    // Lux section
    lv_obj_t *label_lux_title = lv_label_create(screen_main);
    lv_label_set_text(label_lux_title, "Light:");
    lv_obj_add_style(label_lux_title, &style_label, 0);
    lv_obj_align(label_lux_title, LV_ALIGN_TOP_LEFT, 10, 160);
    
    label_lux = lv_label_create(screen_main);
    lv_label_set_text(label_lux, "1200 lux");
    lv_obj_add_style(label_lux, &style_value, 0);
    lv_obj_align(label_lux, LV_ALIGN_TOP_LEFT, 60, 160);
    
    // CO2 section
    lv_obj_t *label_co2_title = lv_label_create(screen_main);
    lv_label_set_text(label_co2_title, "CO2:");
    lv_obj_add_style(label_co2_title, &style_label, 0);
    lv_obj_align(label_co2_title, LV_ALIGN_TOP_LEFT, 10, 190);
    
    label_co2 = lv_label_create(screen_main);
    lv_label_set_text(label_co2, "420 ppm");
    lv_obj_add_style(label_co2, &style_value, 0);
    lv_obj_align(label_co2, LV_ALIGN_TOP_LEFT, 60, 190);
    
    // Create queue for sensor data
    sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
    if (sensor_data_queue == NULL) {
        return;
    }
    
    // Create display update task
    BaseType_t task_result = xTaskCreate(display_update_task, "display_update", 4096, NULL, 5, NULL);
    if (task_result != pdPASS) {
        return;
    }
}

// Display update task
static void display_update_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    
    while (1) {
        // Wait for sensor data
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Try to lock the mutex as LVGL APIs are not thread-safe
            if (!lvgl_lock(50)) {
                continue;
            }
            
            // Update sensor value labels
            if (label_ph) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.2f", sensor_data.ph);
                lv_label_set_text(label_ph, buffer);
            }
            
            if (label_ec) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.2f mS/cm", sensor_data.ec);
                lv_label_set_text(label_ec, buffer);
            }
            
            if (label_temp) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.1f C", sensor_data.temp);
                lv_label_set_text(label_temp, buffer);
            }
            
            if (label_hum) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.1f %%", sensor_data.hum);
                lv_label_set_text(label_hum, buffer);
            }
            
            if (label_lux) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.0f lux", sensor_data.lux);
                lv_label_set_text(label_lux, buffer);
            }
            
            if (label_co2) {
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%.0f ppm", sensor_data.co2);
                lv_label_set_text(label_co2, buffer);
            }
            
            // Force screen refresh
            lv_obj_invalidate(lv_scr_act());
            
            // Release the mutex
            lvgl_unlock();
        }
    }
}

// Initialize LVGL UI
void lvgl_main_init(void)
{
    create_main_ui();
}

// Update sensor values on screen
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // Check if queue is initialized
    if (sensor_data_queue == NULL) {
        return;
    }
    
    // Create sensor data structure
    sensor_data_t sensor_data = {
        .ph = ph,
        .ec = ec,
        .temp = temp,
        .hum = hum,
        .lux = lux,
        .co2 = co2
    };
    
    // Send sensor data to queue (non-blocking)
    xQueueSend(sensor_data_queue, &sensor_data, 0);
}