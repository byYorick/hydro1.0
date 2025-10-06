#pragma once

#include "hydro_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int ph_acid_ia;
    int ph_acid_ib;
    int ph_base_ia;
    int ph_base_ib;
    int ec_a_ia;
    int ec_a_ib;
    int ec_b_ia;
    int ec_b_ib;
    int ec_c_ia;
    int ec_c_ib;
} automation_pump_pins_t;

typedef struct {
    float ph;
    float ec;
} automation_sensor_data_t;

void automation_controller_init(const automation_pump_pins_t *pins, const hydro_settings_t *initial_settings);
void automation_controller_apply_settings(const hydro_settings_t *settings);
void automation_controller_update(const automation_sensor_data_t *data);

#ifdef __cplusplus
}
#endif
