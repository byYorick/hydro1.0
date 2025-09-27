#include "xpt2046.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// Forward declarations for LVGL lock/unlock functions
bool lvgl_lock(int timeout_ms);
void lvgl_unlock(void);

static const char *TAG = "xpt2046";

// SPI device handle
static spi_device_handle_t spi_touch_handle = NULL;

// Touch calibration values
static uint16_t min_raw_x = XPT2046_MIN_RAW_X;
static uint16_t max_raw_x = XPT2046_MAX_RAW_X;
static uint16_t min_raw_y = XPT2046_MIN_RAW_Y;
static uint16_t max_raw_y = XPT2046_MAX_RAW_Y;

// Touch detection threshold
static const uint16_t PRESS_THRESHOLD = 150;  // Reduced from 300 to 150 for better sensitivity

// Touch debounce parameters
static const uint16_t STUCK_TOUCH_THRESHOLD = 10;  // Number of consecutive stuck readings to ignore
static uint16_t stuck_touch_count = 0;            // Counter for consecutive stuck readings
static uint16_t last_valid_x = 0;                 // Last valid X coordinate
static uint16_t last_valid_y = 0;                 // Last valid Y coordinate

// Using the same SPI host as the LCD
#define XPT2046_HOST SPI2_HOST

// Forward declaration
static uint16_t xpt2046_read_register(uint8_t command);

bool xpt2046_init(void)
{
    ESP_LOGI(TAG, "Initializing XPT2046 touch controller");
    
    // Configure SPI device for touch controller
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,                          // SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = XPT2046_CLOCK_SPEED_HZ,
        .input_delay_ns = 0,
        .spics_io_num = 5,                  // Chip select pin
        .flags = SPI_DEVICE_NO_DUMMY,       // Add this flag for shared SPI bus
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL,
    };
    
    ESP_LOGI(TAG, "Configuring SPI device with CS pin %d and clock speed %d Hz", 
             devcfg.spics_io_num, devcfg.clock_speed_hz);
    
    // Try to add device to the existing SPI bus (LCD should initialize the bus first)
    esp_err_t ret = spi_bus_add_device(XPT2046_HOST, &devcfg, &spi_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add SPI device to existing bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Small delay to ensure device is ready
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Configure interrupt pin
    gpio_config_t irq_config = {
        .pin_bit_mask = (1ULL << 4),        // IRQ pin
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // Enable pull-up for IRQ
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    ESP_LOGI(TAG, "Configuring IRQ pin %d as input with pull-up", 4);
    
    ret = gpio_config(&irq_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure IRQ pin: %s", esp_err_to_name(ret));
        spi_bus_remove_device(spi_touch_handle);
        spi_touch_handle = NULL;
        return false;
    }
    
    // Test communication by reading a register
    uint16_t test_value = xpt2046_read_register(0xB0); // Read Z1 position register
    ESP_LOGI(TAG, "XPT2046 communication test - Z1 register response: 0x%04X", test_value);
    
    // Additional test by reading Z2 register
    uint16_t test_value2 = xpt2046_read_register(0xC0); // Read Z2 position register
    ESP_LOGI(TAG, "XPT2046 communication test - Z2 register response: 0x%04X", test_value2);
    
    // Check if we got valid responses
    if (test_value == 0 && test_value2 == 0) {
        ESP_LOGW(TAG, "Warning: Zero values from touch controller registers - possible communication issue");
    }
    
    ESP_LOGI(TAG, "XPT2046 touch controller initialized successfully");
    return true;
}

void xpt2046_deinit(void)
{
    if (spi_touch_handle != NULL) {
        spi_bus_remove_device(spi_touch_handle);
        spi_touch_handle = NULL;
    }
    
    ESP_LOGI(TAG, "XPT2046 touch controller deinitialized");
}

static uint16_t xpt2046_read_register(uint8_t command)
{
    if (spi_touch_handle == NULL) {
        ESP_LOGW(TAG, "SPI handle is NULL - touch controller not initialized properly");
        return 0;
    }
    
    ESP_LOGD(TAG, "Reading register 0x%02X", command);
    
    // Acquire LVGL mutex to ensure exclusive SPI bus access
    if (!lvgl_lock(100)) {  // Wait up to 100ms for the lock
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for SPI access");
        return 0;
    }
    
    // Create a combined transaction to avoid rx > tx length issue
    uint8_t tx_data[3] = {command, 0x00, 0x00};  // Command + 2 dummy bytes
    uint8_t rx_data[3] = {0};                    // Response buffer
    
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA, // Use tx_data and rx_data directly
        .cmd = 0,
        .addr = 0,
        .length = 24,           // 3 bytes * 8 bits
        .rxlength = 0,          // Set to 0 when using SPI_TRANS_USE_RXDATA
        .user = NULL,
        .tx_data = {command, 0x00, 0x00},  // Directly use tx_data
        .rx_data = {0},                    // Directly use rx_data
    };
    
    // Perform the transaction
    esp_err_t ret = spi_device_polling_transmit(spi_touch_handle, &t);
    
    // Release LVGL mutex
    lvgl_unlock();
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read register 0x%02X: %s", command, esp_err_to_name(ret));
        return 0;
    }
    
    // Extract the 12-bit value from the response (rx_data[1] and rx_data[2])
    // XPT2046 returns data in the format: [command_response, data_high, data_low]
    uint16_t response = ((uint16_t)t.rx_data[1] << 8) | t.rx_data[2];
    
    // Convert 12-bit value (MSB first)
    uint16_t result = (response >> 3) & 0x0FFF;
    ESP_LOGD(TAG, "Register 0x%02X read: raw=0x%04X, converted=%d", command, response, result);
    
    return result;
}

