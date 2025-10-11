#include "notification_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "data_logger.h"
#include "config_manager.h"
#include "nvs.h"
#include <string.h>
#include <time.h>

static const char *TAG = "NOTIF_SYS";

#define NOTIF_NVS_NAMESPACE    "notif_sys"
#define NOTIF_NVS_KEY_COUNT    "crit_count"
#define NOTIF_NVS_KEY_NOTIFS   "crit_notifs"

// Глобальные переменные
static notification_t *g_notifications = NULL;
static uint32_t g_max_notifications = 0;
static uint32_t g_notification_count = 0;
static uint32_t g_next_id = 1;
static SemaphoreHandle_t g_mutex = NULL;
static notification_callback_t g_callback = NULL;

// Защита от спама уведомлений (debounce)
#define NOTIF_DEBOUNCE_MS 30000  // Не создавать одинаковые уведомления чаще чем раз в 30 секунд
static char g_last_message[128] = {0};
static int64_t g_last_message_time = 0;

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

esp_err_t notification_system_deinit(void)
{
    if (g_notifications == NULL) {
        ESP_LOGW(TAG, "Notification system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Удаляем mutex
    if (g_mutex != NULL) {
        vSemaphoreDelete(g_mutex);
        g_mutex = NULL;
    }

    // Освобождаем память
    free(g_notifications);
    g_notifications = NULL;
    
    g_max_notifications = 0;
    g_notification_count = 0;
    g_next_id = 1;
    g_callback = NULL;

    ESP_LOGI(TAG, "Notification system deinitialized");
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
    
    // ЗАЩИТА ОТ СПАМА: Проверяем не создавали ли мы такое же уведомление недавно
    int64_t now = esp_timer_get_time() / 1000;
    if (g_last_message_time > 0 && (now - g_last_message_time) < NOTIF_DEBOUNCE_MS) {
        if (strcmp(g_last_message, message) == 0) {
            int64_t remaining = (NOTIF_DEBOUNCE_MS - (now - g_last_message_time)) / 1000;
            ESP_LOGI(TAG, "[GUARD] Duplicate notification suppressed (cooldown: %lld sec)", remaining);
            return 0; // Пропускаем дубликат - защита от спама!
        }
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Mutex timeout, notification may be delayed");
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
    
    // Копируем уведомление для callback (чтобы не держать mutex)
    notification_t notif_copy = *notif;
    
    // Обновляем debounce кэш
    strncpy(g_last_message, message, sizeof(g_last_message) - 1);
    g_last_message[sizeof(g_last_message) - 1] = '\0';
    g_last_message_time = esp_timer_get_time() / 1000;

    xSemaphoreGive(g_mutex);

    // Вызываем callback если зарегистрирован (вне mutex)
    if (g_callback != NULL) {
        g_callback(&notif_copy);
    }

    const char *type_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    ESP_LOGI(TAG, "Created notification [%s]: %s", 
             type_str[type], message);
    
    // Автологирование критических уведомлений в Data Logger
    const system_config_t *config = config_manager_get_cached();
    if (config && config->notification_config.auto_log_critical) {
        // Логируем WARNING, ERROR и CRITICAL уведомления
        if (type >= NOTIF_TYPE_WARNING) {
            log_level_t log_level;
            switch (type) {
                case NOTIF_TYPE_WARNING:
                    log_level = LOG_LEVEL_WARNING;
                    break;
                case NOTIF_TYPE_ERROR:
                    log_level = LOG_LEVEL_ERROR;
                    break;
                case NOTIF_TYPE_CRITICAL:
                    log_level = LOG_LEVEL_ERROR; // Критические тоже как ERROR
                    break;
                default:
                    log_level = LOG_LEVEL_INFO;
                    break;
            }
            
            char log_msg[150];
            snprintf(log_msg, sizeof(log_msg), "[%s] %s", type_str[type], message);
            esp_err_t log_err = data_logger_log_alarm(log_level, log_msg);
            if (log_err == ESP_OK) {
                ESP_LOGD(TAG, "Auto-logged alarm to data logger");
            }
        }
    }
    
    // Автосохранение критических уведомлений в NVS
    if (config && config->notification_config.save_critical_to_nvs && type == NOTIF_TYPE_CRITICAL) {
        esp_err_t nvs_err = notification_save_critical_to_nvs();
        if (nvs_err == ESP_OK) {
            ESP_LOGD(TAG, "Auto-saved critical notification to NVS");
        } else {
            ESP_LOGW(TAG, "Failed to auto-save critical to NVS: %s", esp_err_to_name(nvs_err));
        }
    }

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

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGD(TAG, "Mutex timeout in get_unread_count");
        return 0;
    }

    uint32_t unread = 0;
    uint32_t count = g_notification_count; // Копируем локально
    
    for (uint32_t i = 0; i < count; i++) {
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

// Сохранение критических уведомлений в NVS
esp_err_t notification_save_critical_to_nvs(void)
{
    if (g_notifications == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Подсчитываем количество критических уведомлений
    uint32_t critical_count = 0;
    for (uint32_t i = 0; i < g_notification_count; i++) {
        if (g_notifications[i].type == NOTIF_TYPE_CRITICAL && !g_notifications[i].acknowledged) {
            critical_count++;
        }
    }
    
    if (critical_count == 0) {
        ESP_LOGI(TAG, "No critical notifications to save");
        xSemaphoreGive(g_mutex);
        return ESP_OK;
    }
    
    // Выделяем буфер для критических уведомлений
    notification_t *critical_notifs = malloc(critical_count * sizeof(notification_t));
    if (critical_notifs == NULL) {
        xSemaphoreGive(g_mutex);
        ESP_LOGE(TAG, "Failed to allocate buffer for critical notifications");
        return ESP_ERR_NO_MEM;
    }
    
    // Копируем критические уведомления
    uint32_t idx = 0;
    for (uint32_t i = 0; i < g_notification_count && idx < critical_count; i++) {
        if (g_notifications[i].type == NOTIF_TYPE_CRITICAL && !g_notifications[i].acknowledged) {
            critical_notifs[idx++] = g_notifications[i];
        }
    }
    
    xSemaphoreGive(g_mutex);
    
    // Открываем NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NOTIF_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        free(critical_notifs);
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }
    
    // Сохраняем количество
    err = nvs_set_u32(nvs_handle, NOTIF_NVS_KEY_COUNT, critical_count);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        free(critical_notifs);
        ESP_LOGE(TAG, "Failed to save critical count: %s", esp_err_to_name(err));
        return err;
    }
    
    // Сохраняем уведомления
    err = nvs_set_blob(nvs_handle, NOTIF_NVS_KEY_NOTIFS, critical_notifs, 
                       critical_count * sizeof(notification_t));
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        free(critical_notifs);
        ESP_LOGE(TAG, "Failed to save critical notifications: %s", esp_err_to_name(err));
        return err;
    }
    
    // Commit
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    free(critical_notifs);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved %lu critical notifications to NVS", (unsigned long)critical_count);
    } else {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }
    
    return err;
}

// Загрузка критических уведомлений из NVS
esp_err_t notification_load_critical_from_nvs(void)
{
    if (g_notifications == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Открываем NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NOTIF_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No critical notifications in NVS");
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }
    
    // Читаем количество
    uint32_t critical_count = 0;
    err = nvs_get_u32(nvs_handle, NOTIF_NVS_KEY_COUNT, &critical_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "No critical notifications count in NVS");
        return ESP_OK;
    } else if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to read critical count: %s", esp_err_to_name(err));
        return err;
    }
    
    if (critical_count == 0) {
        nvs_close(nvs_handle);
        return ESP_OK;
    }
    
    // Выделяем буфер
    notification_t *critical_notifs = malloc(critical_count * sizeof(notification_t));
    if (critical_notifs == NULL) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to allocate buffer for loading critical notifications");
        return ESP_ERR_NO_MEM;
    }
    
    // Читаем уведомления
    size_t blob_size = critical_count * sizeof(notification_t);
    err = nvs_get_blob(nvs_handle, NOTIF_NVS_KEY_NOTIFS, critical_notifs, &blob_size);
    nvs_close(nvs_handle);
    
    if (err != ESP_OK) {
        free(critical_notifs);
        ESP_LOGE(TAG, "Failed to read critical notifications: %s", esp_err_to_name(err));
        return err;
    }
    
    // Восстанавливаем уведомления
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(critical_notifs);
        return ESP_ERR_TIMEOUT;
    }
    
    uint32_t restored = 0;
    for (uint32_t i = 0; i < critical_count; i++) {
        if (g_notification_count >= g_max_notifications) {
            ESP_LOGW(TAG, "Notification buffer full, cannot restore more");
            break;
        }
        
        g_notifications[g_notification_count++] = critical_notifs[i];
        restored++;
    }
    
    xSemaphoreGive(g_mutex);
    free(critical_notifs);
    
    ESP_LOGI(TAG, "Restored %lu critical notifications from NVS", (unsigned long)restored);
    return ESP_OK;
}

