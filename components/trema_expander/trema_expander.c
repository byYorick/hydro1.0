#include "trema_expander.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "trema_expander";

// Stub values for when expander is not connected
static bool use_stub_values = false;

// Buffer for I2C communication
static uint8_t data[16];

// Expander initialization flag
static bool expander_initialized = false;

// Register caches to minimize I2C traffic
static uint8_t direction_reg = 0x00;   // All pins input by default
static uint8_t type_reg = 0x00;        // All pins digital by default
static uint8_t pull_up_reg = 0x00;     // No pull-ups by default
static uint8_t pull_down_reg = 0x00;   // No pull-downs by default
static uint8_t out_mode_reg = 0x00;    // All push-pull by default
static uint8_t digital_reg = 0x00;     // All low by default

bool trema_expander_init(void)
{
    // Try to communicate with the expander
    // Read the model register to verify expander presence
    data[0] = 0x04; // REG_MODEL
    if (i2c_bus_write(TREMA_EXPANDER_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write to I2C expander");
        use_stub_values = true;
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (i2c_bus_read(TREMA_EXPANDER_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read from I2C expander");
        use_stub_values = true;
        return false;
    }
    
    // Check if we got a valid response
    // For iarduino expander, model ID should be 0x07
    if (data[0] != 0x07) {
        ESP_LOGW(TAG, "Invalid expander model ID: 0x%02X", data[0]);
        use_stub_values = true;
        return false;
    }
    
    expander_initialized = true;
    use_stub_values = false;
    ESP_LOGI(TAG, "I2C expander initialized successfully");
    return true;
}

void trema_expander_pin_mode(uint8_t pin, uint8_t mode, uint8_t type)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // Update direction register
    if (mode == OUTPUT) {
        direction_reg |= (1 << pin);  // Set bit to 1 (output)
    } else {
        direction_reg &= ~(1 << pin); // Clear bit to 0 (input)
    }
    
    // Update type register
    if (type == ANALOG) {
        type_reg |= (1 << pin);       // Set bit to 1 (analog)
    } else {
        type_reg &= ~(1 << pin);      // Clear bit to 0 (digital)
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write direction register
    data[0] = REG_EXP_DIRECTION;
    data[1] = direction_reg;
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
    
    // Write type register
    data[0] = REG_EXP_TYPE;
    data[1] = type_reg;
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
}

void trema_expander_pin_pull(uint8_t pin, uint8_t pull)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // Update pull registers based on pull type
    switch (pull) {
        case PULL_UP:
            pull_up_reg |= (1 << pin);     // Enable pull-up
            pull_down_reg &= ~(1 << pin);  // Disable pull-down
            break;
        case PULL_DOWN:
            pull_down_reg |= (1 << pin);   // Enable pull-down
            pull_up_reg &= ~(1 << pin);    // Disable pull-up
            break;
        case PULL_NO:
        default:
            pull_up_reg &= ~(1 << pin);    // Disable pull-up
            pull_down_reg &= ~(1 << pin);  // Disable pull-down
            break;
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write pull-up register
    data[0] = REG_EXP_PULL_UP;
    data[1] = pull_up_reg;
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
    
    // Write pull-down register
    data[0] = REG_EXP_PULL_DOWN;
    data[1] = pull_down_reg;
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
}

void trema_expander_pin_out_scheme(uint8_t pin, uint8_t scheme)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // Update output mode register
    if (scheme == OUT_OPEN_DRAIN) {
        out_mode_reg |= (1 << pin);    // Set bit to 1 (open drain)
    } else {
        out_mode_reg &= ~(1 << pin);   // Clear bit to 0 (push-pull)
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write output mode register
    data[0] = REG_EXP_OUT_MODE;
    data[1] = out_mode_reg;
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
}

void trema_expander_digital_write(uint8_t pin, uint8_t value)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // Update digital register cache
    if (value) {
        digital_reg |= (1 << pin);     // Set bit to 1 (high)
    } else {
        digital_reg &= ~(1 << pin);    // Clear bit to 0 (low)
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write to the appropriate register based on value
    if (value) {
        data[0] = REG_EXP_WRITE_HIGH;
    } else {
        data[0] = REG_EXP_WRITE_LOW;
    }
    data[1] = (1 << pin);  // Only set the bit for this pin
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 2);
}

uint8_t trema_expander_digital_read(uint8_t pin)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return 0;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return 0;
    }
    
    // If using stub values, return 0
    if (use_stub_values) {
        return 0;
    }
    
    // Read digital register
    data[0] = REG_EXP_DIGITAL;
    if (i2c_bus_write(TREMA_EXPANDER_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read digital register");
        return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    if (i2c_bus_read(TREMA_EXPANDER_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read digital value");
        return 0;
    }
    
    // Return the value of the specific bit
    return (data[0] >> pin) & 0x01;
}

void trema_expander_analog_write(uint8_t pin, uint16_t value)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return;
    }
    
    // Validate value range (0-4095 for 12-bit PWM)
    if (value > 4095) {
        ESP_LOGW(TAG, "Invalid analog value: %d", value);
        return;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // For analog write, we need to use PWM functionality
    // This would typically involve setting up PWM registers
    // For now, we'll just treat it as a digital write if value > 0
    trema_expander_digital_write(pin, value > 0 ? 1 : 0);
    
    ESP_LOGD(TAG, "Analog write not fully implemented, using digital write instead");
}

uint16_t trema_expander_analog_read(uint8_t pin)
{
    // Validate pin number
    if (pin > 7) {
        ESP_LOGW(TAG, "Invalid pin number: %d", pin);
        return 0;
    }
    
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return 0;
    }
    
    // If using stub values, return 0
    if (use_stub_values) {
        return 0;
    }
    
    // Read analog register (2 bytes per pin, starting at REG_EXP_ANALOG)
    data[0] = REG_EXP_ANALOG + (pin * 2);  // Each pin uses 2 bytes
    if (i2c_bus_write(TREMA_EXPANDER_ADDR, data, 1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read analog register");
        return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    if (i2c_bus_read(TREMA_EXPANDER_ADDR, data, 2) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read analog value");
        return 0;
    }
    
    // Combine the two bytes into a 16-bit value
    return ((uint16_t)data[1] << 8) | data[0];
}

void trema_expander_freq_pwm(uint16_t frequency)
{
    // Check if expander is initialized
    if (!expander_initialized && !use_stub_values) {
        ESP_LOGW(TAG, "Expander not initialized");
        return;
    }
    
    // If using stub values, just return
    if (use_stub_values) {
        return;
    }
    
    // Write frequency to registers (2 bytes)
    data[0] = 0x08; // REG_EXP_FREQUENCY_L
    data[1] = frequency & 0xFF;         // Low byte
    data[2] = (frequency >> 8) & 0xFF;  // High byte
    i2c_bus_write(TREMA_EXPANDER_ADDR, data, 3);
}

bool trema_expander_is_using_stub_values(void)
{
    return use_stub_values;
}