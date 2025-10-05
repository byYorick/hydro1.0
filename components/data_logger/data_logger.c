#include "data_logger.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "DATA_LOGGER";

// Глобальные переменные
static log_entry_t *g_log_entries = NULL;
static uint32_t g_max_entries = 0;
static uint32_t g_entry_count = 0;
static uint32_t g_next_id = 1;
static SemaphoreHandle_t g_mutex = NULL;
static data_logger_callback_t g_callback = NULL;
static bool g_auto_cleanup = false;

esp_err_t data_logger_init(uint32_t max_entries)
{
    if (g_log_entries != NULL) {
        ESP_LOGW(TAG, "Data logger already initialized");
        return ESP_OK;
    }

    g_log_entries = calloc(max_entries, sizeof(log_entry_t));
    if (g_log_entries == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for log entries");
        return ESP_ERR_NO_MEM;
    }

    g_max_entries = max_entries;
    g_entry_count = 0;
    g_next_id = 1;

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        free(g_log_entries);
        g_log_entries = NULL;
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Data logger initialized (max entries: %lu)", (unsigned long)max_entries);
    return ESP_OK;
}

esp_err_t data_logger_log_sensor_data(float ph, float ec, float temp,
                                      float hum, float lux, float co2)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Если достигнут лимит, удаляем самую старую запись
    if (g_entry_count >= g_max_entries) {
        memmove(&g_log_entries[0], &g_log_entries[1],
                (g_max_entries - 1) * sizeof(log_entry_t));
        g_entry_count--;
    }

    // Создаем новую запись
    log_entry_t *entry = &g_log_entries[g_entry_count];
    entry->id = g_next_id++;
    entry->type = LOG_TYPE_SENSOR_DATA;
    entry->level = LOG_LEVEL_INFO;
    entry->timestamp = (uint32_t)time(NULL);
    entry->ph = ph;
    entry->ec = ec;
    entry->temperature = temp;
    entry->humidity = hum;
    entry->lux = lux;
    entry->co2 = co2;
    snprintf(entry->message, sizeof(entry->message),
             "pH:%.2f EC:%.2f T:%.1f H:%.1f L:%.0f CO2:%.0f",
             ph, ec, temp, hum, lux, co2);

    g_entry_count++;

    xSemaphoreGive(g_mutex);

    ESP_LOGD(TAG, "Logged sensor data entry %lu", (unsigned long)entry->id);
    return ESP_OK;
}

esp_err_t data_logger_log_alarm(log_level_t level, const char *message)
{
    if (g_log_entries == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (g_entry_count >= g_max_entries) {
        memmove(&g_log_entries[0], &g_log_entries[1],
                (g_max_entries - 1) * sizeof(log_entry_t));
        g_entry_count--;
    }

    log_entry_t *entry = &g_log_entries[g_entry_count];
    entry->id = g_next_id++;
    entry->type = LOG_TYPE_ALARM;
    entry->level = level;
    entry->timestamp = (uint32_t)time(NULL);
    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';

    g_entry_count++;

    xSemaphoreGive(g_mutex);

    ESP_LOGW(TAG, "Logged alarm entry %lu: %s", (unsigned long)entry->id, entry->message);
    return ESP_OK;
}

esp_err_t data_logger_log_pump_action(uint8_t pump_id, uint32_t duration_ms,
                                      const char *message)
{
    if (g_log_entries == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (g_entry_count >= g_max_entries) {
        memmove(&g_log_entries[0], &g_log_entries[1],
                (g_max_entries - 1) * sizeof(log_entry_t));
        g_entry_count--;
    }

    log_entry_t *entry = &g_log_entries[g_entry_count];
    entry->id = g_next_id++;
    entry->type = LOG_TYPE_PUMP_ACTION;
    entry->level = LOG_LEVEL_INFO;
    entry->timestamp = (uint32_t)time(NULL);

    if (message != NULL) {
        snprintf(entry->message, sizeof(entry->message),
                 "Pump %d: %s (%lums)", pump_id, message, (unsigned long)duration_ms);
    } else {
        snprintf(entry->message, sizeof(entry->message),
                 "Pump %d action (%lums)", pump_id, (unsigned long)duration_ms);
    }

    g_entry_count++;

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Logged pump action entry %lu: %s", (unsigned long)entry->id, entry->message);
    return ESP_OK;
}

uint32_t data_logger_get_count(void)
{
    if (g_log_entries == NULL) {
        return 0;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
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

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_entry_count = 0;
    memset(g_log_entries, 0, g_max_entries * sizeof(log_entry_t));

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "All log entries cleared");
    return ESP_OK;
}

esp_err_t data_logger_save_to_nvs(void)
{
    ESP_LOGI(TAG, "NVS save not implemented yet");
    return ESP_OK;
}

esp_err_t data_logger_load_from_nvs(void)
{
    ESP_LOGI(TAG, "NVS load not implemented yet");
    return ESP_OK;
}

// Установка callback для логирования
esp_err_t data_logger_set_callback(data_logger_callback_t callback)
{
    g_callback = callback;
    ESP_LOGI(TAG, "Data logger callback set");
    return ESP_OK;
}

// Установка автоматической очистки
esp_err_t data_logger_set_auto_cleanup(bool enabled, uint32_t days)
{
    g_auto_cleanup = enabled;
    ESP_LOGI(TAG, "Auto cleanup %s (%lu days)", enabled ? "enabled" : "disabled", (unsigned long)days);
    return ESP_OK;
}

// Логирование действий пользователя
esp_err_t data_logger_log_user_action(const char *action, const char *details)
{
    if (action == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data_logger_entry_t entry = {
        .id = g_next_id++,
        .timestamp = (uint32_t)time(NULL),
        .type = DATA_LOG_USER_ACTION,
        .sensor_type = 0,
        .value = 0.0f,
        .message = {0}
    };

    if (details != NULL) {
        snprintf(entry.message, sizeof(entry.message), "%s: %s", action, details);
    } else {
        strncpy(entry.message, action, sizeof(entry.message) - 1);
    }
    entry.message[sizeof(entry.message) - 1] = '\0';

    // Используем существующую функцию логирования
    log_entry_t log_entry = {
        .id = entry.id,
        .type = LOG_TYPE_SYSTEM_EVENT,
        .level = LOG_LEVEL_INFO,
        .timestamp = entry.timestamp,
        .ph = 0.0f,
        .ec = 0.0f,
        .temperature = 0.0f,
        .humidity = 0.0f,
        .lux = 0.0f,
        .co2 = 0.0f
    };
    strncpy(log_entry.message, entry.message, sizeof(log_entry.message) - 1);
    log_entry.message[sizeof(log_entry.message) - 1] = '\0';

    // Используем существующую функцию логирования
    return data_logger_log_sensor_data(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// Обработка логирования (заглушка)
esp_err_t data_logger_process(void)
{
    // В данной реализации обработка не требуется
    return ESP_OK;
}

// Логирование системных событий
esp_err_t data_logger_log_system_event(log_level_t level, const char *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    log_entry_t log_entry = {
        .id = g_next_id++,
        .type = LOG_TYPE_SYSTEM_EVENT,
        .level = level,
        .timestamp = (uint32_t)time(NULL),
        .ph = 0.0f,
        .ec = 0.0f,
        .temperature = 0.0f,
        .humidity = 0.0f,
        .lux = 0.0f,
        .co2 = 0.0f
    };

    strncpy(log_entry.message, message, sizeof(log_entry.message) - 1);
    log_entry.message[sizeof(log_entry.message) - 1] = '\0';

    // Используем существующую функцию логирования
    return data_logger_log_sensor_data(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

