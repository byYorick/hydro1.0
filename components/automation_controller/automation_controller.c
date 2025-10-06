#include "automation_controller.h"
#include "peristaltic_pump.h"
#include "trema_relay.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
#include <time.h>

static const char *TAG = "automation_ctrl";

typedef struct {
    int ia;
    int ib;
    const char *label;
} pump_channel_t;

static automation_pump_pins_t s_pins;
static hydro_settings_t s_settings;
static bool s_initialized = false;
static int64_t s_last_ph_acid_ms = 0;
static int64_t s_last_ph_base_ms = 0;
static int64_t s_last_ec_dose_ms = 0;
static pump_channel_t s_ec_channels[3];
static size_t s_ec_channel_count = 0;
static size_t s_next_ec_channel = 0;
static bool s_lighting_state_known = false;
static bool s_lighting_is_on = false;
static bool s_time_warning_logged = false;
static bool s_relay_auto_switch_forced_off = false;

static bool pins_valid(int ia, int ib)
{
    return ia >= 0 && ib >= 0;
}

static void run_pump(int ia, int ib, uint32_t duration_ms, const char *label)
{
    if (!pins_valid(ia, ib)) {
        ESP_LOGW(TAG, "Pump %s pins not configured", label);
        return;
    }

    ESP_LOGI(TAG, "Running pump %s for %ums", label, duration_ms);
    pump_run_ms(ia, ib, duration_ms);
}

