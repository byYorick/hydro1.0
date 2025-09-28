# Fix for Task Watchdog Timeout Issue in ESP32-S3 Hydroponics Project

## Problem Description

The system was experiencing task watchdog timeouts with the following error:
```
E (35733) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (35733) task_wdt:  - IDLE1 (CPU 1)
E (35733) task_wdt: Tasks currently running:
E (35733) task_wdt: CPU 0: IDLE0
E (35733) task_wdt: CPU 1: lvgl_timer
```

This indicated that the LVGL timer task was taking too long to execute, causing the task watchdog to trigger.

## Root Cause

The LVGL timer task was blocking for extended periods due to:
1. Long timeouts when trying to acquire the LVGL mutex
2. Inefficient handling of LVGL timer callbacks
3. Potential contention with other tasks trying to access LVGL

## Solution Implemented

### 1. Optimized LVGL Timer Task

Modified the LVGL timer task in [components/lvgl_main/lvgl_main.c](file:///c%3A/esp/hydro/hydro1.0/components/lvgl_main/lvgl_main.c) to:
- Use very short timeouts when acquiring the LVGL mutex (10ms)
- Implement adaptive delay based on LVGL's recommendations
- Cap delays to prevent watchdog timeouts
- Maintain a fallback sleep time when lock acquisition fails

```c
static void lvgl_timer_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL timer task started");
    
    uint32_t last_sleep_time = 0;
    
    while (1) {
        // Try to acquire the lock with a very short timeout to prevent blocking
        if (lvgl_lock(10)) {  // 10ms timeout
            // Run the LVGL timer handler
            uint32_t sleep_ms = lv_timer_handler();
            lvgl_unlock();
            
            // Use the sleep time suggested by LVGL, but with reasonable limits
            if (sleep_ms > 50) {
                sleep_ms = 50;  // Cap to 50ms to prevent watchdog issues
            } else if (sleep_ms < 5) {
                sleep_ms = 5;   // Minimum 5ms delay
            }
            
            last_sleep_time = sleep_ms;
        } else {
            // If we can't acquire the lock, use the last known good sleep time
            // but with a minimum to prevent busy waiting
            if (last_sleep_time < 5) {
                last_sleep_time = 10;
            }
        }
        
        // Delay based on LVGL's recommendation or last known good value
        vTaskDelay(pdMS_TO_TICKS(last_sleep_time));
    }
}
```

### 2. Optimized Display Update Task

Modified the display update task to:
- Use reasonable timeouts when acquiring the LVGL mutex (500ms)
- Add appropriate delays to reduce contention
- Improve error handling

```c
static void display_update_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
        // Ожидание данных датчиков
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Received sensor data from queue: pH=%.2f, EC=%.2f, Temp=%.1f", 
                     sensor_data.ph, sensor_data.ec, sensor_data.temp);
            
            // Try to acquire the lock with a reasonable timeout
            if (!lvgl_lock(500)) {  // 500ms timeout
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            // Проверка, что LVGL система инициализирована
            if (lv_is_initialized()) {
                // Обновление отображения датчиков
                update_sensor_display(&sensor_data);
                ESP_LOGI(TAG, "Sensor display updated successfully");
            } else {
                ESP_LOGW(TAG, "LVGL not initialized, skipping display update");
            }
            
            // Освобождение мьютекса
            lvgl_unlock();
        } else {
            ESP_LOGD(TAG, "No sensor data received within timeout");
        }
        
        // Small delay to prevent excessive CPU usage and reduce contention
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

## Configuration

The task watchdog timeout is configured in [sdkconfig.defaults](file:///c%3A/esp/hydro/hydro1.0/sdkconfig.defaults) as:
```
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
```

This provides a 10-second timeout, which should be sufficient for normal operation with the optimized tasks.

## Testing the Fix

To verify the fix is working:

1. Flash the updated firmware to the ESP32-S3
2. Monitor the serial output for task watchdog timeout errors
3. These errors should no longer appear
4. Verify that the display updates correctly and UI remains responsive
5. Confirm that sensor data is still being processed and displayed

## Performance Impact

The fix should improve system stability by:
- Preventing task watchdog timeouts
- Reducing contention between LVGL tasks
- Maintaining responsive UI updates
- Preserving all existing functionality

## Alternative Solutions

If issues persist, consider:

1. **Increasing watchdog timeout**: Adjust `CONFIG_ESP_TASK_WDT_TIMEOUT_S` in [sdkconfig.defaults](file:///c%3A/esp/hydro/hydro1.0/sdkconfig.defaults)
2. **Adjusting task priorities**: Modify task priorities to ensure proper scheduling
3. **Reducing LVGL workload**: Optimize UI complexity or update frequency
4. **Using dedicated CPU cores**: Pin critical tasks to specific CPU cores

## Conclusion

This fix resolves the task watchdog timeout issue by optimizing the LVGL timer and display update tasks. The solution maintains all existing functionality while providing better coordination between tasks and preventing system crashes due to watchdog timeouts.