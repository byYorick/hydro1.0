#include "encoder.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "encoder";

// Encoder configuration
static encoder_config_t enc_config = {
    .a_pin = -1,
    .b_pin = -1,
    .sw_pin = -1,
    .high_limit = 100,
    .low_limit = -100
};

// PCNT unit handle
static pcnt_unit_handle_t pcnt_unit = NULL;

// Callback function and user context
static encoder_callback_t enc_callback = NULL;
static void* enc_user_ctx = NULL;

// Queue for watch point events
static QueueHandle_t encoder_queue = NULL;

// Task handle for encoder monitoring
static TaskHandle_t encoder_task_handle = NULL;

// Interrupt callback for watch point events
static bool encoder_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    // Send event data to queue from interrupt callback
    if (encoder_queue != NULL) {
        xQueueSendFromISR(encoder_queue, &(edata->watch_point_value), &high_task_wakeup);
    }
    return (high_task_wakeup == pdTRUE);
}

// Task to handle encoder events
static void encoder_task(void *pvParameters)
{
    int event_count = 0;
    int last_count = 0;
    
    while (1) {
        // Check for watch point events
        if (encoder_queue != NULL && xQueueReceive(encoder_queue, &event_count, pdMS_TO_TICKS(10))) {
            // Determine rotation direction based on count change
            if (event_count > last_count) {
                if (enc_callback) {
                    enc_callback(ENCODER_EVENT_RIGHT, enc_user_ctx);
                }
            } else if (event_count < last_count) {
                if (enc_callback) {
                    enc_callback(ENCODER_EVENT_LEFT, enc_user_ctx);
                }
            }
            last_count = event_count;
        }
        
        // Check button press (with debouncing)
        static uint32_t last_button_time = 0;
        static bool last_button_state = false;
        uint32_t current_time = xTaskGetTickCount();
        
        // Only check button if it's configured
        if (enc_config.sw_pin != -1) {
            bool button_state = !gpio_get_level(enc_config.sw_pin); // Active low
            
            // Simple debouncing - only trigger if state stable for 50ms
            if (button_state != last_button_state && (current_time - last_button_time) > pdMS_TO_TICKS(50)) {
                if (button_state && enc_callback) {
                    enc_callback(ENCODER_EVENT_BUTTON, enc_user_ctx);
                }
                last_button_state = button_state;
                last_button_time = current_time;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool encoder_init_with_config(const encoder_config_t* config, encoder_callback_t callback, void* user_ctx)
{
    if (!config || config->a_pin == -1 || config->b_pin == -1) {
        ESP_LOGE(TAG, "Invalid encoder configuration");
        return false;
    }
    
    // Store configuration
    enc_config = *config;
    enc_callback = callback;
    enc_user_ctx = user_ctx;
    
    // Configure GPIO for switch pin if provided
    if (enc_config.sw_pin != -1) {
        gpio_config_t sw_config = {
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = 1ULL << enc_config.sw_pin,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&sw_config));
    }
    
    // Configure PCNT unit
    pcnt_unit_config_t unit_config = {
        .high_limit = enc_config.high_limit,
        .low_limit = enc_config.low_limit,
    };
    
    esp_err_t err = pcnt_new_unit(&unit_config, &pcnt_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT unit: %s", esp_err_to_name(err));
        return false;
    }
    
    // Set glitch filter
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 2000,  // Increase filter to 2000ns for better stability
    };
    pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config);
    
    // Configure PCNT channels
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = enc_config.a_pin,
        .level_gpio_num = enc_config.b_pin,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    err = pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel A: %s", esp_err_to_name(err));
        return false;
    }
    
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = enc_config.b_pin,
        .level_gpio_num = enc_config.a_pin,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    err = pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel B: %s", esp_err_to_name(err));
        return false;
    }
    
    // Set edge and level actions for PCNT channels
    pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    
    // Add watch points (avoid division by zero)
    int watch_points[5];
    int watch_count = 0;
    
    if (enc_config.high_limit != enc_config.low_limit) {
        watch_points[watch_count++] = enc_config.low_limit;
        if ((enc_config.high_limit - enc_config.low_limit) > 4) {
            watch_points[watch_count++] = enc_config.low_limit + (enc_config.high_limit - enc_config.low_limit) / 4;
            watch_points[watch_count++] = enc_config.low_limit + (enc_config.high_limit - enc_config.low_limit) / 2;
            watch_points[watch_count++] = enc_config.low_limit + 3 * (enc_config.high_limit - enc_config.low_limit) / 4;
        }
        watch_points[watch_count++] = enc_config.high_limit;
    } else {
        watch_points[watch_count++] = enc_config.low_limit;
    }
    
    for (int i = 0; i < watch_count; i++) {
        pcnt_unit_add_watch_point(pcnt_unit, watch_points[i]);
    }
    
    // Create queue for events
    encoder_queue = xQueueCreate(10, sizeof(int));
    if (!encoder_queue) {
        ESP_LOGE(TAG, "Failed to create encoder queue");
        return false;
    }
    
    // Register callbacks
    pcnt_event_callbacks_t cbs = {
        .on_reach = encoder_pcnt_on_reach,
    };
    pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, NULL);
    
    // Enable and start PCNT unit
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
    
    // Create encoder task with higher priority to prevent reboots
    BaseType_t task_result = xTaskCreate(encoder_task, "encoder_task", 2048, NULL, 10, &encoder_task_handle);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create encoder task");
        vQueueDelete(encoder_queue);
        encoder_queue = NULL;
        return false;
    }
    
    ESP_LOGI(TAG, "Encoder initialized successfully");
    return true;
}

int encoder_get_count(void)
{
    int count = 0;
    if (pcnt_unit) {
        pcnt_unit_get_count(pcnt_unit, &count);
    }
    return count;
}

void encoder_clear_count(void)
{
    if (pcnt_unit) {
        pcnt_unit_clear_count(pcnt_unit);
    }
}

bool encoder_button_pressed(void)
{
    if (enc_config.sw_pin != -1) {
        return !gpio_get_level(enc_config.sw_pin); // Active low
    }
    return false;
}