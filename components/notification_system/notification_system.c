#include "notification_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "NOTIF_SYS";

// Глобальные переменные
static notification_t *g_notifications = NULL;
static uint32_t g_max_notifications = 0;
static uint32_t g_notification_count = 0;
static uint32_t g_next_id = 1;
static SemaphoreHandle_t g_mutex = NULL;
static notification_callback_t g_callback = NULL;

esp_err_t notification_system_init(uint32_t max_notifications)
{
    if (g_notifications != NULL) {
        ESP_LOGW(TAG, "Notification system already initialized");
        return ESP_OK;
    }

    g_notifications = calloc(max_notifications, sizeof(notification_t));
    if (g_notifications == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for notifications");
        return ESP_ERR_NO_MEM;
    }

    g_max_notifications = max_notifications;
    g_notification_count = 0;
    g_next_id = 1;

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        free(g_notifications);
        g_notifications = NULL;
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Notification system initialized (max: %lu)", (unsigned long)max_notifications);
    return ESP_OK;
}

uint32_t notification_create(notification_type_t type,
                             notification_priority_t priority,
                             notification_source_t source,
                             const char *message)
{
    if (g_notifications == NULL || message == NULL) {
        return 0;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return 0;
    }

    // Если достигнут лимит, удаляем самое старое уведомление
    if (g_notification_count >= g_max_notifications) {
        // Сдвигаем все уведомления
        memmove(&g_notifications[0], &g_notifications[1], 
                (g_max_notifications - 1) * sizeof(notification_t));
        g_notification_count--;
    }

    // Создаем новое уведомление
    notification_t *notif = &g_notifications[g_notification_count];
    notif->id = g_next_id++;
    notif->type = type;
    notif->priority = priority;
    notif->source = source;
    strncpy(notif->message, message, sizeof(notif->message) - 1);
    notif->message[sizeof(notif->message) - 1] = '\0';
    notif->timestamp = (uint32_t)time(NULL);
    notif->acknowledged = false;

    g_notification_count++;

    uint32_t id = notif->id;

    xSemaphoreGive(g_mutex);

    // Вызываем callback если зарегистрирован
    if (g_callback != NULL) {
        g_callback(notif);
    }

    const char *type_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    ESP_LOGI(TAG, "Created notification [%s]: %s", 
             type_str[type], message);

    return id;
}

esp_err_t notification_acknowledge(uint32_t notification_id)
{
    if (g_notifications == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (uint32_t i = 0; i < g_notification_count; i++) {
        if (g_notifications[i].id == notification_id) {
            g_notifications[i].acknowledged = true;
            xSemaphoreGive(g_mutex);
            ESP_LOGI(TAG, "Acknowledged notification %lu", (unsigned long)notification_id);
            return ESP_OK;
        }
    }

    xSemaphoreGive(g_mutex);
    ESP_LOGW(TAG, "Notification %lu not found", (unsigned long)notification_id);
    return ESP_ERR_NOT_FOUND;
}

uint32_t notification_get_unread_count(void)
{
    if (g_notifications == NULL) {
        return 0;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return 0;
    }

    uint32_t unread = 0;
    for (uint32_t i = 0; i < g_notification_count; i++) {
        if (!g_notifications[i].acknowledged) {
            unread++;
        }
    }

    xSemaphoreGive(g_mutex);
    return unread;
}

esp_err_t notification_register_callback(notification_callback_t callback)
{
    g_callback = callback;
    ESP_LOGI(TAG, "Callback registered");
    return ESP_OK;
}

esp_err_t notification_clear_all(void)
{
    if (g_notifications == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_notification_count = 0;
    memset(g_notifications, 0, g_max_notifications * sizeof(notification_t));

    xSemaphoreGive(g_mutex);
    ESP_LOGI(TAG, "All notifications cleared");
    return ESP_OK;
}

// Упрощенная версия создания системного уведомления
uint32_t notification_system(notification_type_t type, const char *message, notification_source_t source)
{
    return notification_create(type, NOTIF_PRIORITY_NORMAL, source, message);
}

// Обработка уведомлений (заглушка)
esp_err_t notification_process(void)
{
    // В данной реализации обработка не требуется
    return ESP_OK;
}

// Проверка наличия критических уведомлений
bool notification_has_critical(void)
{
    if (g_notifications == NULL) {
        return false;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    bool has_critical = false;
    for (uint32_t i = 0; i < g_notification_count; i++) {
        if (g_notifications[i].type == NOTIF_TYPE_CRITICAL && !g_notifications[i].acknowledged) {
            has_critical = true;
            break;
        }
    }

    xSemaphoreGive(g_mutex);
    return has_critical;
}

// Создание предупреждения о датчике
uint32_t notification_sensor_warning(notification_source_t source, float current_value, float alarm_low, float alarm_high)
{
    char message[128];
    if (current_value < alarm_low) {
        snprintf(message, sizeof(message), "Low value: %.2f (min: %.2f)", current_value, alarm_low);
        return notification_create(NOTIF_TYPE_WARNING, NOTIF_PRIORITY_HIGH, source, message);
    } else if (current_value > alarm_high) {
        snprintf(message, sizeof(message), "High value: %.2f (max: %.2f)", current_value, alarm_high);
        return notification_create(NOTIF_TYPE_WARNING, NOTIF_PRIORITY_HIGH, source, message);
    }
    return 0;
}

// Установка callback для уведомлений
esp_err_t notification_set_callback(notification_callback_t callback)
{
    return notification_register_callback(callback);
}

// Преобразование типа уведомления в строку
const char* notification_type_to_string(notification_type_t type)
{
    switch (type) {
        case NOTIF_TYPE_INFO:
            return "INFO";
        case NOTIF_TYPE_WARNING:
            return "WARNING";
        case NOTIF_TYPE_ERROR:
            return "ERROR";
        case NOTIF_TYPE_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

