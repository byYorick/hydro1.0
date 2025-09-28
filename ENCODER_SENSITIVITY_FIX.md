# Fix for Encoder Sensitivity Issue in ESP32-S3 Hydroponics Project

## Problem Description

The rotary encoder was triggering 4 events per single physical rotation, making it overly sensitive and difficult to use for UI navigation.

## Root Cause

Rotary encoders naturally produce multiple electrical transitions per mechanical detent due to their quadrature encoding mechanism. Without proper filtering, each of these transitions is interpreted as a separate event.

## Solution Implemented

### 1. Added Count Filtering in Encoder Task

Modified [components/encoder/encoder.c](file:///c%3A/esp/hydro/hydro1.0/components/encoder/encoder.c) to implement count-based filtering:

```c
// For filtering - only trigger events every N counts
static int32_t last_filtered_count = 0;
const int32_t COUNT_FILTER = 4; // Adjust this value to change sensitivity (higher = less sensitive)

// Apply filtering to reduce sensitivity
int32_t current_count = encoder_get_count();
int32_t count_diff = current_count - last_filtered_count;

// Only trigger events when we have enough count difference
if (abs(count_diff) >= COUNT_FILTER) {
    // Process encoder event
    last_filtered_count = current_count;
}
```

### 2. Adjustable Sensitivity

The sensitivity can be easily adjusted by changing the `COUNT_FILTER` value:
- Higher values = Less sensitive (more physical rotation needed for event)
- Lower values = More sensitive (less physical rotation needed for event)
- Default value of 4 reduces 4:1 ratio to 1:1

### 3. Removed Debug Logging from ISR Context

Commented out debug logging in the LVGL encoder callback to prevent ISR-related issues:

```c
//ESP_LOGD(TAG, "Encoder diff: %ld, Button: %d", (long)enc_data.enc_diff, enc_data.state);
```

## How It Works

1. The PCNT peripheral still counts all encoder transitions for accuracy
2. The encoder task now filters events based on accumulated count differences
3. Only when the count difference exceeds the threshold (COUNT_FILTER) is an event generated
4. This effectively groups multiple transitions into a single logical event

## Testing the Fix

To test the fix:

1. Rotate the encoder one full detent/click
2. Verify that only one event is generated (instead of 4)
3. Adjust the COUNT_FILTER value if needed for your specific encoder

## Fine-tuning Sensitivity

If the encoder still feels too sensitive or not sensitive enough:

1. Increase `COUNT_FILTER` value to make it less sensitive
2. Decrease `COUNT_FILTER` value to make it more sensitive
3. Valid range: 1-10 (1 = most sensitive, 10 = least sensitive)

## Alternative Solutions

For encoders with different characteristics, consider:

1. **Hardware solutions**: Use encoders with built-in debouncing
2. **Software solutions**: Implement time-based debouncing in addition to count filtering
3. **PCNT configuration**: Adjust PCNT edge detection settings for different encoder types

## Conclusion

This fix provides a simple yet effective solution to the encoder sensitivity issue by implementing count-based filtering in the encoder task. The solution is adjustable and maintains compatibility with the existing LVGL integration.