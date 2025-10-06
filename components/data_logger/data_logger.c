#include "data_logger.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "DATA_LOGGER";

#define DATA_LOGGER_NAMESPACE      "hydro_logs"
#define DATA_LOGGER_KEY_HEADER     "meta"
#define DATA_LOGGER_KEY_ENTRIES    "entries"
#define DATA_LOGGER_STORAGE_VER    1
#define DATA_LOGGER_PERSIST_SEC    300

static log_entry_t *g_log_entries = NULL;
static uint32_t g_max_entries = 0;
static uint32_t g_entry_count = 0;
static uint32_t g_head_index = 0;
static uint32_t g_next_id = 1;
static SemaphoreHandle_t g_mutex = NULL;
static data_logger_callback_t g_callback = NULL;
static bool g_auto_cleanup = false;
static uint32_t g_auto_cleanup_days = LOG_AUTO_CLEANUP_DAYS;
static bool g_dirty = false;
static time_t g_last_persist_ts = 0;

typedef struct {
    uint32_t version;
    uint32_t count;
    uint32_t next_id;
} log_storage_header_t;

static esp_err_t ensure_mutex(void)
{
    if (g_mutex == NULL) {
        g_mutex = xSemaphoreCreateMutex();
        if (g_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static uint32_t get_storage_index(uint32_t logical_index)
{
    return (g_head_index + logical_index) % g_max_entries;
}

static void dispatch_callback(const log_entry_t *entry)
{
    if (g_callback == NULL || entry == NULL) {
        return;
    }

    data_logger_entry_t cb_entry = {
        .id = entry->id,
        .timestamp = entry->timestamp,
        .type = entry->type,
        .sensor_type = 0,
        .value = 0.0f,
    };
    strncpy(cb_entry.message, entry->message, sizeof(cb_entry.message) - 1);
    cb_entry.message[sizeof(cb_entry.message) - 1] = '\0';

    g_callback(&cb_entry);
}

static void store_entry(const log_entry_t *entry)
{
    if (g_entry_count < g_max_entries) {
        uint32_t index = get_storage_index(g_entry_count);
        g_log_entries[index] = *entry;
        g_entry_count++;
    } else {
        g_log_entries[g_head_index] = *entry;
        g_head_index = (g_head_index + 1) % g_max_entries;
    }
    g_dirty = true;
    dispatch_callback(entry);
}

static void remove_old_entries_locked(time_t threshold)
{
    while (g_entry_count > 0) {
        log_entry_t *oldest = &g_log_entries[g_head_index];
        if ((time_t)oldest->timestamp >= threshold) {
            break;
        }
        memset(oldest, 0, sizeof(log_entry_t));
        g_head_index = (g_head_index + 1) % g_max_entries;
        g_entry_count--;
        g_dirty = true;
    }

    if (g_entry_count == 0) {
        g_head_index = 0;
    }
}

static esp_err_t save_locked(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DATA_LOGGER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", DATA_LOGGER_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    log_storage_header_t header = {
        .version = DATA_LOGGER_STORAGE_VER,
        .count = g_entry_count,
        .next_id = g_next_id,
    };

    err = nvs_set_blob(handle, DATA_LOGGER_KEY_HEADER, &header, sizeof(header));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write log header: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    size_t blob_size = g_entry_count * sizeof(log_entry_t);
    if (blob_size > 0) {
        log_entry_t *buffer = malloc(blob_size);
        if (buffer == NULL) {
            nvs_close(handle);
            ESP_LOGE(TAG, "Failed to allocate buffer for log persistence");
            return ESP_ERR_NO_MEM;
        }

        for (uint32_t i = 0; i < g_entry_count; i++) {
            buffer[i] = g_log_entries[get_storage_index(i)];
        }

        err = nvs_set_blob(handle, DATA_LOGGER_KEY_ENTRIES, buffer, blob_size);
        free(buffer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write log entries: %s", esp_err_to_name(err));
            nvs_close(handle);
            return err;
        }
    } else {
        err = nvs_erase_key(handle, DATA_LOGGER_KEY_ENTRIES);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) {
        g_dirty = false;
        g_last_persist_ts = time(NULL);
        ESP_LOGI(TAG, "Persisted %lu log entries", (unsigned long)g_entry_count);
    }

    return err;
}

static esp_err_t load_locked(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DATA_LOGGER_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", DATA_LOGGER_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    log_storage_header_t header = {0};
    size_t header_size = sizeof(header);
    err = nvs_get_blob(handle, DATA_LOGGER_KEY_HEADER, &header, &header_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_OK;
    } else if (err != ESP_OK || header.version != DATA_LOGGER_STORAGE_VER) {
        ESP_LOGW(TAG, "Log storage header missing or incompatible, ignoring stored logs");
        nvs_close(handle);
        return ESP_OK;
    }

    size_t blob_size = 0;
    err = nvs_get_blob(handle, DATA_LOGGER_KEY_ENTRIES, NULL, &blob_size);
    if (err == ESP_ERR_NVS_NOT_FOUND || blob_size == 0) {
        g_entry_count = 0;
        g_head_index = 0;
        g_next_id = header.next_id;
        nvs_close(handle);
        return ESP_OK;
    } else if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to query log entries blob: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t stored_count = blob_size / sizeof(log_entry_t);
    if (stored_count > g_max_entries) {
        stored_count = g_max_entries;
    }

    log_entry_t *buffer = malloc(stored_count * sizeof(log_entry_t));
    if (buffer == NULL) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to allocate buffer for log load");
        return ESP_ERR_NO_MEM;
    }

    err = nvs_get_blob(handle, DATA_LOGGER_KEY_ENTRIES, buffer, &blob_size);
    nvs_close(handle);
    if (err != ESP_OK) {
        free(buffer);
        ESP_LOGE(TAG, "Failed to read log entries: %s", esp_err_to_name(err));
        return err;
    }

    memset(g_log_entries, 0, g_max_entries * sizeof(log_entry_t));
    g_entry_count = stored_count;
    g_head_index = 0;
    for (uint32_t i = 0; i < stored_count; i++) {
        g_log_entries[i] = buffer[i];
    }
    g_next_id = header.next_id;
    free(buffer);
    g_dirty = false;
    ESP_LOGI(TAG, "Restored %lu log entries from NVS", (unsigned long)g_entry_count);
    return ESP_OK;
}

esp_err_t data_logger_init(uint32_t max_entries)
{
    if (g_log_entries != NULL) {
        ESP_LOGW(TAG, "Data logger already initialized");
        return ESP_OK;
    }

    if (max_entries == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ensure_mutex();
    if (err != ESP_OK) {
        return err;
    }

    g_log_entries = calloc(max_entries, sizeof(log_entry_t));
    if (g_log_entries == NULL) {
        ESP_LOGE(TAG, "Failed to allocate log buffer");
        return ESP_ERR_NO_MEM;
    }

    g_max_entries = max_entries;
    g_entry_count = 0;
    g_head_index = 0;
    g_next_id = 1;
    g_dirty = false;
    g_last_persist_ts = time(NULL);

    ESP_LOGI(TAG, "Data logger initialized (capacity: %lu entries)", (unsigned long)max_entries);
    return ESP_OK;
}

static esp_err_t log_generic_entry(log_entry_t *entry)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ensure_mutex();
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    entry->id = g_next_id++;
    entry->timestamp = (uint32_t)time(NULL);

    store_entry(entry);

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t data_logger_log_sensor_data(float ph, float ec, float temp,
                                      float hum, float lux, float co2)
{
    log_entry_t entry = {
        .type = LOG_TYPE_SENSOR_DATA,
        .level = LOG_LEVEL_INFO,
        .ph = ph,
        .ec = ec,
        .temperature = temp,
        .humidity = hum,
        .lux = lux,
        .co2 = co2,
    };
    snprintf(entry.message, sizeof(entry.message),
             "pH:%.2f EC:%.2f T:%.1f H:%.1f L:%.0f CO2:%.0f",
             ph, ec, temp, hum, lux, co2);
    return log_generic_entry(&entry);
}

esp_err_t data_logger_log_alarm(log_level_t level, const char *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    log_entry_t entry = {
        .type = LOG_TYPE_ALARM,
        .level = level,
    };
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    return log_generic_entry(&entry);
}

esp_err_t data_logger_log_pump_action(uint8_t pump_id, uint32_t duration_ms,
                                      const char *message)
{
    log_entry_t entry = {
        .type = LOG_TYPE_PUMP_ACTION,
        .level = LOG_LEVEL_INFO,
    };

    if (message != NULL) {
        snprintf(entry.message, sizeof(entry.message),
                 "Pump %u: %s (%lums)", pump_id, message, (unsigned long)duration_ms);
    } else {
        snprintf(entry.message, sizeof(entry.message),
                 "Pump %u action (%lums)", pump_id, (unsigned long)duration_ms);
    }

    return log_generic_entry(&entry);
}

uint32_t data_logger_get_count(void)
{
    if (g_log_entries == NULL) {
        return 0;
    }

    if (ensure_mutex() != ESP_OK) {
        return 0;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    uint32_t count = g_entry_count;
    xSemaphoreGive(g_mutex);
    return count;
}

esp_err_t data_logger_clear(void)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ensure_mutex() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memset(g_log_entries, 0, g_max_entries * sizeof(log_entry_t));
    g_entry_count = 0;
    g_head_index = 0;
    g_dirty = true;

    xSemaphoreGive(g_mutex);
    ESP_LOGI(TAG, "All log entries cleared");
    return ESP_OK;
}

esp_err_t data_logger_save_to_nvs(void)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ensure_mutex() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = save_locked();

    xSemaphoreGive(g_mutex);
    return err;
}

esp_err_t data_logger_load_from_nvs(void)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ensure_mutex() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = load_locked();

    xSemaphoreGive(g_mutex);
    return err;
}

