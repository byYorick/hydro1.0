#include "trema_relay.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "trema_relay";

// Stub values for when relay is not connected
static bool use_stub_values = false;

// Buffer for I2C communication
static uint8_t data[8];

// Relay initialization flag
static bool relay_initialized = false;

// Model type (0x0A for 2-channel relay, 0x0B for 4-channel SSR)
static uint8_t relay_model = 0x00;

// Digital register cache to minimize I2C traffic
static uint8_t digital_reg = 0x00;

// Watchdog timer state
static bool wdt_enabled = false;
static uint8_t wdt_timeout = 0;

// Auto-switching variables
static bool auto_switch_enabled = false;
static TaskHandle_t auto_switch_task_handle = NULL;

bool trema_relay_init(void)
{
    ESP_LOGI(TAG, "Initializing trema relay at address 0x%02X", TREMA_RELAY_ADDR);
    
    // Try to communicate with the relay
    // Read the model register to verify relay presence
    data[0] = 0x04; // REG_MODEL
    ESP_LOGD(TAG, "Writing to register 0x%02X", data[0]);
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to I2C relay at address 0x%02X", TREMA_RELAY_ADDR);
        use_stub_values = true;
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ESP_LOGD(TAG, "Reading model ID from relay");
    if (i2c_bus_read(TREMA_RELAY_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from I2C relay at address 0x%02X", TREMA_RELAY_ADDR);
        use_stub_values = true;
        return false;
    }
    
    ESP_LOGI(TAG, "Received model ID: 0x%02X", data[0]);
    
    // Check if we got a valid response
    // For iarduino relay, model ID should be 0x0A (2-channel) or 0x0B (4-channel SSR)
    if (data[0] != 0x0A && data[0] != 0x0B) {
        ESP_LOGW(TAG, "Invalid relay model ID: 0x%02X (expected 0x0A or 0x0B)", data[0]);
        use_stub_values = true;
        return false;
    }
    
    relay_model = data[0];
    relay_initialized = true;
    use_stub_values = false;
    ESP_LOGI(TAG, "I2C relay (model 0x%02X) initialized successfully", relay_model);
    return true;
}

void trema_relay_digital_write(uint8_t channel, uint8_t value)
{
    // Validate channel number based on model
    uint8_t max_channel = (relay_model == 0x0B) ? 3 : 1; // 4-channel SSR vs 2-channel relay
    if (channel > max_channel) {
        ESP_LOGW(TAG, "Invalid channel number: %d (max: %d)", channel, max_channel);
        return;
    }
    
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return;
    }
    
    // Update digital register cache
    if (value) {
        digital_reg |= (1 << channel);     // Set bit to 1 (high)
    } else {
        digital_reg &= ~(1 << channel);    // Clear bit to 0 (low)
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write to the appropriate register based on value
    if (value) {
        // Set bit high
        data[0] = REG_REL_DIGITAL_ONE;
        data[1] = (1 << (channel + 4));  // Set high bits for this channel
    } else {
        // Set bit low
        data[0] = REG_REL_DIGITAL_ONE;
        data[1] = (1 << channel);        // Set low bits for this channel
    }
    
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write digital value to relay");
    }
}

uint8_t trema_relay_digital_read(uint8_t channel)
{
    // Validate channel number based on model
    uint8_t max_channel = (relay_model == 0x0B) ? 3 : 1; // 4-channel SSR vs 2-channel relay
    if (channel > max_channel) {
        ESP_LOGW(TAG, "Invalid channel number: %d (max: %d)", channel, max_channel);
        return 0;
    }
    
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return 0;
    }
    
    // If using stub values, return 0
    if (use_stub_values) {
        return 0;
    }
    
    // Read digital register
    data[0] = REG_REL_DIGITAL_ALL;
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read digital register");
        return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    if (i2c_bus_read(TREMA_RELAY_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read digital value");
        return 0;
    }
    
    // Return the value of the specific bit
    return (data[0] >> channel) & 0x01;
}

