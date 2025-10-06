#include "hydro_settings.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

#define NVS_NAMESPACE "hydro_cfg"
#define NVS_KEY_SETTINGS "settings"
#define MAX_SETTINGS_LISTENERS 5

static const char *TAG = "hydro_settings";

static hydro_settings_t s_settings = {
    .version = HYDRO_SETTINGS_VERSION,
    .target_ph = 6.2f,
    .ph_tolerance = 0.3f,
    .target_ec = 1.8f,
    .ec_tolerance = 0.2f,
    .dosing_duration_ms = 500,
    .dosing_cooldown_s = 120,
    .auto_dosing_enabled = true,
    .lighting_auto_mode = true,
    .lighting_manual_state = true,
    .lighting_on_hour = 6,
    .lighting_off_hour = 22
};

static hydro_settings_listener_t s_listeners[MAX_SETTINGS_LISTENERS];
static bool s_initialized = false;

static esp_err_t update_and_save(void);

static void notify_listeners(void)
{
    for (size_t i = 0; i < MAX_SETTINGS_LISTENERS; ++i) {
        if (s_listeners[i]) {
            s_listeners[i](&s_settings);
        }
    }
}

static bool float_changed(float current, float next, float epsilon)
{
    return fabsf(current - next) >= epsilon;
}

static esp_err_t commit_if_changed_bool(bool *field, bool value)
{
    if (*field == value) {
        return ESP_OK;
    }
    *field = value;
    return update_and_save();
}

static esp_err_t commit_if_changed_u32(uint32_t *field, uint32_t value)
{
    if (*field == value) {
        return ESP_OK;
    }
    *field = value;
    return update_and_save();
}

static esp_err_t commit_if_changed_float(float *field, float value, float epsilon)
{
    if (!float_changed(*field, value, epsilon)) {
        return ESP_OK;
    }
    *field = value;
    return update_and_save();
}

static uint8_t normalize_hour(uint8_t hour)
{
    return hour % 24;
}

static esp_err_t save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save settings: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings saved to NVS");
    }

    return err;
}

static void load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS (using defaults): %s", esp_err_to_name(err));
        return;
    }

    size_t required_size = sizeof(s_settings);
    hydro_settings_t loaded_settings;
    memset(&loaded_settings, 0, sizeof(loaded_settings));
    err = nvs_get_blob(handle, NVS_KEY_SETTINGS, &loaded_settings, &required_size);
    if (err == ESP_OK && required_size == sizeof(s_settings)) {
        if (loaded_settings.version == HYDRO_SETTINGS_VERSION) {
            s_settings = loaded_settings;
            ESP_LOGI(TAG, "Settings loaded from NVS");
        } else {
            ESP_LOGW(TAG, "Settings version mismatch (stored=%u, expected=%u). Using defaults.",
                     loaded_settings.version, HYDRO_SETTINGS_VERSION);
            save_to_nvs();
        }
    } else {
        ESP_LOGW(TAG, "No stored settings found. Saving defaults.");
        save_to_nvs();
    }

    nvs_close(handle);
}

esp_err_t hydro_settings_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    load_from_nvs();
    s_initialized = true;
    notify_listeners();
    return ESP_OK;
}

const hydro_settings_t *hydro_settings_get(void)
{
    return &s_settings;
}

static esp_err_t update_and_save(void)
{
    esp_err_t err = save_to_nvs();
    if (err == ESP_OK) {
        notify_listeners();
    }
    return err;
}

esp_err_t hydro_settings_set_target_ph(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 14.0f) {
        value = 14.0f;
    }
    return commit_if_changed_float(&s_settings.target_ph, value, 0.01f);
}

esp_err_t hydro_settings_set_ph_tolerance(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 7.0f) {
        value = 7.0f;
    }
    return commit_if_changed_float(&s_settings.ph_tolerance, value, 0.01f);
}

esp_err_t hydro_settings_set_target_ec(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 10.0f) {
        value = 10.0f;
    }
    return commit_if_changed_float(&s_settings.target_ec, value, 0.01f);
}

esp_err_t hydro_settings_set_ec_tolerance(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 5.0f) {
        value = 5.0f;
    }
    return commit_if_changed_float(&s_settings.ec_tolerance, value, 0.01f);
}

esp_err_t hydro_settings_set_dosing_duration(uint32_t duration_ms)
{
    if (duration_ms < 100) {
        duration_ms = 100;
    }
    if (duration_ms > 10000) {
        duration_ms = 10000;
    }
    return commit_if_changed_u32(&s_settings.dosing_duration_ms, duration_ms);
}

esp_err_t hydro_settings_set_dosing_cooldown(uint32_t cooldown_seconds)
{
    if (cooldown_seconds < 10) {
        cooldown_seconds = 10;
    }
    if (cooldown_seconds > 3600) {
        cooldown_seconds = 3600;
    }
    return commit_if_changed_u32(&s_settings.dosing_cooldown_s, cooldown_seconds);
}

esp_err_t hydro_settings_set_auto_dosing_enabled(bool enabled)
{
    return commit_if_changed_bool(&s_settings.auto_dosing_enabled, enabled);
}

esp_err_t hydro_settings_set_lighting_auto_mode(bool enabled)
{
    return commit_if_changed_bool(&s_settings.lighting_auto_mode, enabled);
}

esp_err_t hydro_settings_set_lighting_manual_state(bool on)
{
    return commit_if_changed_bool(&s_settings.lighting_manual_state, on);
}

esp_err_t hydro_settings_set_lighting_schedule(uint8_t on_hour, uint8_t off_hour)
{
    on_hour = normalize_hour(on_hour);
    off_hour = normalize_hour(off_hour);

    if (s_settings.lighting_on_hour == on_hour && s_settings.lighting_off_hour == off_hour) {
        return ESP_OK;
    }
    s_settings.lighting_on_hour = on_hour;
    s_settings.lighting_off_hour = off_hour;
    return update_and_save();
}

esp_err_t hydro_settings_register_listener(hydro_settings_listener_t listener)
{
    if (!listener) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < MAX_SETTINGS_LISTENERS; ++i) {
        if (s_listeners[i] == listener) {
            return ESP_OK;
        }
        if (s_listeners[i] == NULL) {
            s_listeners[i] = listener;
            if (s_initialized) {
                listener(&s_settings);
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}