static bool get_current_hour(uint8_t *hour)
{
    if (!hour) {
        return false;
    }

    time_t now = 0;
    time(&now);
    if (now <= 0) {
        const int64_t uptime_hours = (esp_timer_get_time() / 1000000LL) / 3600LL;
        *hour = (uint8_t)(uptime_hours % 24LL);
        return false;
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    *hour = (uint8_t)timeinfo.tm_hour;
    return true;
}

static bool schedule_active(uint8_t on_hour, uint8_t off_hour, uint8_t current_hour)
{
    if (on_hour == off_hour) {
        return true;
    }

    if (on_hour < off_hour) {
        return current_hour >= on_hour && current_hour < off_hour;
    }

    return current_hour >= on_hour || current_hour < off_hour;
}

static void set_lighting_output(bool on, const char *reason)
{
    if (s_lighting_state_known && s_lighting_is_on == on) {
        return;
    }

    trema_relay_digital_write(0, on ? 1 : 0);
    s_lighting_is_on = on;
    s_lighting_state_known = true;
    ESP_LOGI(TAG, "Lighting %s (%s)", on ? "ON" : "OFF", reason);
}

static void update_lighting_state(bool force)
{
    if (!s_relay_auto_switch_forced_off) {
        trema_relay_auto_switch(false);
        s_relay_auto_switch_forced_off = true;
    }

    if (!s_settings.lighting_auto_mode) {
        if (force || !s_lighting_state_known || s_lighting_is_on != s_settings.lighting_manual_state) {
            set_lighting_output(s_settings.lighting_manual_state, "manual override");
        }
        return;
    }

    uint8_t current_hour = 0;
    const bool rtc_valid = get_current_hour(&current_hour);
    if (!rtc_valid && !s_time_warning_logged) {
        ESP_LOGW(TAG, "RTC time not set, using uptime hours for lighting schedule");
        s_time_warning_logged = true;
    }

    const bool should_enable = schedule_active(s_settings.lighting_on_hour, s_settings.lighting_off_hour, current_hour);
    if (force || !s_lighting_state_known || s_lighting_is_on != should_enable) {
        set_lighting_output(should_enable, "auto schedule");
    }
}

static int64_t get_cooldown_interval_ms(void)
{
    return (int64_t)s_settings.dosing_cooldown_s * 1000LL;
}

static bool cooldown_elapsed(int64_t now_ms, int64_t last_ms)
{
    const int64_t cooldown_ms = get_cooldown_interval_ms();
    if (cooldown_ms == 0) {
        return true;
    }
    return last_ms == 0 || (now_ms - last_ms) >= cooldown_ms;
}

static bool value_valid(float value)
{
    return !isnan(value) && !isinf(value);
}

static void prime_ec_channels(void)
{
    s_ec_channel_count = 0;
    s_next_ec_channel = 0;

    const pump_channel_t ec_candidates[] = {
        { s_pins.ec_a_ia, s_pins.ec_a_ib, "EC A" },
        { s_pins.ec_b_ia, s_pins.ec_b_ib, "EC B" },
        { s_pins.ec_c_ia, s_pins.ec_c_ib, "EC C" }
    };

    for (size_t i = 0; i < sizeof(ec_candidates) / sizeof(ec_candidates[0]); ++i) {
        if (pins_valid(ec_candidates[i].ia, ec_candidates[i].ib)) {
            s_ec_channels[s_ec_channel_count++] = ec_candidates[i];
        }
    }
}

static pump_channel_t *next_ec_channel(void)
{
    if (s_ec_channel_count == 0) {
        return NULL;
    }

    pump_channel_t *channel = &s_ec_channels[s_next_ec_channel];
    s_next_ec_channel = (s_next_ec_channel + 1) % s_ec_channel_count;
    return channel;
}

void automation_controller_init(const automation_pump_pins_t *pins, const hydro_settings_t *initial_settings)
{
    if (!pins || !initial_settings) {
        ESP_LOGE(TAG, "Invalid initialization parameters");
        return;
    }

    s_pins = *pins;
    s_settings = *initial_settings;

    if (pins_valid(s_pins.ph_acid_ia, s_pins.ph_acid_ib)) {
        pump_init(s_pins.ph_acid_ia, s_pins.ph_acid_ib);
    }
    if (pins_valid(s_pins.ph_base_ia, s_pins.ph_base_ib)) {
        pump_init(s_pins.ph_base_ia, s_pins.ph_base_ib);
    }
    if (pins_valid(s_pins.ec_a_ia, s_pins.ec_a_ib)) {
        pump_init(s_pins.ec_a_ia, s_pins.ec_a_ib);
    }
    if (pins_valid(s_pins.ec_b_ia, s_pins.ec_b_ib)) {
        pump_init(s_pins.ec_b_ia, s_pins.ec_b_ib);
    }
    if (pins_valid(s_pins.ec_c_ia, s_pins.ec_c_ib)) {
        pump_init(s_pins.ec_c_ia, s_pins.ec_c_ib);
    }

    prime_ec_channels();

    s_last_ph_acid_ms = 0;
    s_last_ph_base_ms = 0;
    s_last_ec_dose_ms = 0;
    s_lighting_state_known = false;
    s_time_warning_logged = false;
    s_relay_auto_switch_forced_off = false;

    update_lighting_state(true);

    s_initialized = true;
    ESP_LOGI(TAG, "Automation controller initialized");
}

void automation_controller_apply_settings(const hydro_settings_t *settings)
{
    if (!settings) {
        return;
    }

    s_settings = *settings;
    prime_ec_channels();
    s_relay_auto_switch_forced_off = false;
    update_lighting_state(true);
}

void automation_controller_update(const automation_sensor_data_t *data)
{
    if (!s_initialized || !data) {
        return;
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;

    update_lighting_state(false);

    if (!s_settings.auto_dosing_enabled) {
        return;
    }

    if (!value_valid(data->ph) || !value_valid(data->ec)) {
        ESP_LOGW(TAG, "Invalid sensor data (pH=%.2f, EC=%.2f), skipping dosing", data->ph, data->ec);
        return;
    }

    const float ph_high_threshold = s_settings.target_ph + s_settings.ph_tolerance;
    const float ph_low_threshold = s_settings.target_ph - s_settings.ph_tolerance;

    if (data->ph > ph_high_threshold && pins_valid(s_pins.ph_acid_ia, s_pins.ph_acid_ib)) {
        if (cooldown_elapsed(now_ms, s_last_ph_acid_ms)) {
            run_pump(s_pins.ph_acid_ia, s_pins.ph_acid_ib, s_settings.dosing_duration_ms, "pH acid");
            s_last_ph_acid_ms = now_ms;
        } else {
            ESP_LOGD(TAG, "pH acid pump cooldown active");
        }
    } else if (data->ph < ph_low_threshold && pins_valid(s_pins.ph_base_ia, s_pins.ph_base_ib)) {
        if (cooldown_elapsed(now_ms, s_last_ph_base_ms)) {
            run_pump(s_pins.ph_base_ia, s_pins.ph_base_ib, s_settings.dosing_duration_ms, "pH base");
            s_last_ph_base_ms = now_ms;
        } else {
            ESP_LOGD(TAG, "pH base pump cooldown active");
        }
    }

    const float ec_low_threshold = s_settings.target_ec - s_settings.ec_tolerance;
    if (data->ec < ec_low_threshold && s_settings.dosing_duration_ms > 0) {
        if (cooldown_elapsed(now_ms, s_last_ec_dose_ms)) {
            pump_channel_t *channel = next_ec_channel();
            if (channel) {
                run_pump(channel->ia, channel->ib, s_settings.dosing_duration_ms, channel->label);
                s_last_ec_dose_ms = now_ms;
            } else {
                ESP_LOGW(TAG, "No EC pumps configured for dosing");
            }
        } else {
            ESP_LOGD(TAG, "EC pump cooldown active");
        }
    }
}