bool xpt2046_is_touched(void)
{
    if (spi_touch_handle == NULL) {
        ESP_LOGW(TAG, "SPI handle is NULL in is_touched function");
        return false;
    }
    
    ESP_LOGD(TAG, "Reading Z1 register for touch detection...");
    // Check if touch is detected by reading Z1 value
    uint16_t z1 = xpt2046_read_register(XPT2046_CMD_Z1POS);
    
    // Also read Z2 to verify touch consistency
    uint16_t z2 = xpt2046_read_register(XPT2046_CMD_Z2POS);
    
    ESP_LOGD(TAG, "Touch detection values - Z1: %d, Z2: %d, Threshold: %d", z1, z2, PRESS_THRESHOLD);
    
    // Additional check for stuck touch - if Z1 is at maximum, it's likely stuck
    if (z1 >= 4000 || z2 >= 4000) {
        ESP_LOGD(TAG, "Possible stuck touch detected (Z1=%d, Z2=%d), ignoring", z1, z2);
        return false;
    }
    
    // Check for invalid low values
    if (z1 <= 10 || z2 <= 10) {
        ESP_LOGD(TAG, "Possible no touch detected (Z1=%d, Z2=%d), ignoring", z1, z2);
        return false;
    }
    
    // Check if touch is detected with a more robust method
    // Add upper bound to avoid stuck touch and check Z2 consistency
    bool touched = (z1 > PRESS_THRESHOLD) && (z1 < 3500) && (z2 < 3500);
    
    // Additional consistency check - Z1 and Z2 should be somewhat close
    if (touched) {
        int diff = (int)z1 - (int)z2;
        if (diff < 0) diff = -diff;  // Absolute difference
        if (diff > 1000) {  // If difference is too large, likely a false touch
            ESP_LOGD(TAG, "Z1 and Z2 values inconsistent (Z1=%d, Z2=%d, diff=%d), ignoring", z1, z2, diff);
            return false;
        }
    }
    
    ESP_LOGD(TAG, "Touch detection: Z1=%d, Z2=%d, Threshold=%d, Touched=%s", 
             z1, z2, PRESS_THRESHOLD, touched ? "YES" : "NO");
    
    return touched;
}

