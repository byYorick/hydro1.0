/**
 * @file error_handler.c
 * @brief Реализация централизованной системы обработки ошибок
 */

#include "error_handler.h"
#include "notification_system.h"
#include "../lvgl_ui/screens/popup_screen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>

static const char *TAG = "ERROR_HANDLER";

// Глобальные переменные
static bool g_initialized = false;
static bool g_show_popup = true;
static SemaphoreHandle_t g_mutex = NULL;
static error_callback_t g_callback = NULL;
static QueueHandle_t g_error_queue = NULL;

// Статистика ошибок
static struct {
    uint32_t total;
    uint32_t critical;
    uint32_t errors;
    uint32_t warnings;
    uint32_t info;
    uint32_t debug;
} g_stats = {0};

/**
 * @brief Показ попапа ошибки через Screen Manager
 */
static void show_error_popup_via_screen_manager(const error_info_t *error)
{
    if (!g_show_popup || !error) {
        return;
    }

    // Проверяем, что мы в правильном контексте для работы с LVGL
    if (!lv_is_initialized()) {
        ESP_LOGW(TAG, "LVGL not initialized, skipping popup display");
        return;
    }
    
    // Определяем таймаут в зависимости от уровня
    uint32_t timeout = 3000; // 3 секунды по умолчанию
    if (error->level >= ERROR_LEVEL_ERROR) {
        timeout = 5000; // 5 секунд для ошибок
    }
    if (error->level == ERROR_LEVEL_CRITICAL) {
        timeout = 10000; // 10 секунд для критических
    }

    // Проверяем, что мы в главной задаче или задаче с доступом к LVGL
    const char *current_task = pcTaskGetName(NULL);
    if (strstr(current_task, "sensor") != NULL || 
        strstr(current_task, "i2c") != NULL ||
        strstr(current_task, "system") != NULL) {
        ESP_LOGW(TAG, "Error popup called from %s task - deferring to LVGL task", current_task);
        
        // Добавляем в очередь для отложенного показа
        error_queue_item_t queue_item = {
            .error = *error,
            .timeout = timeout
        };
        
        if (xQueueSend(g_error_queue, &queue_item, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to queue error popup - queue full");
        } else {
            ESP_LOGD(TAG, "Error popup queued for LVGL task");
        }
        return;
    }
    
    ESP_LOGI(TAG, "[INFO] Showing error popup via Screen Manager: [%s] %s", 
             error_level_to_string(error->level), error->message);
    
    // Показываем через popup_screen (управляется Screen Manager)
    popup_show_error(error, timeout);
}

esp_err_t error_handler_init(bool show_popup)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Error handler already initialized");
        return ESP_OK;
    }

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Создаем очередь для отложенного показа ошибок
    g_error_queue = xQueueCreate(10, sizeof(error_queue_item_t));
    if (g_error_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create error queue");
        vSemaphoreDelete(g_mutex);
        return ESP_ERR_NO_MEM;
    }

    g_show_popup = show_popup;
    g_initialized = true;

    ESP_LOGI(TAG, "Error handler initialized (popup: %s via Screen Manager)", 
             show_popup ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t error_handler_report(error_category_t category, 
                               error_level_t level,
                               esp_err_t code,
                               const char *component,
                               const char *format, ...)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "Error handler not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (component == NULL || format == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Форматирование сообщения
    char message[128];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Обновить статистику
    g_stats.total++;
    switch (level) {
        case ERROR_LEVEL_DEBUG:
            g_stats.debug++;
            break;
        case ERROR_LEVEL_INFO:
            g_stats.info++;
            break;
        case ERROR_LEVEL_WARNING:
            g_stats.warnings++;
            break;
        case ERROR_LEVEL_ERROR:
            g_stats.errors++;
            break;
        case ERROR_LEVEL_CRITICAL:
            g_stats.critical++;
            break;
    }

    xSemaphoreGive(g_mutex);

    // Создать структуру ошибки
    error_info_t error = {
        .category = category,
        .level = level,
        .code = code,
        .timestamp = (uint32_t)time(NULL)
    };
    strncpy(error.message, message, sizeof(error.message) - 1);
    strncpy(error.component, component, sizeof(error.component) - 1);

    // Логирование в ESP_LOG
    switch (level) {
        case ERROR_LEVEL_DEBUG:
            ESP_LOGD(component, "[%s] %s (code: %d)", 
                    error_category_to_string(category), message, code);
            break;
        case ERROR_LEVEL_INFO:
            ESP_LOGI(component, "[%s] %s (code: %d)", 
                    error_category_to_string(category), message, code);
            break;
        case ERROR_LEVEL_WARNING:
            ESP_LOGW(component, "[%s] %s (code: %d)", 
                    error_category_to_string(category), message, code);
            break;
        case ERROR_LEVEL_ERROR:
            ESP_LOGE(component, "[%s] %s (code: %d)", 
                    error_category_to_string(category), message, code);
            break;
        case ERROR_LEVEL_CRITICAL:
            ESP_LOGE(component, "[CRITICAL] [%s] %s (code: %d)", 
                    error_category_to_string(category), message, code);
            break;
    }

    // Отправить в систему уведомлений
    notification_type_t notif_type = NOTIF_TYPE_INFO;
    notification_priority_t notif_priority = NOTIF_PRIORITY_NORMAL;
    notification_source_t notif_source = NOTIF_SOURCE_SYSTEM;

    switch (level) {
        case ERROR_LEVEL_WARNING:
            notif_type = NOTIF_TYPE_WARNING;
            notif_priority = NOTIF_PRIORITY_NORMAL;
            break;
        case ERROR_LEVEL_ERROR:
            notif_type = NOTIF_TYPE_ERROR;
            notif_priority = NOTIF_PRIORITY_HIGH;
            break;
        case ERROR_LEVEL_CRITICAL:
            notif_type = NOTIF_TYPE_CRITICAL;
            notif_priority = NOTIF_PRIORITY_URGENT;
            break;
        default:
            break;
    }

    // Определить источник по категории
    switch (category) {
        case ERROR_CATEGORY_SENSOR:
            notif_source = NOTIF_SOURCE_SENSOR;
            break;
        case ERROR_CATEGORY_PUMP:
            notif_source = NOTIF_SOURCE_PUMP;
            break;
        case ERROR_CATEGORY_RELAY:
            notif_source = NOTIF_SOURCE_RELAY;
            break;
        default:
            notif_source = NOTIF_SOURCE_SYSTEM;
            break;
    }

    // Создать уведомление (только для warnings и выше)
    if (level >= ERROR_LEVEL_WARNING) {
        char notif_message[160];
        snprintf(notif_message, sizeof(notif_message), "%s: %.90s", component, message);
        notification_create(notif_type, notif_priority, notif_source, notif_message);
    }

    // Показать попап через Screen Manager (только для errors и выше)
    if (level >= ERROR_LEVEL_ERROR) {
        show_error_popup_via_screen_manager(&error);
    }

    // Вызвать callback если зарегистрирован
    if (g_callback != NULL) {
        g_callback(&error);
    }

    return ESP_OK;
}

