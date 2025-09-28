# Fix for ISR Logging Issue in ESP32-S3 Hydroponics Project

## Problem Description

The ESP32-S3 device was experiencing crashes during the initial LVGL display flush operation. The crash occurred due to an abort in `lock_acquire_generic` function when debug logging was attempted from an interrupt service routine (ISR) context.

### Root Cause

1. LVGL initiates a display flush operation
2. This triggers an SPI transaction for the LCD panel
3. The SPI transaction completes and calls a callback from ISR context
4. Debug logging from the ISR tries to acquire a mutex, which is not allowed
5. This causes an abort and system panic

## Solutions Implemented

### 1. Reduced Log Verbosity

In [main/app_main.c](file:///c%3A/esp/hydro/hydro1.0/main/app_main.c), we've added code to reduce the log level for problematic components:

```c
// Reduce log levels for components that may log from ISR context
// This prevents crashes due to mutex acquisition in interrupt context
esp_log_level_set("spi_master", ESP_LOG_INFO);  // Reduce from DEBUG to INFO
esp_log_level_set("LCD", ESP_LOG_INFO);         // Reduce LCD logging level
```

This prevents debug logs from being generated in ISR contexts, eliminating the mutex acquisition issue.

### 2. Changed SPI to Polling Mode

In [components/lcd_ili9341/lcd_ili9341.c](file:///c%3A/esp/hydro/hydro1.0/components/lcd_ili9341/lcd_ili9341.c), we've modified the SPI configuration to use polling mode instead of interrupt mode:

```c
esp_lcd_panel_io_spi_config_t io_config = {
    .dc_gpio_num = PIN_NUM_LCD_DC,
    .cs_gpio_num = PIN_NUM_LCD_CS,
    .pclk_hz = LCD_PIXEL_CLOCK_HZ,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .spi_mode = 0,
    .trans_queue_depth = 0,  // Set to 0 for polling mode to avoid ISR issues
    .on_color_trans_done = notify_lvgl_flush_ready,
    .user_ctx = &disp_drv,
};
```

Setting `trans_queue_depth` to 0 enables polling mode, which eliminates ISR callbacks and the associated logging issues.

### 3. Disabled Debug Logging in Critical Sections

In [components/lcd_ili9341/lcd_ili9341.c](file:///c%3A/esp/hydro/hydro1.0/components/lcd_ili9341/lcd_ili9341.c), we've commented out debug logging in critical sections that might be called from ISR:

```c
//ESP_LOGD("LCD", "notify_lvgl_flush_ready called");
```

This prevents any debug logs from being generated in ISR contexts.

## Testing the Fix

To verify the fix is working:

1. Flash the updated firmware to the ESP32-S3
2. Monitor the serial output for any panic messages
3. Verify that the display initializes correctly and shows the UI
4. Confirm that sensor data is displayed properly

## Alternative Solutions

If these changes don't resolve the issue, consider these alternatives:

### Menuconfig Approach

Use `idf.py menuconfig` to change the default log verbosity:
- Path: Component config → Log output → Default log verbosity → Info

### ISR-Safe Logging

For cases where debug logging is needed in ISR:
- Replace `ESP_LOGD` with `ESP_DRAM_LOGD` (ISR-safe but limited)
- Enable `CONFIG_LOG_MASTER_LEVEL=y` in menuconfig for buffered ISR-safe logs
- Use `ESP_EARLY_LOGD` for critical ISR logs

## Performance Impact

The polling mode approach may have a slight performance impact compared to interrupt mode, but it provides better stability. The impact should be minimal for most UI applications.

## Conclusion

These changes should resolve the abort issue caused by mutex acquisition in ISR context during LVGL display operations. The solution prioritizes system stability over maximum performance, which is appropriate for this application.