esp_err_t data_logger_set_callback(data_logger_callback_t callback)
{
    g_callback = callback;
    ESP_LOGI(TAG, "Data logger callback set");
    return ESP_OK;
}

esp_err_t data_logger_set_auto_cleanup(bool enabled, uint32_t days)
{
    g_auto_cleanup = enabled;
    g_auto_cleanup_days = (days == 0) ? LOG_AUTO_CLEANUP_DAYS : days;
    ESP_LOGI(TAG, "Auto cleanup %s (%u days)", enabled ? "enabled" : "disabled", (unsigned)g_auto_cleanup_days);
    return ESP_OK;
}

esp_err_t data_logger_log_user_action(const char *action, const char *details)
{
    if (action == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    log_entry_t entry = {
        .type = DATA_LOG_USER_ACTION,
        .level = LOG_LEVEL_INFO,
    };

    if (details != NULL) {
        snprintf(entry.message, sizeof(entry.message), "%s: %s", action, details);
    } else {
        strncpy(entry.message, action, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';
    }

    return log_generic_entry(&entry);
}

esp_err_t data_logger_process(void)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ensure_mutex() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    time_t now = time(NULL);

    if (g_auto_cleanup && g_entry_count > 0 && g_auto_cleanup_days > 0) {
        time_t threshold = now - (time_t)g_auto_cleanup_days * 86400;
        remove_old_entries_locked(threshold);
    }

    if (g_dirty && (now - g_last_persist_ts) >= DATA_LOGGER_PERSIST_SEC) {
        save_locked();
    }

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t data_logger_log_system_event(log_level_t level, const char *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    log_entry_t entry = {
        .type = LOG_TYPE_SYSTEM_EVENT,
        .level = level,
    };
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    return log_generic_entry(&entry);
}

const char* data_logger_type_to_string(log_record_type_t type)
{
    switch (type) {
        case LOG_TYPE_SENSOR_DATA: return "sensor";
        case LOG_TYPE_ALARM: return "alarm";
        case LOG_TYPE_PUMP_ACTION: return "pump";
        case LOG_TYPE_SYSTEM_EVENT: return "system";
        case DATA_LOG_USER_ACTION: return "user";
        case DATA_LOG_SYSTEM_EVENT: return "system-legacy";
        default: return "unknown";
    }
}
