/*
 * LCD ILI9341 Driver Implementation
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_ili9341.h"
#include "lvgl.h"

#include "lcd_ili9341.h"

// Using SPI2 
#define LCD_HOST  SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// LCD Configuration //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)  // Increased from 20MHz to 40MHz for better pixel clarity

// LCD backlight on level (1 - high level, 0 - low level)
#define LCD_BK_LIGHT_ON_LEVEL  1

// LCD backlight off level
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

// SPI clock pin number (SCLK)
#define PIN_NUM_SCLK           12

// SPI data pin number (MOSI)
#define PIN_NUM_MOSI           11

// SPI input pin number (MISO), -1 if not used
#define PIN_NUM_MISO           -1

// Command/Data pin number (DC)
#define PIN_NUM_LCD_DC         9

// Reset pin number (RST)
#define PIN_NUM_LCD_RST        14

// Chip select pin number (CS)
#define PIN_NUM_LCD_CS         10

// Backlight control pin number
#define PIN_NUM_BK_LIGHT       15

#define LVGL_TICK_PERIOD_MS    2  // Reset to 2ms for balanced performance
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (12 * 1024)
#define LVGL_TASK_PRIORITY     5  // Increased priority to prevent conflicts

static SemaphoreHandle_t lvgl_mux = NULL;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static TaskHandle_t lvgl_task_handle = NULL;  // Task handle for debugging
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;  // Store IO handle for callback

// Forward declarations
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_port_update_callback(lv_disp_drv_t *drv);
static void increase_lvgl_tick(void *arg);
static void lvgl_task_handler(void *pvParameters);

// Lock LVGL mutex
// Used to ensure thread safety when working with LVGL API
bool lvgl_lock(int timeout_ms)
{
    // Convert timeout from milliseconds to FreeRTOS ticks
    // If `timeout_ms` is -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

// Unlock LVGL mutex
// Releases the mutex after finishing work with LVGL API
void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

// Deprecated function for updating sensor values
// UI is now handled by the lvgl_main component
void lcd_ili9341_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    // This function is now deprecated as UI is handled by lvgl_main component
    // Kept for backward compatibility
}

// LVGL task handler
// Executes LVGL timer handling and manages display updates
static void lvgl_task_handler(void *pvParameters)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    
    ESP_LOGI("LCD", "LVGL task handler started");
    
    while (1) {
        // Lock the mutex as LVGL APIs are not thread-safe
        if (lvgl_lock(200)) {  // Increased timeout to 200ms
            // Additional check to ensure LVGL is initialized
            if (lv_is_initialized()) {
                task_delay_ms = lv_timer_handler();
            } else {
                ESP_LOGW("LCD", "LVGL not initialized, skipping timer handler");
                task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
            }
            // Release the mutex
            lvgl_unlock();
        } else {
            // If we can't lock the mutex, continue to retry
            ESP_LOGW("LCD", "Failed to acquire LVGL lock, retrying");
        }
        
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// Initialize LCD ILI9341 display
// Configures SPI, initializes the panel, and registers the display driver for LVGL
lv_disp_t* lcd_ili9341_init(void)
{
    ESP_LOGI("LCD", "Initializing LCD ILI9341 display");
    
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // Initialize LVGL library
    lv_init();

    // Create mutex for LVGL
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mux == NULL) {
        ESP_LOGE("LCD", "Failed to create LVGL mutex");
        return NULL;
    }

    // Install LVGL tick timer BEFORE creating any LVGL objects
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_err_t timer_result = esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    if (timer_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to create LVGL tick timer: %s", esp_err_to_name(timer_result));
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(timer_result);

    timer_result = esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    if (timer_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to start LVGL tick timer: %s", esp_err_to_name(timer_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(timer_result);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 40 * sizeof(uint16_t), // Increased buffer size for efficiency
    };
    ESP_LOGI("LCD", "Initializing SPI bus");
    esp_err_t spi_result = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (spi_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to initialize SPI bus: %s", esp_err_to_name(spi_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(spi_result);

    static lv_disp_drv_t disp_drv;      // contains callback functions
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,  // Add callback for flush ready notification
        .user_ctx = &disp_drv,  // Pass display driver as user context
    };
    ESP_LOGI("LCD", "Creating panel IO handle");
    // Attach the LCD to the SPI bus
    esp_err_t io_result = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io_handle);
    if (io_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to create panel IO: %s", esp_err_to_name(io_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(io_result);

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // Changed from RGB to BGR for ILI9341
        .bits_per_pixel = 16,
    };
    ESP_LOGI("LCD", "Creating panel handle");
    esp_err_t panel_result = esp_lcd_new_panel_ili9341(lcd_io_handle, &panel_config, &panel_handle);
    if (panel_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to create panel: %s", esp_err_to_name(panel_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(panel_result);

    ESP_LOGI("LCD", "Resetting panel");
    esp_err_t reset_result = esp_lcd_panel_reset(panel_handle);
    if (reset_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to reset panel: %s", esp_err_to_name(reset_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(reset_result);
    
    ESP_LOGI("LCD", "Initializing panel");
    esp_err_t init_result = esp_lcd_panel_init(panel_handle);
    if (init_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to initialize panel: %s", esp_err_to_name(init_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_ERROR_CHECK(init_result);
    
    ESP_LOGI("LCD", "Configuring panel orientation");
    // Настройка панели:
    esp_lcd_panel_swap_xy(panel_handle, false);
    esp_lcd_panel_mirror(panel_handle, false, true); // стандарт для 240x320 портрет

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_LOGI("LCD", "Turning on display");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Turn on LCD backlight
    ESP_LOGI("LCD", "Turning on backlight");
    esp_err_t backlight_result = gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
    if (backlight_result != ESP_OK) {
        ESP_LOGE("LCD", "Failed to set backlight: %s", esp_err_to_name(backlight_result));
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }

    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    ESP_LOGI("LCD", "Allocating draw buffers: %d bytes each", 240 * 40 * sizeof(lv_color_t));
    lv_color_t *buf1 = heap_caps_malloc(240 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    if (buf1 == NULL) {
        ESP_LOGE("LCD", "Failed to allocate first draw buffer");
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_LOGI("LCD", "First draw buffer allocated at %p", buf1);
    
    lv_color_t *buf2 = heap_caps_malloc(240 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    if (buf2 == NULL) {
        ESP_LOGE("LCD", "Failed to allocate second draw buffer");
        heap_caps_free(buf1);
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }
    ESP_LOGI("LCD", "Second draw buffer allocated at %p", buf2);

    // initialize LVGL draw buffers with recommended size
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffers called draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, 240 * 40);
    ESP_LOGI("LCD", "Draw buffers initialized with %d pixels", 240 * 40);

    // Register display driver to LVGL
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;
    // В драйвере:
    disp_drv.antialiasing = 1;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.drv_update_cb = lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;

    // Enable anti-aliasing for better text rendering
    disp_drv.antialiasing = 1;
    
    // Enable screen rotation support
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_NONE;
    
    // Improve text rendering quality
    disp_drv.full_refresh = 0;  // Partial refresh for better performance
    disp_drv.direct_mode = 0;   // Use buffer mode for better text quality

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL) {
        ESP_LOGE("LCD", "Failed to register display driver");
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }

    // Small delay to ensure the display driver is fully registered
    vTaskDelay(pdMS_TO_TICKS(10));

    // Create LVGL task with higher priority
    BaseType_t task_result = xTaskCreate(lvgl_task_handler, "lvgl_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle);
    if (task_result != pdPASS) {
        ESP_LOGE("LCD", "Failed to create LVGL task");
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        esp_timer_delete(lvgl_tick_timer);
        vSemaphoreDelete(lvgl_mux);
        return NULL;
    }

    return disp;
}

// Set display brightness
// Takes brightness value from 0 to 100
void lcd_ili9341_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    
    // For simple on/off control, we'll just turn it on/off
    // For PWM control, you would need to implement PWM on the backlight pin
    gpio_set_level(PIN_NUM_BK_LIGHT, brightness > 0 ? LCD_BK_LIGHT_ON_LEVEL : LCD_BK_LIGHT_OFF_LEVEL);
}

// Callback functions
// Notifies screen refresh readiness
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    ESP_LOGD("LCD", "notify_lvgl_flush_ready called");
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    
    // Check if we can acquire the mutex without blocking to avoid deadlock
    if (xSemaphoreTakeRecursive(lvgl_mux, 0) == pdTRUE) {
        lv_disp_flush_ready(disp_driver);
        xSemaphoreGiveRecursive(lvgl_mux);
        ESP_LOGD("LCD", "lv_disp_flush_ready completed");
    } else {
        // If we can't acquire the mutex immediately, defer the flush ready call
        ESP_LOGW("LCD", "Could not acquire mutex in callback, deferring flush ready");
    }
    
    return false;
}

// Screen refresh callback function
// Draws bitmap to the specified area of the display
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    ESP_LOGD("LCD", "lvgl_flush_cb called");
    
    // Safety check for null pointers
    if (!drv || !area || !color_map) {
        ESP_LOGE("LCD", "Null pointer in lvgl_flush_cb");
        return;
    }
    
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    
    // Safety check for panel handle
    if (!panel_handle) {
        ESP_LOGE("LCD", "Invalid panel handle in lvgl_flush_cb");
        return;
    }
    
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    // Bounds checking
    if (offsetx1 < 0 || offsety1 < 0 || offsetx2 >= 240 || offsety2 >= 320) {
        ESP_LOGW("LCD", "Invalid area coordinates: (%d,%d) to (%d,%d)", offsetx1, offsety1, offsetx2, offsety2);
        // Don't return here, let the panel handle deal with it
    }
    
    // Log flush operations for debugging (only for larger areas to avoid spam)
    int area_size = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);
    if (area_size > 1000) {  // Only log for areas larger than 1000 pixels
        ESP_LOGD("LCD", "Flush area: (%d,%d) to (%d,%d), size: %d pixels", 
                 offsetx1, offsety1, offsetx2, offsety2, area_size);
    }
    
    ESP_LOGD("LCD", "Calling esp_lcd_panel_draw_bitmap");
    // copy a buffer's content to a specific area of the display
    esp_err_t result = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    
    if (result != ESP_OK) {
        ESP_LOGW("LCD", "Failed to draw bitmap: %s", esp_err_to_name(result));
    } else {
        ESP_LOGD("LCD", "Successfully drew bitmap: (%d,%d) to (%d,%d)", offsetx1, offsety1, offsetx2, offsety2);
    }
    
    // NOTE: We no longer call lv_disp_flush_ready here as it's handled in the callback
    // This prevents the deadlock that was occurring
}

// Port update callback function
// Handles display rotation
static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    // Safety check for null pointer
    if (!drv) {
        ESP_LOGE("LCD", "Null pointer in lvgl_port_update_callback");
        return;
    }
    
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    
    // Safety check for panel handle
    if (!panel_handle) {
        ESP_LOGE("LCD", "Invalid panel handle in lvgl_port_update_callback");
        return;
    }

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);  // mirror Y = true
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    default:
        ESP_LOGW("LCD", "Unknown rotation value: %d", drv->rotated);
        break;
    }
    
    // After rotation, force a full screen refresh to ensure text clarity
    // Only invalidate if LVGL is initialized
    if (lv_is_initialized() && lv_scr_act()) {
        lv_obj_invalidate(lv_scr_act());
    }
}

// Increase LVGL ticks
// Tells LVGL how many milliseconds have elapsed
static void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}