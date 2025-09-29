#include "encoder.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

// Encoder pin definitions (will be passed from main app)
static int enc_a_pin = -1;
static int enc_b_pin = -1;
static int enc_sw_pin = -1;

// Encoder configuration
static uint32_t long_press_duration_ms = 1000; // Default 1 second for long press

// PCNT unit for encoder
#define PCNT_ENCODER_UNIT PCNT_UNIT_0
#define PCNT_ENCODER_CHANNEL_A PCNT_CHANNEL_0
#define PCNT_ENCODER_CHANNEL_B PCNT_CHANNEL_1

// Event queue for encoder events
static QueueHandle_t encoder_event_queue = NULL;
#define ENCODER_QUEUE_SIZE 10

// Button state tracking
static bool button_pressed = false;
static int64_t button_press_time = 0;
static bool long_press_detected = false;

// Forward declarations
static void encoder_gpio_isr_handler(void *arg);
static void encoder_button_isr_handler(void *arg);
static void encoder_button_task(void *arg);

void encoder_set_pins(int a_pin, int b_pin, int sw_pin)
{
    enc_a_pin = a_pin;
    enc_b_pin = b_pin;
    enc_sw_pin = sw_pin;
}

void encoder_set_long_press_duration(uint32_t duration_ms)
{
    long_press_duration_ms = duration_ms;
}

QueueHandle_t encoder_get_event_queue(void)
{
    return encoder_event_queue;
}

void encoder_init(void)
{
    if (enc_a_pin == -1 || enc_b_pin == -1 || enc_sw_pin == -1) {
        ESP_LOGE(TAG, "Encoder pins not set");
        return;
    }
    
    // Create event queue
    encoder_event_queue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
    if (encoder_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create encoder event queue");
        return;
    }
    
    // Configure PCNT for encoder rotation
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = enc_a_pin,
        .ctrl_gpio_num = enc_b_pin,
        .channel = PCNT_ENCODER_CHANNEL_A,
        .unit = PCNT_ENCODER_UNIT,
        .pos_mode = PCNT_COUNT_DIS,
        .neg_mode = PCNT_COUNT_INC,
        .lctrl_mode = PCNT_MODE_REVERSE,
        .hctrl_mode = PCNT_MODE_KEEP,
        .counter_h_lim = 100,
        .counter_l_lim = -100,
    };
    pcnt_unit_config(&pcnt_config);
    
    pcnt_config.pulse_gpio_num = enc_b_pin;
    pcnt_config.ctrl_gpio_num = enc_a_pin;
    pcnt_config.channel = PCNT_ENCODER_CHANNEL_B;
    pcnt_unit_config(&pcnt_config);
    
    // Filter out glitches
    pcnt_set_filter_value(PCNT_ENCODER_UNIT, 100);
    pcnt_filter_enable(PCNT_ENCODER_UNIT);
    
    // Enable PCNT
    pcnt_counter_pause(PCNT_ENCODER_UNIT);
    pcnt_counter_clear(PCNT_ENCODER_UNIT);
    pcnt_counter_resume(PCNT_ENCODER_UNIT);
    
    // Configure GPIO for encoder button with interrupt
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << enc_sw_pin),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Hook ISR handlers
    gpio_isr_handler_add(enc_sw_pin, encoder_button_isr_handler, (void *)enc_sw_pin);
    
    // Create button handling task
    xTaskCreate(encoder_button_task, "encoder_button", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Encoder initialized with pins A:%d, B:%d, SW:%d", enc_a_pin, enc_b_pin, enc_sw_pin);
}

static void encoder_button_isr_handler(void *arg)
{
    // This is a simple ISR that just wakes up the button task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // The button task will handle the actual button logic
    // We just need to make sure it runs when the button state changes
    // The task will check the current button state
}

static void encoder_button_task(void *arg)
{
    while (1) {
        // Check button state
        bool current_button_state = gpio_get_level(enc_sw_pin);
        
        // Button pressed (active low)
        if (!current_button_state && !button_pressed) {
            button_pressed = true;
            button_press_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            long_press_detected = false;
            ESP_LOGD(TAG, "Button pressed at %lld ms", button_press_time);
        }
        // Button released
        else if (current_button_state && button_pressed) {
            int64_t button_release_time = esp_timer_get_time() / 1000;
            int64_t press_duration = button_release_time - button_press_time;
            
            button_pressed = false;
            
            // Check if long press was already detected
            if (long_press_detected) {
                // Long press already reported, just send release event
                encoder_event_t event = {
                    .type = ENCODER_EVENT_BUTTON_RELEASE,
                    .value = 0
                };
                if (encoder_event_queue != NULL) {
                    xQueueSend(encoder_event_queue, &event, 0);
                }
            } else {
                // Short press - for LVGL encoder, we typically just send press and release
                encoder_event_t event = {
                    .type = ENCODER_EVENT_BUTTON_PRESS,
                    .value = press_duration
                };
                if (encoder_event_queue != NULL) {
                    xQueueSend(encoder_event_queue, &event, 0);
                }
                
                // Also send release event
                event.type = ENCODER_EVENT_BUTTON_RELEASE;
                event.value = 0;
                xQueueSend(encoder_event_queue, &event, 0);
            }
            
            long_press_detected = false;
            ESP_LOGD(TAG, "Button released after %lld ms", press_duration);
        }
        // Button still pressed - check for long press
        else if (button_pressed && !long_press_detected) {
            int64_t current_time = esp_timer_get_time() / 1000;
            int64_t press_duration = current_time - button_press_time;
            
            if (press_duration >= long_press_duration_ms) {
                long_press_detected = true;
                encoder_event_t event = {
                    .type = ENCODER_EVENT_BUTTON_LONG_PRESS,
                    .value = press_duration
                };
                if (encoder_event_queue != NULL) {
                    xQueueSend(encoder_event_queue, &event, 0);
                }
                ESP_LOGD(TAG, "Long press detected after %lld ms", press_duration);
            }
        }
        
        // Check for encoder rotation
        int16_t count = 0;
        pcnt_get_counter_value(PCNT_ENCODER_UNIT, &count);
        if (count != 0) {
            pcnt_counter_clear(PCNT_ENCODER_UNIT);
            
            encoder_event_t event = {
                .type = (count > 0) ? ENCODER_EVENT_ROTATE_CW : ENCODER_EVENT_ROTATE_CCW,
                .value = abs(count)
            };
            
            if (encoder_event_queue != NULL) {
                xQueueSend(encoder_event_queue, &event, 0);
            }
            
            ESP_LOGD(TAG, "Encoder rotated: %s (%d)", 
                     (count > 0) ? "CW" : "CCW", abs(count));
        }
        
        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}