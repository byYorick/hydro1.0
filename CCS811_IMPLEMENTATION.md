# CCS811 Air Quality Sensor Implementation

## Overview

This document describes the implementation of the CCS811 air quality sensor library for the ESP-IDF framework. The implementation is based on the Adafruit CCS811 library and provides a complete interface for interacting with the sensor.

## Features

1. **Complete Register Definitions**: All sensor registers are properly defined according to the datasheet
2. **Proper Initialization Sequence**: 
   - Hardware ID verification
   - Software reset
   - Application mode startup
   - Error checking
3. **Measurement Functions**:
   - Read eCO2 (equivalent CO2) values
   - Read TVOC (Total Volatile Organic Compounds) values
   - Check data readiness
4. **Configuration Options**:
   - Drive mode selection (Idle, 1sec, 10sec, 60sec, 250ms)
   - Interrupt enable/disable
   - Environmental data compensation (humidity and temperature)
5. **Error Handling**:
   - Hardware error detection
   - Communication error handling
   - Stub value support for disconnected sensors

## API Functions

### Initialization
```c
bool ccs811_init(void);
```
Initializes the CCS811 sensor with proper hardware verification and application startup.

### Data Reading
```c
bool ccs811_data_ready(void);
bool ccs811_read_data(float *eco2, float *tvoc);
bool ccs811_read_eco2(float *eco2);
bool ccs811_read_tvoc(float *tvoc);
```

### Configuration
```c
void ccs811_set_drive_mode(uint8_t mode);
void ccs811_enable_interrupt(void);
void ccs811_disable_interrupt(void);
void ccs811_set_environmental_data(uint8_t humidity, float temperature);
```

### Error Handling
```c
bool ccs811_check_error(void);
void ccs811_software_reset(void);
```

## Implementation Details

### Stub Values Support
The implementation includes support for stub values when the sensor is not connected. This allows the system to continue operating with default values rather than failing completely.

### Thread Safety
The implementation uses the existing I2C bus mutex from the i2c_bus component to ensure thread-safe access to the sensor.

### Error Logging
The implementation uses debug-level logging for communication issues, which reduces log noise while still providing diagnostic information when needed.

## Usage in Main Application

The sensor is initialized in the sensor_task function and readings are taken periodically. The values are then passed to the LVGL UI for display.

## Register Map

| Register | Address | Description |
|----------|---------|-------------|
| STATUS | 0x00 | Status register |
| MEAS_MODE | 0x01 | Measurement mode |
| ALG_RESULT_DATA | 0x02 | Algorithm result data |
| RAW_DATA | 0x03 | Raw data |
| ENV_DATA | 0x05 | Environmental data |
| NTC | 0x06 | NTC register |
| THRESHOLDS | 0x10 | Thresholds |
| BASELINE | 0x11 | Baseline |
| HW_ID | 0x20 | Hardware ID |
| HW_VERSION | 0x21 | Hardware version |
| FW_BOOT_VERSION | 0x23 | Firmware bootloader version |
| FW_APP_VERSION | 0x24 | Firmware application version |
| ERROR_ID | 0xE0 | Error ID |
| SW_RESET | 0xFF | Software reset |

## Drive Modes

| Mode | Value | Description |
|------|-------|-------------|
| CCS811_DRIVE_MODE_IDLE | 0x00 | Idle mode |
| CCS811_DRIVE_MODE_1SEC | 0x01 | 1 second measurement interval |
| CCS811_DRIVE_MODE_10SEC | 0x02 | 10 second measurement interval |
| CCS811_DRIVE_MODE_60SEC | 0x03 | 60 second measurement interval |
| CCS811_DRIVE_MODE_250MS | 0x04 | 250 millisecond measurement interval |