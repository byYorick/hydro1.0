#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*sensor_temp_hum_reader_t)(float *temperature, float *humidity);
typedef esp_err_t (*sensor_value_reader_t)(float *value);
typedef bool (*sensor_lux_reader_t)(float *lux_value);
typedef bool (*sensor_co2_reader_t)(float *co2_value, float *tvoc_value);

typedef struct {
    sensor_temp_hum_reader_t read_temperature_humidity;
    sensor_value_reader_t read_ph;
    sensor_value_reader_t read_ec;
    sensor_lux_reader_t read_lux;
    sensor_co2_reader_t read_co2;
} sensor_interface_t;

typedef esp_err_t (*pump_run_ms_fn_t)(pump_index_t pump, uint32_t duration_ms);

typedef struct {
    pump_run_ms_fn_t run_pump_ms;
} actuator_interface_t;

esp_err_t system_interfaces_init(void);

const sensor_interface_t *system_interfaces_get_sensor_interface(void);
esp_err_t system_interfaces_set_sensor_interface(const sensor_interface_t *iface);

const actuator_interface_t *system_interfaces_get_actuator_interface(void);
esp_err_t system_interfaces_set_actuator_interface(const actuator_interface_t *iface);

#ifdef __cplusplus
}
#endif

