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
    
    // ========== IoT конфигурация по умолчанию ==========
    
    // WiFi
    strncpy(config->wifi.ssid, "HydroMonitor", sizeof(config->wifi.ssid) - 1);
    strncpy(config->wifi.password, "", sizeof(config->wifi.password) - 1);
    config->wifi.use_static_ip = false;
    strncpy(config->wifi.static_ip, "192.168.1.50", sizeof(config->wifi.static_ip) - 1);
    strncpy(config->wifi.gateway, "192.168.1.1", sizeof(config->wifi.gateway) - 1);
    strncpy(config->wifi.netmask, "255.255.255.0", sizeof(config->wifi.netmask) - 1);
    strncpy(config->wifi.dns, "8.8.8.8", sizeof(config->wifi.dns) - 1);
    config->wifi.auto_reconnect = true;
    config->wifi.network_mode = 0; // STA
    
    // MQTT
    strncpy(config->mqtt.broker_uri, "mqtt://192.168.1.100:1883", sizeof(config->mqtt.broker_uri) - 1);
    strncpy(config->mqtt.client_id, "hydro_gateway_001", sizeof(config->mqtt.client_id) - 1);
    strncpy(config->mqtt.username, "", sizeof(config->mqtt.username) - 1);
    strncpy(config->mqtt.password, "", sizeof(config->mqtt.password) - 1);
    config->mqtt.keepalive = 120;
    config->mqtt.auto_reconnect = true;
    config->mqtt.enabled = true;
    config->mqtt.publish_interval = 5;
    
    // Telegram
    strncpy(config->telegram.bot_token, "", sizeof(config->telegram.bot_token) - 1);
    strncpy(config->telegram.chat_id, "", sizeof(config->telegram.chat_id) - 1);
    config->telegram.enabled = false; // По умолчанию выключен
    config->telegram.enable_commands = true;
    config->telegram.report_hour = 20;
    config->telegram.notify_critical = true;
    config->telegram.notify_warnings = true;
    
    // SD-карта
    config->sd.enabled = true;
    config->sd.log_interval = 60;
    config->sd.cleanup_days = 30;
    config->sd.auto_sync = true;
    config->sd.sd_mode = 0; // SPI
    
    // Mesh
    config->mesh.enabled = false;
    config->mesh.role = 0; // Gateway
    config->mesh.device_id = 1;
    config->mesh.heartbeat_interval = 30;
    
    // AI
    config->ai.enabled = true;
    config->ai.min_confidence = 0.7f;
    config->ai.correction_interval = 300;
    config->ai.use_ml_model = false; // PID по умолчанию
    
    // ========== PID контроллеры для насосов ==========
    
    // pH UP PID (быстрая реакция на кислую среду)
    config->pump_pid[0].kp = 2.0f;
    config->pump_pid[0].ki = 0.5f;
    config->pump_pid[0].kd = 0.1f;
    config->pump_pid[0].output_min = 1.0f;
    config->pump_pid[0].output_max = 50.0f;
    config->pump_pid[0].enabled = true;
    config->pump_pid[0].auto_mode = true;
    
    // pH DOWN PID (быстрая реакция на щелочную среду)
    config->pump_pid[1].kp = 2.0f;
    config->pump_pid[1].ki = 0.5f;
    config->pump_pid[1].kd = 0.1f;
    config->pump_pid[1].output_min = 1.0f;
    config->pump_pid[1].output_max = 50.0f;
    config->pump_pid[1].enabled = true;
    config->pump_pid[1].auto_mode = true;
    
    // EC A PID (медленнее, питательные вещества)
    config->pump_pid[2].kp = 1.0f;
    config->pump_pid[2].ki = 0.2f;
    config->pump_pid[2].kd = 0.05f;
    config->pump_pid[2].output_min = 1.0f;
    config->pump_pid[2].output_max = 30.0f;
    config->pump_pid[2].enabled = true;
    config->pump_pid[2].auto_mode = true;
    
    // EC B PID (аналогично EC A)
    config->pump_pid[3].kp = 1.0f;
    config->pump_pid[3].ki = 0.2f;
    config->pump_pid[3].kd = 0.05f;
    config->pump_pid[3].output_min = 1.0f;
    config->pump_pid[3].output_max = 30.0f;
    config->pump_pid[3].enabled = true;
    config->pump_pid[3].auto_mode = true;
    
    // EC C PID (еще медленнее, микроэлементы)
    config->pump_pid[4].kp = 0.8f;
    config->pump_pid[4].ki = 0.15f;
    config->pump_pid[4].kd = 0.03f;
    config->pump_pid[4].output_min = 0.5f;
    config->pump_pid[4].output_max = 15.0f;
    config->pump_pid[4].enabled = true;
    config->pump_pid[4].auto_mode = true;
    
    // WATER PID (только разбавление, без D)
    config->pump_pid[5].kp = 0.5f;
    config->pump_pid[5].ki = 0.1f;
    config->pump_pid[5].kd = 0.0f;
    config->pump_pid[5].output_min = 5.0f;
    config->pump_pid[5].output_max = 100.0f;
    config->pump_pid[5].enabled = true;
    config->pump_pid[5].auto_mode = false; // Ручной по умолчанию
    
    // Режим управления
    config->control_mode = 1; // PID по умолчанию
    
    // Системные
    config->display_brightness = 80;
    strncpy(config->device_name, "HydroMonitor-ESP32S3", sizeof(config->device_name) - 1);
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
