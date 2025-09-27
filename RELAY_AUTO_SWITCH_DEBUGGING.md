# Relay Auto-Switch Debugging Guide

## Overview

This document provides guidance for debugging issues with the relay auto-switch functionality in the hydroponics project. The most critical issue is the stack overflow that was causing system crashes.

## Common Issues and Solutions

### 1. Stack Overflow Error

#### Symptoms
```
***ERROR*** A stack overflow in task relay_auto_swit has been detected.
Backtrace: 0x40375b7e:0x3fca6ca0 0x4037be4d:0x3fca6cc0 ...
```

#### Root Causes
1. Insufficient stack size for the auto-switch task
2. Task priority conflicts with other system tasks
3. Recursive function calls or deep function nesting

#### Solutions

##### Increase Stack Size
The primary fix is to increase the stack size for the auto-switch task:
```c
// Before (insufficient):
xTaskCreate(auto_switch_task, "relay_auto_switch", 2048, NULL, 5, &auto_switch_task_handle);

// After (sufficient):
xTaskCreate(auto_switch_task, "relay_auto_switch", 4096, NULL, 3, &auto_switch_task_handle);
```

##### Adjust Task Priority
Use consistent task priorities to avoid conflicts:
- Main tasks: Priority 3
- Auto-switch task: Priority 3 (same as other tasks)

##### Monitor Stack Usage
Enable stack monitoring in menuconfig to detect potential issues early:
```
idf.py menuconfig
# Component config → FreeRTOS → Check for stack overflow
```

### 2. Auto-Switch Not Working

#### Symptoms
- Relay channels not switching automatically
- No log messages from auto-switch task
- Task not starting or stopping unexpectedly

#### Solutions

##### Check Initialization Order
Ensure the relay is properly initialized before starting auto-switch:
```c
if (trema_relay_init()) {
    trema_relay_auto_switch(true);  // Start auto-switch after successful init
}
```

##### Verify Task Creation
Check that the task is created successfully:
```c
BaseType_t result = xTaskCreate(auto_switch_task, "relay_auto_switch", 4096, NULL, 3, &auto_switch_task_handle);
if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create auto-switch task");
}
```

## Debugging Tools

### Relay Auto-Switch Test Application
The [relay_auto_switch_test.c](file:///c%3A/esp/hydro/hydro1.0/main/relay_auto_switch_test.c) application provides a focused test environment:
- Isolated relay auto-switch testing
- Manual control verification
- Auto-switch mode testing with proper logging

### Log Analysis
Enable verbose logging to see detailed auto-switch operations:
```bash
# Set log level to INFO or DEBUG
idf.py menuconfig
# Component config → Log output → Default log verbosity
```

Expected debug output:
```
I (1234) trema_relay: Auto-switch function called with enable=1
I (1235) trema_relay: Creating auto-switch task
I (1240) trema_relay: Auto-switch task started
I (1245) trema_relay: Max channel: 1, Relay model: 0x0E
I (1250) trema_relay: Turning on channel 0
I (3250) trema_relay: Turning on channel 1
I (5250) trema_relay: Turning on channel 0
```

## Testing Steps

### Step 1: Basic Functionality Test
1. Flash the relay auto-switch test application
2. Monitor serial output
3. Verify "Auto-switch task started" message appears
4. Check that channels are switching every 2 seconds

### Step 2: Stack Monitoring
1. Enable stack overflow checking in menuconfig
2. Run the application for extended periods
3. Monitor for any stack overflow warnings

### Step 3: Integration Test
1. Flash the main application
2. Verify that auto-switch works alongside other components
3. Check that no stack overflow errors occur

## Best Practices

### Task Management
1. Always use adequate stack sizes (minimum 4096 bytes for complex tasks)
2. Use consistent task priorities
3. Properly clean up tasks when stopping
4. Check task creation return values

### Memory Management
1. Monitor heap usage during development
2. Avoid large local variables in tasks
3. Use static variables when possible
4. Free allocated memory properly

### Error Handling
1. Always check return values from FreeRTOS functions
2. Implement proper error logging
3. Provide graceful fallback mechanisms
4. Clean up resources on failure

## Troubleshooting Steps

### Step 1: Verify Stack Size
1. Check that auto-switch task uses at least 4096 bytes stack
2. Monitor stack usage with FreeRTOS tools

### Step 2: Check Task Priority
1. Ensure consistent priority with other system tasks
2. Avoid priority inversion issues

### Step 3: Validate Task Creation
1. Confirm task creation returns pdPASS
2. Verify task handle is properly set

### Step 4: Monitor Runtime Behavior
1. Check for proper channel switching
2. Verify task cleanup on stop
3. Monitor for any error conditions

## Known Issues

### Task Resource Conflicts
When multiple tasks run simultaneously:
- Ensure adequate CPU time for all tasks
- Check for resource contention (I2C bus, etc.)
- Monitor task scheduling delays

### Memory Constraints
In memory-constrained environments:
- Optimize stack usage per task
- Avoid unnecessary local variables
- Consider using smaller stack sizes for simple tasks

## Contact Support

If issues persist after following this guide:
1. Document all error messages and backtraces
2. Include hardware setup details
3. Provide debug log output
4. Describe steps already taken
5. Include information about task priorities and stack sizes used