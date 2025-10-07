/**
 * @file error_handler.c
 * @brief Реализация централизованной системы обработки ошибок
 */

#include "error_handler.h"
#include "notification_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>

static const char *TAG = "ERROR_HANDLER";

// Прототипы внутренних функций
static void show_error_popup_internal(const error_info_t *error);
static void queue_error_popup(const error_info_t *error);
static void popup_check_cb(lv_timer_t *timer);
static void popup_close_cb(lv_timer_t *timer);

// Глобальные переменные
static bool g_initialized = false;
static bool g_show_popup = true;
static SemaphoreHandle_t g_mutex = NULL;
static error_callback_t g_callback = NULL;
static QueueHandle_t g_popup_queue = NULL;
static const lv_font_t *g_popup_font = NULL; // Шрифт для всплывающих окон

// Статистика ошибок
static struct {
    uint32_t total;
    uint32_t critical;
    uint32_t errors;
    uint32_t warnings;
    uint32_t info;
    uint32_t debug;
} g_stats = {0};

// LVGL объекты для всплывающих окон
static lv_obj_t *g_popup = NULL;
static lv_obj_t *g_popup_label = NULL;
static lv_timer_t *g_popup_timer = NULL;
static lv_timer_t *g_popup_check_timer = NULL;

/**
 * @brief Callback для автоматического закрытия всплывающего окна
 */
static void popup_close_cb(lv_timer_t *timer)
{
    if (g_popup != NULL) {
        lv_obj_del(g_popup);
        g_popup = NULL;
        g_popup_label = NULL;
    }
    if (g_popup_timer != NULL) {
        lv_timer_del(g_popup_timer);
        g_popup_timer = NULL;
    }
}

/**
 * @brief Callback для проверки очереди и показа всплывающих окон (вызывается из LVGL таймера)
 */
static void popup_check_cb(lv_timer_t *timer)
{
    if (g_popup_queue == NULL) {
        return;
    }

    // Если уже показано окно, не показываем новое
    if (g_popup != NULL) {
        return;
    }

    // Проверяем очередь
    error_info_t error;
    if (xQueueReceive(g_popup_queue, &error, 0) == pdTRUE) {
        // Показываем всплывающее окно
        show_error_popup_internal(&error);
    }
}

/**
 * @brief Внутренняя функция для показа всплывающего окна (вызывается только из LVGL контекста)
 */
static void show_error_popup_internal(const error_info_t *error)
{
    if (!g_show_popup) {
        return;
    }

    // Закрыть предыдущее окно если есть
    if (g_popup != NULL) {
        lv_obj_del(g_popup);
        g_popup = NULL;
        g_popup_label = NULL;
    }
    
    if (g_popup_timer != NULL) {
        lv_timer_del(g_popup_timer);
        g_popup_timer = NULL;
    }

    // Создать контейнер для всплывающего окна
    g_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_popup, 280, 120);
    lv_obj_align(g_popup, LV_ALIGN_CENTER, 0, 0);
    
    // Установить цвет в зависимости от уровня ошибки
    lv_color_t bg_color = lv_color_hex(0x2196F3); // Синий по умолчанию
    switch (error->level) {
        case ERROR_LEVEL_WARNING:
            bg_color = lv_color_hex(0xFFA726); // Оранжевый
            break;
        case ERROR_LEVEL_ERROR:
            bg_color = lv_color_hex(0xF44336); // Красный
            break;
        case ERROR_LEVEL_CRITICAL:
            bg_color = lv_color_hex(0xD32F2F); // Темно-красный
            break;
        case ERROR_LEVEL_INFO:
            bg_color = lv_color_hex(0x4CAF50); // Зеленый
            break;
        default:
            break;
    }
    lv_obj_set_style_bg_color(g_popup, bg_color, 0);
    lv_obj_set_style_border_width(g_popup, 2, 0);
    lv_obj_set_style_border_color(g_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(g_popup, 20, 0);
    lv_obj_set_style_shadow_opa(g_popup, LV_OPA_50, 0);

    // Создать метку с текстом ошибки
    g_popup_label = lv_label_create(g_popup);
    lv_obj_set_width(g_popup_label, 260);
    lv_label_set_long_mode(g_popup_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(g_popup_label, lv_color_hex(0xFFFFFF), 0);
    
    // Использовать русский шрифт если установлен, иначе стандартный
    if (g_popup_font != NULL) {
        lv_obj_set_style_text_font(g_popup_label, g_popup_font, 0);
    } else {
        lv_obj_set_style_text_font(g_popup_label, &lv_font_montserrat_14, 0);
    }
    
    // Форматировать текст
    char popup_text[256];
    snprintf(popup_text, sizeof(popup_text), 
             "%s\n\n%s: %s\n\nКод: %d (%s)",
             error_level_to_string(error->level),
             error->component,
             error->message,
             error->code,
             esp_err_to_name(error->code));
    
    lv_label_set_text(g_popup_label, popup_text);
    lv_obj_center(g_popup_label);

    // Установить таймер для автоматического закрытия
    uint32_t timeout = 3000; // 3 секунды по умолчанию
    if (error->level >= ERROR_LEVEL_ERROR) {
        timeout = 5000; // 5 секунд для ошибок
    }
    if (error->level == ERROR_LEVEL_CRITICAL) {
        timeout = 10000; // 10 секунд для критических
    }
    
    g_popup_timer = lv_timer_create(popup_close_cb, timeout, NULL);
    lv_timer_set_repeat_count(g_popup_timer, 1);
}

/**
 * @brief Добавить ошибку в очередь для показа всплывающего окна
 */
static void queue_error_popup(const error_info_t *error)
{
    if (g_popup_queue == NULL || !g_show_popup) {
        return;
    }

    // Пытаемся добавить в очередь (не блокируем если очередь полна)
    xQueueSend(g_popup_queue, error, 0);
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

    // Создаем очередь для всплывающих окон (максимум 5 ошибок в очереди)
    g_popup_queue = xQueueCreate(5, sizeof(error_info_t));
    if (g_popup_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create popup queue");
        vSemaphoreDelete(g_mutex);
        return ESP_ERR_NO_MEM;
    }

    g_show_popup = show_popup;
    g_initialized = true;

    // Создаем LVGL таймер для проверки очереди (каждые 100 мс)
    if (show_popup) {
        g_popup_check_timer = lv_timer_create(popup_check_cb, 100, NULL);
        if (g_popup_check_timer == NULL) {
            ESP_LOGW(TAG, "Failed to create popup check timer");
        }
    }

    ESP_LOGI(TAG, "Error handler initialized (popup: %s)", show_popup ? "ON" : "OFF");
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

    // Добавить в очередь для показа всплывающего окна (только для errors и выше)
    if (level >= ERROR_LEVEL_ERROR) {
        queue_error_popup(&error);
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
    
    if (enable && g_popup_check_timer == NULL) {
        // Создаем таймер если еще не создан
        g_popup_check_timer = lv_timer_create(popup_check_cb, 100, NULL);
    } else if (!enable && g_popup_check_timer != NULL) {
        // Удаляем таймер если отключаем
        lv_timer_del(g_popup_check_timer);
        g_popup_check_timer = NULL;
    }
    
    ESP_LOGI(TAG, "Popup %s", enable ? "enabled" : "disabled");
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

esp_err_t error_handler_set_font(const void *font)
{
    if (font == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_popup_font = (const lv_font_t *)font;
    ESP_LOGI(TAG, "Custom font set for error popups");
    return ESP_OK;
}

