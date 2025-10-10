#include "system_interfaces.h"

#include "ccs811.h"
#include "peristaltic_pump.h"
#include "sht3x.h"
#include "trema_ec.h"
#include "trema_lux.h"
#include "trema_ph.h"
#include <math.h>

static sensor_interface_t g_sensor_interface = {0};
static actuator_interface_t g_actuator_interface = {0};
static bool g_initialized = false;

// GPIO пины для насосов (один пин на насос - через оптопару)
static const int s_pump_pins[PUMP_INDEX_COUNT] = {
    PUMP_PH_UP_PIN,
    PUMP_PH_DOWN_PIN,
    PUMP_EC_A_PIN,
    PUMP_EC_B_PIN,
    PUMP_EC_C_PIN,
    PUMP_WATER_PIN,
};

static bool default_read_temperature_humidity(float *temperature, float *humidity)
{
    return sht3x_read(temperature, humidity);
}

static esp_err_t default_read_ph(float *value)
{
    return trema_ph_read(value) ? ESP_OK : ESP_FAIL;
}

static esp_err_t default_read_ec(float *value)
{
    return trema_ec_read(value) ? ESP_OK : ESP_FAIL;
}

static bool default_read_lux(float *lux_value)
{
    uint16_t raw = 0;
    bool ok = trema_lux_read(&raw);
    if (ok && lux_value != NULL) {
        *lux_value = (float)raw;
    } else if (lux_value != NULL) {
        *lux_value = NAN;
    }
    return ok;
}

static bool default_read_co2(float *co2_value, float *tvoc_value)
{
    float local_co2 = 0.0f;
    float local_tvoc = 0.0f;
    bool ok = ccs811_read_data(&local_co2, &local_tvoc);
    if (ok) {
        if (co2_value != NULL) {
            *co2_value = local_co2;
        }
        if (tvoc_value != NULL) {
            *tvoc_value = local_tvoc;
        }
    }
    return ok;
}

static esp_err_t default_run_pump_ms(pump_index_t pump, uint32_t duration_ms)
{
    if (pump >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    pump_run_ms(s_pump_pins[pump], duration_ms);
    return ESP_OK;
}

static void apply_sensor_interface_defaults(sensor_interface_t *dst, const sensor_interface_t *src)
{
    sensor_interface_t defaults = {
        .read_temperature_humidity = default_read_temperature_humidity,
        .read_ph = default_read_ph,
        .read_ec = default_read_ec,
        .read_lux = default_read_lux,
        .read_co2 = default_read_co2,
    };

    *dst = defaults;
    if (src == NULL) {
        return;
    }

    if (src->read_temperature_humidity != NULL) {
        dst->read_temperature_humidity = src->read_temperature_humidity;
    }
    if (src->read_ph != NULL) {
        dst->read_ph = src->read_ph;
    }
    if (src->read_ec != NULL) {
        dst->read_ec = src->read_ec;
    }
    if (src->read_lux != NULL) {
        dst->read_lux = src->read_lux;
    }
    if (src->read_co2 != NULL) {
        dst->read_co2 = src->read_co2;
    }
}

static void apply_actuator_interface_defaults(actuator_interface_t *dst, const actuator_interface_t *src)
{
    actuator_interface_t defaults = {
        .run_pump_ms = default_run_pump_ms,
    };

    *dst = defaults;
    if (src == NULL) {
        return;
    }

    if (src->run_pump_ms != NULL) {
        dst->run_pump_ms = src->run_pump_ms;
    }
}

esp_err_t system_interfaces_init(void)
{
    apply_sensor_interface_defaults(&g_sensor_interface, NULL);
    apply_actuator_interface_defaults(&g_actuator_interface, NULL);
    g_initialized = true;
    return ESP_OK;
}

const sensor_interface_t *system_interfaces_get_sensor_interface(void)
{
    if (!g_initialized) {
        system_interfaces_init();
    }
    return &g_sensor_interface;
}

esp_err_t system_interfaces_set_sensor_interface(const sensor_interface_t *iface)
{
    if (!g_initialized) {
        system_interfaces_init();
    }
    apply_sensor_interface_defaults(&g_sensor_interface, iface);
    return ESP_OK;
}

const actuator_interface_t *system_interfaces_get_actuator_interface(void)
{
    if (!g_initialized) {
        system_interfaces_init();
    }
    return &g_actuator_interface;
}

esp_err_t system_interfaces_set_actuator_interface(const actuator_interface_t *iface)
{
    if (!g_initialized) {
        system_interfaces_init();
    }
    apply_actuator_interface_defaults(&g_actuator_interface, iface);
    return ESP_OK;
}