esp_err_t error_handler_register_callback(error_callback_t callback)
{
    g_callback = callback;
    ESP_LOGI(TAG, "Callback registered");
    return ESP_OK;
}

esp_err_t error_handler_set_popup(bool enable)
{
    g_show_popup = enable;
    ESP_LOGI(TAG, "Error popup %s (via Screen Manager)", enable ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t error_handler_get_stats(uint32_t *total, uint32_t *critical, 
                                  uint32_t *errors, uint32_t *warnings)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (total) *total = g_stats.total;
    if (critical) *critical = g_stats.critical;
    if (errors) *errors = g_stats.errors;
    if (warnings) *warnings = g_stats.warnings;

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t error_handler_clear_stats(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memset(&g_stats, 0, sizeof(g_stats));

    xSemaphoreGive(g_mutex);
    ESP_LOGI(TAG, "Statistics cleared");
    return ESP_OK;
}

const char* error_category_to_string(error_category_t category)
{
    switch (category) {
        case ERROR_CATEGORY_I2C:
            return "I2C";
        case ERROR_CATEGORY_SENSOR:
            return "SENSOR";
        case ERROR_CATEGORY_DISPLAY:
            return "DISPLAY";
        case ERROR_CATEGORY_STORAGE:
            return "STORAGE";
        case ERROR_CATEGORY_SYSTEM:
            return "SYSTEM";
        case ERROR_CATEGORY_PUMP:
            return "PUMP";
        case ERROR_CATEGORY_RELAY:
            return "RELAY";
        case ERROR_CATEGORY_CONTROLLER:
            return "CONTROLLER";
        case ERROR_CATEGORY_NETWORK:
            return "NETWORK";
        case ERROR_CATEGORY_OTHER:
            return "OTHER";
        default:
            return "UNKNOWN";
    }
}

const char* error_level_to_string(error_level_t level)
{
    switch (level) {
        case ERROR_LEVEL_DEBUG:
            return "DEBUG";
        case ERROR_LEVEL_INFO:
            return "INFO";
        case ERROR_LEVEL_WARNING:
            return "ВНИМАНИЕ";
        case ERROR_LEVEL_ERROR:
            return "ОШИБКА";
        case ERROR_LEVEL_CRITICAL:
            return "КРИТИЧЕСКАЯ ОШИБКА";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Обработка очереди ошибок (вызывается из LVGL задачи)
 */
esp_err_t error_handler_process_queue(void)
{
    if (!g_initialized || !g_error_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    
    error_queue_item_t queue_item;
    BaseType_t result = xQueueReceive(g_error_queue, &queue_item, 0); // Неблокирующий вызов
    
    if (result == pdTRUE) {
        ESP_LOGI(TAG, "Processing queued error popup: [%s] %s", 
                 error_level_to_string(queue_item.error.level), queue_item.error.message);
        
        // Показываем popup в контексте LVGL задачи
        popup_show_error(&queue_item.error, queue_item.timeout);
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND; // Нет элементов в очереди
}

esp_err_t error_handler_set_font(const void *font)
{
    if (font == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Шрифт управляется popup_screen.c теперь
    ESP_LOGI(TAG, "Custom font API deprecated - popups managed by Screen Manager");
    return ESP_OK;
}

