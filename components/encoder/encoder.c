#include "encoder.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

// Количество импульсов PCNT на один реальный шаг энкодера (для типичного энкодера 2 импульса на щелчок)
static const int32_t COUNT_FILTER = 4;

// Накопитель импульсов PCNT, чтобы не терять половинчатые шаги при очистке счетчика
static int32_t accumulated_delta = 0;

// Encoder pin definitions (will be passed from main app)
static int enc_a_pin = -1;
static int enc_b_pin = -1;
static int enc_sw_pin = -1;

// Encoder configuration
static uint32_t long_press_duration_ms = 1000; // Default 1 second for long press

// Pulse Counter (PCNT) handles
static pcnt_unit_handle_t pcnt_unit = NULL;
static pcnt_channel_handle_t pcnt_chan_a = NULL;
static pcnt_channel_handle_t pcnt_chan_b = NULL;

// Event queue for encoder events
static QueueHandle_t encoder_event_queue = NULL;
#define ENCODER_QUEUE_SIZE 10

// Button state tracking
static bool button_pressed = false;
static int64_t button_press_time = 0;
static bool long_press_detected = false;

// Forward declarations

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
    
    // Configure GPIOs for encoder inputs (enable pull-ups)
    gpio_config_t enc_io = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << enc_a_pin) | (1ULL << enc_b_pin),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&enc_io);

    // Create PCNT unit
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    if (pcnt_new_unit(&unit_config, &pcnt_unit) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT unit");
        return;
    }

    // Create channels for quadrature decoding
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = enc_a_pin,
        .level_gpio_num = enc_b_pin,
    };
    if (pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel A");
        return;
    }
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = enc_b_pin,
        .level_gpio_num = enc_a_pin,
    };
    if (pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel B");
        return;
    }

    // Set edge and level actions for quadrature
    pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    
    // Включаем фильтр дребезга, чтобы отсеять шумовые импульсы от механического энкодера
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000,  // ~1 мкс достаточно для подавления дребезга
    };
    esp_err_t filter_ret = pcnt_unit_set_glitch_filter(pcnt_unit, &filter_cfg);
    if (filter_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set PCNT glitch filter: %s", esp_err_to_name(filter_ret));
    }
    
    // Enable and start PCNT
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
    
    // Configure GPIO for encoder button with interrupt
    // Only trigger on falling edge (button press) to reduce interrupt frequency
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
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
    // The button task will handle the actual button logic
    // We just need to make sure it runs when the button state changes
    // The task will check the current button state
}

static void encoder_button_task(void *arg)
{
    // Add debouncing variables
    static bool last_button_state = true; // Assume button is released initially (active low)
    static int64_t last_button_change_time = 0;
    const int64_t debounce_delay_ms = 50; // 50ms debounce time
    
    while (1) {
        // Check button state
        bool current_button_state = gpio_get_level(enc_sw_pin);
        
        // Get current time
        int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        
        // Check if button state has changed and enough time has passed for debouncing
        if (current_button_state != last_button_state && 
            (current_time - last_button_change_time) > debounce_delay_ms) {
            ESP_LOGD(TAG, "Button state changed: %s (debounced)", current_button_state ? "RELEASED" : "PRESSED");
            
            // Update last button change time
            last_button_change_time = current_time;
            last_button_state = current_button_state;
            ESP_LOGD(TAG, "Button state updated, waiting for stable state");
            
            // Button pressed (active low)
            if (!current_button_state && !button_pressed) {
                button_pressed = true;
                button_press_time = current_time;
                long_press_detected = false;
                ESP_LOGD(TAG, "Button pressed at %lld ms", button_press_time);
            }
            // Button released
            else if (current_button_state && button_pressed) {
                int64_t press_duration = current_time - button_press_time;
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
        }
        // Button still pressed - check for long press
        else if (button_pressed && !long_press_detected) {
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
        int count = 0;
        if (pcnt_unit_get_count(pcnt_unit, &count) == ESP_OK && count != 0) {
            accumulated_delta += count;
            pcnt_unit_clear_count(pcnt_unit);
        }

        // Генерируем события только тогда, когда накопилось достаточно импульсов для одного шага
        while (abs(accumulated_delta) >= COUNT_FILTER) {
            encoder_event_t event = {
                .type = (accumulated_delta > 0) ? ENCODER_EVENT_ROTATE_CW : ENCODER_EVENT_ROTATE_CCW,
                .value = 1
            };

            if (encoder_event_queue != NULL) {
                if (xQueueSend(encoder_event_queue, &event, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "Encoder queue full, dropping rotation event");
                    break;
                }
            }

            if (accumulated_delta > 0) {
                accumulated_delta -= COUNT_FILTER;
            } else {
                accumulated_delta += COUNT_FILTER;
            }
            ESP_LOGD(TAG, "PCNT step sent, accumulated=%ld", (long)accumulated_delta);
        }

        if (count == 0) {
            // Периодически логируем уровни для отладки состояния пинов
            static int tick = 0;
            if ((++tick % 50) == 0) {
                int a = gpio_get_level(enc_a_pin);
                int b = gpio_get_level(enc_b_pin);
                ESP_LOGD(TAG, "Levels A=%d B=%d, accumulated=%ld", a, b, (long)accumulated_delta);
            }
        }
        
        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
