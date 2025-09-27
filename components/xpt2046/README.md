# XPT2046 Touch Controller Component

## Overview

This component provides support for the XPT2046 resistive touch controller commonly used with ILI9341 TFT displays. The XPT2046 communicates via SPI and provides touch position detection capabilities.

## Features

- SPI-based communication with the touch controller
- Touch position reading (X/Y coordinates)
- Touch detection status checking
- Calibration support
- Integration with LVGL (through separate input device driver)

## Hardware Connections

The XPT2046 typically connects to the ESP32 using the following pins:

| XPT2046 Pin | ESP32 Pin | Function      |
|-------------|-----------|---------------|
| VCC         | 3.3V      | Power         |
| GND         | GND       | Ground        |
| CLK         | GPIO12    | SPI Clock     |
| MISO        | GPIO13    | SPI MISO      |
| MOSI        | GPIO11    | SPI MOSI      |
| CS          | GPIO5     | Chip Select   |
| IRQ         | GPIO4     | Interrupt     |
| DOUT        | -         | Not connected |

Note: The CLK, MISO, and MOSI pins are typically shared with the display controller.

## Usage

### Initialization

```c
#include "xpt2046.h"

// Initialize the touch controller
if (xpt2046_init()) {
    printf("XPT2046 initialized successfully\n");
} else {
    printf("Failed to initialize XPT2046\n");
}
```

### Reading Touch Input

```c
uint16_t x, y;
if (xpt2046_read_touch(&x, &y)) {
    printf("Touch detected at (%d, %d)\n", x, y);
    // Process touch event
}
```

### Checking Touch Status

```c
if (xpt2046_is_touched()) {
    printf("Screen is being touched\n");
}
```

### Calibration

```c
// Calibrate with known values
xpt2046_calibrate(300, 3800, 200, 3900);
```

## Integration with LVGL

To integrate with LVGL, you need to create an input device driver:

```c
// Example LVGL input device driver
void touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    
    if (xpt2046_read_touch(&last_x, &last_y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = last_x;
        data->point.y = last_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Register the input device
lv_indev_drv_t indev_drv;
lv_indev_drv_init(&indev_drv);
indev_drv.type = LV_INDEV_TYPE_POINTER;
indev_drv.read_cb = touchpad_read;
lv_indev_drv_register(&indev_drv);
```

## Configuration

The component can be configured by modifying the following values in `xpt2046.h`:

- `XPT2046_CLOCK_SPEED_HZ`: SPI clock speed (default: 1MHz)
- `XPT2046_MIN_RAW_X/Y` and `XPT2046_MAX_RAW_X/Y`: Calibration values for raw touch coordinates

## Troubleshooting

1. **Touch not detected**: Check wiring connections, especially CS and IRQ pins
2. **Inaccurate touch positions**: Run calibration procedure
3. **SPI conflicts**: Ensure SPI bus configuration matches your hardware setup