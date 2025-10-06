#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYDRO_SETTINGS_VERSION 1

typedef struct {
    uint8_t version;
    float target_ph;
    float ph_tolerance;
    float target_ec;
    float ec_tolerance;
    uint32_t dosing_duration_ms;
    uint32_t dosing_cooldown_s;
    bool auto_dosing_enabled;
    bool lighting_auto_mode;
    bool lighting_manual_state;
    uint8_t lighting_on_hour;
    uint8_t lighting_off_hour;
} hydro_settings_t;

typedef void (*hydro_settings_listener_t)(const hydro_settings_t *settings);

esp_err_t hydro_settings_init(void);
const hydro_settings_t *hydro_settings_get(void);

esp_err_t hydro_settings_set_target_ph(float value);
esp_err_t hydro_settings_set_ph_tolerance(float value);
esp_err_t hydro_settings_set_target_ec(float value);
esp_err_t hydro_settings_set_ec_tolerance(float value);
esp_err_t hydro_settings_set_dosing_duration(uint32_t duration_ms);
esp_err_t hydro_settings_set_dosing_cooldown(uint32_t cooldown_seconds);
esp_err_t hydro_settings_set_auto_dosing_enabled(bool enabled);
esp_err_t hydro_settings_set_lighting_auto_mode(bool enabled);
esp_err_t hydro_settings_set_lighting_manual_state(bool on);
esp_err_t hydro_settings_set_lighting_schedule(uint8_t on_hour, uint8_t off_hour);

esp_err_t hydro_settings_register_listener(hydro_settings_listener_t listener);

#ifdef __cplusplus
}
#endif
