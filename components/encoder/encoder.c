#include "encoder.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

// Количество импульсов PCNT на один реальный шаг энкодера (для типичного энкодера 4 импульса на щелчок для лучшей стабильности)
static const int32_t COUNT_FILTER = 4;

// Накопитель импульсов PCNT, чтобы не терять половинчатые шаги при очистке счетчика
static int32_t accumulated_delta = 0;

// Время последнего события поворота для дебаунсинга
static int64_t last_rotation_time = 0;
static const int64_t ROTATION_DEBOUNCE_MS = 50; // Минимальный интервал между событиями поворота

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
#define ENCODER_QUEUE_SIZE 50

// Button state tracking
static bool button_pressed = false;
static int64_t button_press_time = 0;
static bool long_press_detected = false;

// Semaphores for button handling
static SemaphoreHandle_t button_press_sem = NULL;
static SemaphoreHandle_t button_release_sem = NULL;

// Forward declarations

static void encoder_button_isr_handler(void *arg);
static void encoder_button_task(void *arg);
static void encoder_rotation_task(void *arg);

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
    
    // Create semaphores for button handling
    button_press_sem = xSemaphoreCreateBinary();
    button_release_sem = xSemaphoreCreateBinary();
    if (button_press_sem == NULL || button_release_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create button semaphores");
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
    // Trigger on any edge (both press and release) for proper button handling
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
    xTaskCreate(encoder_button_task, "encoder_button", 4096, NULL, 5, NULL);
    
    // Create rotation task
    xTaskCreate(encoder_rotation_task, "encoder_rotation", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Encoder initialized with pins A:%d, B:%d, SW:%d", enc_a_pin, enc_b_pin, enc_sw_pin);
}

static void encoder_button_isr_handler(void *arg)
{
    // Проверяем состояние кнопки
    bool current_state = gpio_get_level(enc_sw_pin);
    
    // В ISR нельзя использовать ESP_LOGI - это вызывает блокировку!
    // Убираем все логирование из ISR
    
    if (current_state == 0) { // Кнопка нажата (active low)
        // Даем семафор нажатия
        if (button_press_sem != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(button_press_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else { // Кнопка отпущена
        // Даем семафор отпускания
        if (button_release_sem != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(button_release_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

static void encoder_button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started");
    
    while (1) {
        // Ждем семафор нажатия
        if (xSemaphoreTake(button_press_sem, portMAX_DELAY) == pdTRUE) {
            // Кнопка нажата
            button_pressed = true;
            button_press_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            long_press_detected = false;
            
            ESP_LOGI(TAG, "Button pressed");
            
            // НЕ отправляем событие нажатия сразу - ждем отпускания!
            
            // Ждем отпускания или длинного нажатия
            while (button_pressed) {
                // Проверяем на длинное нажатие
                if (!long_press_detected) {
                    int64_t current_time = esp_timer_get_time() / 1000;
                    int64_t press_duration = current_time - button_press_time;
                    
                    if (press_duration >= long_press_duration_ms) {
                        long_press_detected = true;
                        
                        ESP_LOGI(TAG, "Button long press detected");
                        
                        // Отправляем событие длинного нажатия
                        encoder_event_t long_event = {
                            .type = ENCODER_EVENT_BUTTON_LONG_PRESS,
                            .value = 1
                        };
                        xQueueSend(encoder_event_queue, &long_event, 0);
                    }
                }
                
                // Проверяем семафор отпускания с коротким таймаутом
                if (xSemaphoreTake(button_release_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
                    // Кнопка отпущена
                    button_pressed = false;
                    
                    ESP_LOGI(TAG, "Button released");
                    
                    // Отправляем событие PRESS только при отпускании, если не было длинного нажатия
                    if (!long_press_detected) {
                        encoder_event_t press_event = {
                            .type = ENCODER_EVENT_BUTTON_PRESS,
                            .value = 1
                        };
                        xQueueSend(encoder_event_queue, &press_event, 0);
                        ESP_LOGI(TAG, "Short press event sent on release");
                    }
                    
                    // Отправляем событие отпускания
                    encoder_event_t release_event = {
                        .type = ENCODER_EVENT_BUTTON_RELEASE,
                        .value = 1
                    };
                    xQueueSend(encoder_event_queue, &release_event, 0);
                    break;
                }
            }
        }
    }
}

static void encoder_rotation_task(void *arg)
{
    ESP_LOGI(TAG, "Rotation task started");
    
    while (1) {
        // Check for encoder rotation
        int count = 0;
        if (pcnt_unit_get_count(pcnt_unit, &count) == ESP_OK && count != 0) {
            accumulated_delta += count;
            pcnt_unit_clear_count(pcnt_unit);
        }

        // Генерируем события только тогда, когда накопилось достаточно импульсов для одного шага
        while (abs(accumulated_delta) >= COUNT_FILTER) {
            int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            
            // Проверяем дебаунсинг - отправляем событие только если прошло достаточно времени
            if (current_time - last_rotation_time >= ROTATION_DEBOUNCE_MS) {
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
                
                last_rotation_time = current_time;
                ESP_LOGD(TAG, "PCNT step sent, accumulated=%ld", (long)accumulated_delta);
            }

            if (accumulated_delta > 0) {
                accumulated_delta -= COUNT_FILTER;
            } else {
                accumulated_delta += COUNT_FILTER;
            }
        }

        // Убираем избыточное логирование для предотвращения переполнения стека
        
        // Small delay to prevent excessive CPU usage and add debouncing
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
