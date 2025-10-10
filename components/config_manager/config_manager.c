#include "config_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "CONFIG_MANAGER";

#define CONFIG_MANAGER_NAMESPACE      "hydro_cfg"
#define CONFIG_MANAGER_KEY            "system_cfg"
#define CONFIG_MANAGER_VERSION_KEY    "cfg_ver"
#define CONFIG_MANAGER_VERSION        1

static nvs_handle_t s_nvs_handle = 0;
static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;
static system_config_t s_cached_config = {0};
static bool s_cache_valid = false;

static const char *s_pump_names[PUMP_INDEX_COUNT] = {
    "pH Up", "pH Down", "EC A", "EC B", "EC C", "Water"
};

static void config_set_defaults(system_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(system_config_t));
    config->auto_control_enabled = true;
    config->display_brightness = 80;  // Яркость дисплея по умолчанию 80%

    const float sensor_targets[SENSOR_COUNT] = {
        PH_TARGET_DEFAULT,
        EC_TARGET_DEFAULT,
        TEMP_TARGET_DEFAULT,
        HUMIDITY_TARGET_DEFAULT,
        LUX_TARGET_DEFAULT,
        CO2_TARGET_DEFAULT,
    };

    const float sensor_alarm_low[SENSOR_COUNT] = {
        PH_ALARM_LOW_DEFAULT,
        EC_ALARM_LOW_DEFAULT,
        TEMP_ALARM_LOW_DEFAULT,
        HUMIDITY_ALARM_LOW_DEFAULT,
        LUX_ALARM_LOW_DEFAULT,
        CO2_ALARM_LOW_DEFAULT,
    };

    const float sensor_alarm_high[SENSOR_COUNT] = {
        PH_ALARM_HIGH_DEFAULT,
        EC_ALARM_HIGH_DEFAULT,
        TEMP_ALARM_HIGH_DEFAULT,
        HUMIDITY_ALARM_HIGH_DEFAULT,
        LUX_ALARM_HIGH_DEFAULT,
        CO2_ALARM_HIGH_DEFAULT,
    };

    for (int i = 0; i < SENSOR_COUNT; i++) {
        config->sensor_config[i].target_value = sensor_targets[i];
        config->sensor_config[i].alarm_low = sensor_alarm_low[i];
        config->sensor_config[i].alarm_high = sensor_alarm_high[i];
        config->sensor_config[i].enabled = true;
    }

    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pump_config_t *pump_cfg = &config->pump_config[i];
        memset(pump_cfg, 0, sizeof(pump_config_t));
        strncpy(pump_cfg->name, s_pump_names[i], sizeof(pump_cfg->name) - 1);
        pump_cfg->name[sizeof(pump_cfg->name) - 1] = '\0';
        pump_cfg->enabled = true;
        pump_cfg->flow_rate_ml_per_sec = PUMP_FLOW_RATE_DEFAULT;
        pump_cfg->min_duration_ms = PUMP_MIN_DURATION_MS;
        pump_cfg->max_duration_ms = PUMP_MAX_DURATION_MS;
        pump_cfg->cooldown_ms = PUMP_COOLDOWN_MS;
        pump_cfg->concentration_factor = 1.0f;
    }
}

static esp_err_t config_save_locked(const system_config_t *config)
{
    esp_err_t err = nvs_set_blob(s_nvs_handle, CONFIG_MANAGER_KEY, config, sizeof(system_config_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config blob: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(s_nvs_handle, CONFIG_MANAGER_VERSION_KEY, CONFIG_MANAGER_VERSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config version: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_commit(s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit config: %s", esp_err_to_name(err));
        return err;
    }

    s_cached_config = *config;
    s_cache_valid = true;
    return ESP_OK;
}

esp_err_t config_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create config mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = nvs_open(CONFIG_MANAGER_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", CONFIG_MANAGER_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    system_config_t defaults;
    config_set_defaults(&defaults);

    size_t size = sizeof(system_config_t);
    err = nvs_get_blob(s_nvs_handle, CONFIG_MANAGER_KEY, NULL, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND || size != sizeof(system_config_t)) {
        ESP_LOGW(TAG, "No stored config found, writing defaults");
        ESP_ERROR_CHECK_WITHOUT_ABORT(config_save_locked(&defaults));
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to query config blob: %s", esp_err_to_name(err));
        return err;
    } else {
        uint16_t stored_version = 0;
        err = nvs_get_u16(s_nvs_handle, CONFIG_MANAGER_VERSION_KEY, &stored_version);
        if (err != ESP_OK || stored_version != CONFIG_MANAGER_VERSION) {
            ESP_LOGW(TAG, "Config version mismatch (stored=%u, expected=%u) – resetting", stored_version, CONFIG_MANAGER_VERSION);
            ESP_ERROR_CHECK_WITHOUT_ABORT(config_save_locked(&defaults));
        } else {
            size = sizeof(system_config_t);
            err = nvs_get_blob(s_nvs_handle, CONFIG_MANAGER_KEY, &s_cached_config, &size);
            if (err == ESP_OK && size == sizeof(system_config_t)) {
                s_cache_valid = true;
            }
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Config manager initialized");
    return ESP_OK;
}

esp_err_t config_load(system_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    size_t size = sizeof(system_config_t);
    esp_err_t err = nvs_get_blob(s_nvs_handle, CONFIG_MANAGER_KEY, config, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND || size != sizeof(system_config_t)) {
        ESP_LOGW(TAG, "Config not found in NVS, using defaults");
        config_set_defaults(config);
        err = config_save_locked(config);
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config: %s", esp_err_to_name(err));
    } else {
        s_cached_config = *config;
        s_cache_valid = true;
    }

    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t config_save(const system_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = config_save_locked(config);

    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t config_manager_reset_to_defaults(system_config_t *out_config)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    system_config_t defaults;
    config_set_defaults(&defaults);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = config_save_locked(&defaults);
    xSemaphoreGive(s_mutex);

    if (err == ESP_OK && out_config != NULL) {
        *out_config = defaults;
    }

    return err;
}

const system_config_t *config_manager_get_cached(void)
{
    if (!s_initialized || !s_cache_valid) {
        return NULL;
    }
    return &s_cached_config;
}
