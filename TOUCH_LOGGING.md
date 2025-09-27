# Touch Coordinate Logging

## Overview

The XPT2046 touch controller is configured to output touch coordinates through the ESP-IDF logging system. When you touch the screen, the coordinates will be displayed in the serial monitor/logs.

## Log Output Format

When a touch is detected, you will see log messages like:

```
I (timestamp) app_main: Touch #1 at coordinates: X=150, Y=100
I (timestamp) app_main: Touch #2 at coordinates: X=200, Y=150
```

## Viewing Logs

### Using ESP-IDF Monitor

If you're using the ESP-IDF development environment, you can view the logs using the monitor:

```bash
idf.py monitor
```

### Using Serial Terminal

You can also use any serial terminal program (like PuTTY, Tera Term, or screen) to view the logs:

- **Baud Rate**: 115200
- **Port**: Depends on your system (e.g., COM3 on Windows, /dev/ttyUSB0 on Linux)

### Using VS Code with ESP-IDF Extension

If you're using VS Code with the ESP-IDF extension:
1. Open the ESP-IDF terminal
2. Run `idf.py monitor`

## Log Levels

The touch logging uses two log levels:

- **INFO (I)**: For touch detection and coordinates
- **DEBUG (D)**: For detailed touch processing information

To see DEBUG level logs, you may need to adjust the log level in menuconfig:

```
idf.py menuconfig
```

Navigate to:
Component config → Log output → Default log verbosity

Set to "Debug" to see all touch-related debug information.

## Example Log Output

```
I (12345) app_main: Touch task started
I (15678) app_main: Touch #1 at coordinates: X=160, Y=120
D (15680) app_main: Time since last touch: 3433 ms
D (15685) xpt2046: Touch detection: Z1=2500, Threshold=500, Touched=YES
D (15690) xpt2046: Raw touch data: X=2048, Y=2000
D (15695) xpt2046: Calibrated touch coordinates: X=160, Y=120
```

## Troubleshooting

If you're not seeing touch logs:

1. **Check wiring**: Ensure the XPT2046 is properly connected to the ESP32
2. **Verify power**: Make sure the touch controller is receiving power
3. **Check CS pin**: Verify the chip select pin is correctly configured
4. **Adjust threshold**: If touches aren't being detected, you may need to adjust the PRESS_THRESHOLD value
5. **Enable debug logs**: Set log verbosity to DEBUG to see more detailed information

## Testing Touch Logging

### Using the Touch Log Test Application

A dedicated test application [touch_log_test.c](file:///c%3A/esp/hydro/hydro1.0/main/touch_log_test.c) is provided to verify touch coordinate logging:

1. Build and flash the application:
   ```bash
   idf.py build flash
   ```

2. Monitor the output:
   ```bash
   idf.py monitor
   ```

3. Touch the screen and observe the coordinate logs

### Expected Behavior

When the test application is running:
- You should see "Touch Log Test Started" message
- After initialization, you'll see "Touch the screen to see coordinates in the logs"
- Each touch should produce a log like:
  ```
  I (timestamp) touch_log_test: Touch #1 at coordinates: X=160, Y=120
  ```

## Customization

You can modify the touch logging behavior by adjusting the following in [app_main.c](file:///c%3A/esp/hydro/hydro1.0/main/app_main.c):

1. **Touch detection frequency**: Change the delay in `vTaskDelay(pdMS_TO_TICKS(50))`
2. **Debounce delay**: Adjust the delay after detecting a touch to prevent multiple detections
3. **Log format**: Modify the ESP_LOGI format string to change what information is displayed

## Calibration

If touch coordinates are not accurate, you may need to calibrate the touch controller by adjusting the calibration values in [xpt2046.h](file:///c%3A/esp/hydro/hydro1.0/components/xpt2046/xpt2046.h):

```c
#define XPT2046_MIN_RAW_X        300
#define XPT2046_MAX_RAW_X        3800
#define XPT2046_MIN_RAW_Y        200
#define XPT2046_MAX_RAW_Y        3900
```