bool trema_relay_enable_wdt(uint8_t timeout)
{
    // Validate timeout range
    if (timeout < 1 || timeout > 254) {
        ESP_LOGW(TAG, "Invalid WDT timeout: %d (must be 1-254)", timeout);
        return false;
    }
    
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return false;
    }
    
    // If using stub values, just return true
    if (use_stub_values) {
        wdt_enabled = true;
        wdt_timeout = timeout;
        return true;
    }
    
    // Write timeout to WDT register
    data[0] = REG_REL_WDT;
    data[1] = timeout;
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable WDT");
        return false;
    }
    
    wdt_enabled = true;
    wdt_timeout = timeout;
    ESP_LOGD(TAG, "WDT enabled with timeout: %d seconds", timeout);
    return true;
}

bool trema_relay_disable_wdt(void)
{
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return false;
    }
    
    // If using stub values, just return true
    if (use_stub_values) {
        wdt_enabled = false;
        wdt_timeout = 0;
        return true;
    }
    
    // Write 0 to WDT register to disable
    data[0] = REG_REL_WDT;
    data[1] = 0;
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disable WDT");
        return false;
    }
    
    wdt_enabled = false;
    wdt_timeout = 0;
    ESP_LOGD(TAG, "WDT disabled");
    return true;
}

bool trema_relay_reset_wdt(void)
{
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return false;
    }
    
    // If WDT is not enabled, nothing to reset
    if (!wdt_enabled) {
        ESP_LOGD(TAG, "WDT not enabled, nothing to reset");
        return true;
    }
    
    // If using stub values, just return true
    if (use_stub_values) {
        return true;
    }
    
    // Rewrite the timeout value to reset the WDT
    data[0] = REG_REL_WDT;
    data[1] = wdt_timeout;
    if (i2c_bus_write(TREMA_RELAY_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to reset WDT");
        return false;
    }
    
    ESP_LOGD(TAG, "WDT reset");
    return true;
}

bool trema_relay_get_state_wdt(void)
{
    return wdt_enabled;
}

bool trema_relay_is_using_stub_values(void)
{
    return use_stub_values;
}

// Auto-switching task function
static void auto_switch_task(void *pvParameters)
{
    uint8_t current_channel = 0;
    
    // For 4-channel relay, cycle through channels 0-3
    // For 2-channel relay, cycle through channels 0-1
    uint8_t max_channel = (relay_model == 0x0B) ? 3 : 1;
    
    while (auto_switch_enabled) {
        // Turn off all channels first
        for (int i = 0; i <= max_channel; i++) {
            trema_relay_digital_write(i, LOW);
        }
        
        // Turn on current channel
        trema_relay_digital_write(current_channel, HIGH);
        
        // Log which channel is active
        ESP_LOGD(TAG, "Auto-switch: Channel %d activated", current_channel);
        
        // Move to next channel
        current_channel = (current_channel + 1) % (max_channel + 1);
        
        // Wait for 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // Turn off all channels when stopping
    for (int i = 0; i <= max_channel; i++) {
        trema_relay_digital_write(i, LOW);
    }
    
    auto_switch_task_handle = NULL;
    vTaskDelete(NULL);
}

void trema_relay_auto_switch(bool enable)
{
    // Check if relay is initialized
    if (!relay_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Relay not initialized");
        return;
    }
    
    // If enabling and not already enabled
    if (enable && !auto_switch_enabled) {
        auto_switch_enabled = true;
        
        // Create auto-switch task
        BaseType_t result = xTaskCreate(auto_switch_task, "relay_auto_switch", 2048, NULL, 5, &auto_switch_task_handle);
        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create auto-switch task");
            auto_switch_enabled = false;
            auto_switch_task_handle = NULL;
        } else {
            ESP_LOGI(TAG, "Auto-switch started");
        }
    }
    // If disabling and currently enabled
    else if (!enable && auto_switch_enabled) {
        auto_switch_enabled = false;
        
        // Task will clean itself up
        if (auto_switch_task_handle != NULL) {
            // Give the task time to clean up
            vTaskDelay(pdMS_TO_TICKS(100));
            auto_switch_task_handle = NULL;
        }
        
        ESP_LOGI(TAG, "Auto-switch stopped");
    }
}