bool xpt2046_read_touch(uint16_t *x, uint16_t *y)
{
    if (x == NULL || y == NULL) {
        ESP_LOGW(TAG, "Invalid parameters: x=%p, y=%p", x, y);
        return false;
    }
    
    ESP_LOGD(TAG, "Checking for touch...");
    // Check if touch is detected
    bool is_touched = xpt2046_is_touched();
    ESP_LOGD(TAG, "xpt2046_is_touched returned: %s", is_touched ? "true" : "false");
    
    if (!is_touched) {
        ESP_LOGD(TAG, "No touch detected");
        // Reset stuck touch counter when no touch is detected
        stuck_touch_count = 0;
        return false;
    }
    
    ESP_LOGI(TAG, "Valid touch detected, reading coordinates...");
    
    ESP_LOGD(TAG, "Touch detected, reading coordinates...");
    // Read X and Y positions
    uint16_t raw_x = xpt2046_read_register(XPT2046_CMD_XPOS);
    uint16_t raw_y = xpt2046_read_register(XPT2046_CMD_YPOS);
    
    ESP_LOGI(TAG, "Raw touch data: X=%d, Y=%d", raw_x, raw_y);
    
    // Validate raw data to avoid stuck touch
    // Check for maximum values that indicate stuck touch
    if (raw_x >= 4000 || raw_y >= 4000 || raw_x == 0 || raw_y == 0) {
        ESP_LOGW(TAG, "Invalid raw touch data: X=%d, Y=%d (possible stuck touch)", raw_x, raw_y);
        stuck_touch_count++;
        
        // If we've seen too many stuck touches, ignore them
        if (stuck_touch_count >= STUCK_TOUCH_THRESHOLD) {
            ESP_LOGW(TAG, "Too many consecutive stuck touches detected, ignoring");
            return false;
        }
        
        // Return last valid coordinates if available
        if (last_valid_x != 0 || last_valid_y != 0) {
            *x = last_valid_x;
            *y = last_valid_y;
            ESP_LOGI(TAG, "Returning last valid coordinates due to stuck touch: X=%d, Y=%d", *x, *y);
            return true;
        }
        
        return false;
    }
    
    // Apply calibration
    if (raw_x < min_raw_x) raw_x = min_raw_x;
    if (raw_x > max_raw_x) raw_x = max_raw_x;
    if (raw_y < min_raw_y) raw_y = min_raw_y;
    if (raw_y > max_raw_y) raw_y = max_raw_y;
    
    // Convert to screen coordinates (assuming 240x320 display in portrait mode)
    *x = ((raw_x - min_raw_x) * 240) / (max_raw_x - min_raw_x);
    *y = ((raw_y - min_raw_y) * 320) / (max_raw_y - min_raw_y);
    
    // Ensure coordinates are within bounds
    if (*x >= 240) *x = 239;
    if (*y >= 320) *y = 319;
    
    // Additional validation for stuck touch at edge coordinates
    if (*x == 239 && *y == 319) {
        ESP_LOGW(TAG, "Possible stuck touch at edge coordinates: X=%d, Y=%d", *x, *y);
        stuck_touch_count++;
        
        // If we've seen too many stuck touches, ignore them
        if (stuck_touch_count >= STUCK_TOUCH_THRESHOLD) {
            ESP_LOGW(TAG, "Too many consecutive stuck touches at edge, ignoring");
            return false;
        }
        
        // Return last valid coordinates if available
        if (last_valid_x != 0 || last_valid_y != 0) {
            *x = last_valid_x;
            *y = last_valid_y;
            ESP_LOGI(TAG, "Returning last valid coordinates due to stuck edge touch: X=%d, Y=%d", *x, *y);
            return true;
        }
        
        return false;
    }
    
    // Valid touch detected, reset stuck touch counter and save coordinates
    stuck_touch_count = 0;
    last_valid_x = *x;
    last_valid_y = *y;
    
    ESP_LOGI(TAG, "Calibrated touch coordinates: X=%d, Y=%d", *x, *y);
    
    return true;
}

void xpt2046_calibrate(uint16_t min_x, uint16_t max_x, uint16_t min_y, uint16_t max_y)
{
    min_raw_x = min_x;
    max_raw_x = max_x;
    min_raw_y = min_y;
    max_raw_y = max_y;
    
    ESP_LOGI(TAG, "Touch calibration updated: X(%d-%d), Y(%d-%d)", min_x, max_x, min_y, max_y);
    
    // Log the expected coordinate ranges
    ESP_LOGI(TAG, "Expected screen coordinates: X(0-239), Y(0-319)");
}
