# Encoder Integration with LVGL 8.3 for ESP32-S3 Hydroponics Project

This document explains how the rotary encoder is integrated with LVGL 8.3 in the hydroponics project, ensuring proper functionality with FreeRTOS.

## Overview

The integration implements a complete solution for using a rotary encoder with LVGL's input device system, providing:
- Smooth navigation between UI elements
- Proper button handling for interactions
- Thread-safe operation with FreeRTOS
- Low-latency response to encoder events

## Implementation Details

### 1. Hardware Configuration

The encoder is connected to the following GPIO pins:
- CLK (A): GPIO1
- DT (B): GPIO2
- SW (Button): GPIO3

These pins are configured in `main/app_main.c`:
```c
#define ENC_A_PIN           1   // CLK - Encoder pin (clock signal)
#define ENC_B_PIN           2   // DT - Encoder pin (data)
#define ENC_SW_PIN          3   // Encoder button
```

### 2. Encoder Driver (`components/encoder/`)

The encoder driver uses ESP-IDF's PCNT (Pulse Counter) peripheral for precise quadrature decoding:
- PCNT tracks encoder pulses for rotation detection
- Watch points trigger events at specific count thresholds
- Hardware debouncing filter (2000ns) for stable readings
- Queue-based event handling for efficient processing

Key improvements made:
- Increased queue size from 10 to 20 to prevent overflow
- Removed periodic polling delay for lower latency
- Task now waits indefinitely on the queue for events

### 3. LVGL Integration (`components/lvgl_main/lvgl_encoder.c`)

A dedicated module handles the LVGL integration:
- Implements `lv_indev_drv_t` for encoder input
- Creates an LVGL group for object navigation
- Provides read callback for LVGL's input system
- Automatically adds interactive objects to the group

The read callback function:
```c
static void encoder_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_count = 0;
    
    // Get current encoder count
    int32_t current_count = encoder_get_count();
    
    // Calculate difference since last read
    enc_data.enc_diff = current_count - last_count;
    last_count = current_count;
    
    // Get button state
    enc_data.state = encoder_button_pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    
    // Pass data to LVGL
    data->enc_diff = enc_data.enc_diff;
    data->state = enc_data.state;
    
    // Clear count if there was movement to prevent accumulation
    if (enc_data.enc_diff != 0) {
        encoder_clear_count();
    }
}
```

### 4. UI Navigation

Interactive elements are automatically added to the encoder group:
- Settings button on main screen
- Back button on settings screen

In `lvgl_main.c`:
```c
// Add to encoder group for navigation
lvgl_encoder_add_obj(btn_settings);
```

LVGL handles navigation automatically:
- Rotation moves focus between objects
- Button press triggers `LV_EVENT_CLICKED`
- Group wrapping allows cycling through objects

### 5. FreeRTOS Integration

Task priorities are configured for optimal performance:
- Encoder task: Priority 10 (highest)
- Display update task: Priority 5
- Sensor task: Priority 3
- Main LVGL timer task: Priority 7

The encoder task efficiently waits on the event queue without consuming CPU when idle.

## Usage

### Adding New Interactive Elements

To make new UI elements navigable with the encoder:

1. Create the object:
```c
lv_obj_t *my_button = lv_btn_create(screen);
```

2. Add it to the encoder group:
```c
lvgl_encoder_add_obj(my_button);
```

### Screen Navigation

The encoder provides two types of input:
- Rotation: Moves focus between UI elements
- Button press: Activates the focused element

## Testing

To test the encoder functionality:
1. Rotate the encoder to move focus between the Settings button and Back button
2. Press the encoder button to activate the focused element
3. Verify that screen transitions work correctly

## Troubleshooting

Common issues and solutions:

1. **Encoder not responding**: Check GPIO pin assignments and wiring
2. **Erratic behavior**: Increase glitch filter value in encoder driver
3. **Missed events**: Increase queue size in encoder driver
4. **UI not updating**: Ensure objects are added to the encoder group

## Performance Considerations

- The encoder task uses minimal CPU when idle due to queue-based event handling
- LVGL's input system efficiently processes encoder events
- Proper task prioritization ensures responsive UI