/**
 * @file notification_screen.c
 * @brief Реализация экрана уведомлений через Screen Manager
 */

#include "notification_screen.h"
#include "screen_manager/screen_manager.h"
#include "widgets/event_helpers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

// Объявление русского шрифта
LV_FONT_DECLARE(montserrat_ru);

static const char *TAG = "NOTIF_SCREEN";

// Константы UI
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160
#define ICON_SIZE     40

// Типы данных для передачи в экран
typedef enum {
    NOTIF_SCREEN_TYPE_NOTIFICATION,
    NOTIF_SCREEN_TYPE_ERROR
} notif_screen_type_t;

// Параметры экрана уведомления
typedef struct {
    notif_screen_type_t type;
    uint32_t timeout_ms;
    union {
        notification_t notification;
        error_info_t error;
    } data;
} notif_screen_params_t;

// UI элементы экрана
typedef struct {
    lv_obj_t *container;
    lv_obj_t *icon_label;
    lv_obj_t *msg_label;
    lv_obj_t *ok_button;
    lv_timer_t *close_timer;
    notif_screen_params_t params;
} notif_screen_ui_t;

// Глобальные переменные
static int64_t g_last_close_time = 0;
static uint32_t g_cooldown_ms = 30000; // 30 секунд по умолчанию
static notif_screen_ui_t *g_current_ui = NULL;

// Очередь для отложенного показа (ВСЯ КОММУНИКАЦИЯ ЧЕРЕЗ ОЧЕРЕДЬ)
#define NOTIF_QUEUE_SIZE 5
static QueueHandle_t g_notif_queue = NULL;

// Прототипы
static lv_obj_t* notif_screen_create(void *user_data);
static esp_err_t notif_screen_on_show(lv_obj_t *scr, void *user_data);
static esp_err_t notif_screen_on_hide(lv_obj_t *scr);
static void ok_button_cb(lv_event_t *e);
static void close_timer_cb(lv_timer_t *timer);
static void notif_screen_delete_cb(lv_event_t *e);  // КРИТИЧНО: Предотвращение утечки памяти

// Вспомогательные функции
static lv_color_t get_color(notif_screen_type_t type, const notif_screen_params_t *params);
static const char* get_icon(notif_screen_type_t type, const notif_screen_params_t *params);
static void format_message(char *buffer, size_t size, const notif_screen_params_t *params);

/**
 * @brief Регистрация экрана в Screen Manager
 */
void notification_screen_register(void)
{
    // Создаем очередь для отложенного показа
    g_notif_queue = xQueueCreate(NOTIF_QUEUE_SIZE, sizeof(notif_screen_params_t));
    if (!g_notif_queue) {
        ESP_LOGE(TAG, "Failed to create notification queue");
        return;
    }
    
    // Сбрасываем cooldown при инициализации
    g_last_close_time = 0;
    
    ESP_LOGI(TAG, "Notification queue created, cooldown reset");
    
    screen_config_t config = {
        .id = "notification",
        .title = "Notification",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "main",        // КРИТИЧНО: Возврат на главный экран
        .can_go_back = true,
        .is_root = false,
        .lazy_load = false,
        .cache_on_hide = false,
        .destroy_on_hide = true,  // Удаляем после закрытия
        .has_status_bar = false,
        .has_back_button = false,
        .create_fn = notif_screen_create,
        .on_show = notif_screen_on_show,
        .on_hide = notif_screen_on_hide,
        .user_data = NULL
    };
    
    screen_register(&config);
    ESP_LOGI(TAG, "Notification screen registered (queue size: %d)", NOTIF_QUEUE_SIZE);
}

/**
 * @brief Показать уведомление (ВСЕГДА через очередь)
 */
void notification_screen_show(const notification_t *notification, uint32_t timeout_ms)
{
    if (!notification) {
        ESP_LOGW(TAG, "NULL notification");
        return;
    }
    
    if (!g_notif_queue) {
        ESP_LOGE(TAG, ">>> Notification queue NOT INITIALIZED!");
        return;
    }
    
    // ВСЯ КОММУНИКАЦИЯ ЧЕРЕЗ ОЧЕРЕДЬ - независимо от контекста
    const char *current_task = pcTaskGetName(NULL);
    ESP_LOGI(TAG, ">>> Notification from '%s' task - queuing for LVGL task", current_task);
    
    // Добавляем в очередь для обработки LVGL задачей
    notif_screen_params_t queue_item;
    queue_item.type = NOTIF_SCREEN_TYPE_NOTIFICATION;
    queue_item.timeout_ms = timeout_ms;
    memcpy(&queue_item.data.notification, notification, sizeof(notification_t));
    
    if (xQueueSend(g_notif_queue, &queue_item, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, ">>> FAILED to queue notification - queue FULL!");
    } else {
        ESP_LOGI(TAG, ">>> Notification QUEUED: [%d] %s", notification->type, notification->message);
    }
}

/**
 * @brief Показать ошибку (ВСЕГДА через очередь)
 */
void error_screen_show(const error_info_t *error, uint32_t timeout_ms)
{
    if (!error) {
        ESP_LOGW(TAG, "NULL error");
        return;
    }
    
    if (!g_notif_queue) {
        ESP_LOGE(TAG, ">>> Notification queue NOT INITIALIZED!");
        return;
    }
    
    // ВСЯ КОММУНИКАЦИЯ ЧЕРЕЗ ОЧЕРЕДЬ - независимо от контекста
    const char *current_task = pcTaskGetName(NULL);
    ESP_LOGI(TAG, ">>> Error from '%s' task - queuing for LVGL task", current_task);
    
    // Добавляем в очередь для обработки LVGL задачей
    notif_screen_params_t queue_item;
    queue_item.type = NOTIF_SCREEN_TYPE_ERROR;
    queue_item.timeout_ms = timeout_ms;
    memcpy(&queue_item.data.error, error, sizeof(error_info_t));
    
    if (xQueueSend(g_notif_queue, &queue_item, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, ">>> FAILED to queue error - queue FULL!");
    } else {
        ESP_LOGI(TAG, ">>> Error QUEUED: [%d] %s: %s", 
                 error->level, error->component, error->message);
    }
}

/**
 * @brief Установить cooldown
 */
void notification_screen_set_cooldown(uint32_t cooldown_ms)
{
    g_cooldown_ms = cooldown_ms;
    ESP_LOGI(TAG, "Cooldown set to %lu ms", (unsigned long)cooldown_ms);
}

/**
 * @brief Создание UI экрана
 */
static lv_obj_t* notif_screen_create(void *user_data)
{
    ESP_LOGD(TAG, "Creating notification screen");
    
    // Полупрозрачный фон на весь экран
    lv_obj_t *bg = lv_obj_create(NULL);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);
    
    // Контейнер уведомления по центру
    lv_obj_t *container = lv_obj_create(bg);
    lv_obj_set_size(container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(container);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_shadow_width(container, 20, 0);
    lv_obj_set_style_shadow_opa(container, LV_OPA_30, 0);
    
    // Иконка
    lv_obj_t *icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 15);
    
    // Текст сообщения
    lv_obj_t *msg_label = lv_label_create(container);
    lv_obj_set_style_text_font(msg_label, &montserrat_ru, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg_label, SCREEN_WIDTH - 30);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, -20);  // Центрируем с отступом вверх
    
    // Кнопка OK - переносим на самый низ ЭКРАНА (bg, а не container)
    extern lv_style_t style_card_focused;
    lv_obj_t *ok_button = lv_btn_create(bg);  // Создаём на bg, а не на container!
    lv_obj_set_size(ok_button, 100, 40);
    lv_obj_align(ok_button, LV_ALIGN_BOTTOM_MID, 0, -15);  // Низ экрана
    lv_obj_add_style(ok_button, &style_card_focused, LV_STATE_FOCUSED);  // Стиль фокуса энкодера
    
    // ИСПРАВЛЕНО: Используем widget_add_click_handler для правильной обработки KEY_ENTER
    widget_add_click_handler(ok_button, ok_button_cb, NULL);
    
    lv_obj_t *ok_label = lv_label_create(ok_button);
    lv_label_set_text(ok_label, "OK");
    lv_obj_set_style_text_font(ok_label, &montserrat_ru, 0);
    lv_obj_center(ok_label);
    
    // Сохраняем UI элементы
    notif_screen_ui_t *ui = malloc(sizeof(notif_screen_ui_t));
    if (!ui) {
        ESP_LOGE(TAG, "Failed to allocate UI");
        return bg;
    }
    
    ui->container = container;
    ui->icon_label = icon_label;
    ui->msg_label = msg_label;
    ui->ok_button = ok_button;
    ui->close_timer = NULL;
    memset(&ui->params, 0, sizeof(notif_screen_params_t));
    
    lv_obj_set_user_data(bg, ui);
    g_current_ui = ui;
    
    // КРИТИЧНО: Добавляем обработчик удаления для предотвращения утечки памяти
    lv_obj_add_event_cb(bg, notif_screen_delete_cb, LV_EVENT_DELETE, NULL);
    
    ESP_LOGD(TAG, "Notification screen created");
    return bg;
}

/**
 * @brief Callback при удалении экрана (предотвращение утечки памяти)
 * 
 * ВАЖНО: Объявлен выше, реализация здесь
 */
static void notif_screen_delete_cb(lv_event_t *e) {
    lv_obj_t *scr = lv_event_get_target(e);
    notif_screen_ui_t *ui = (notif_screen_ui_t *)lv_obj_get_user_data(scr);
    
    if (ui) {
        ESP_LOGD(TAG, "Освобождение памяти notification screen");
        
        // Останавливаем таймер если есть
        if (ui->close_timer) {
            lv_timer_del(ui->close_timer);
            ui->close_timer = NULL;
        }
        
        free(ui);
        lv_obj_set_user_data(scr, NULL);
        
        if (g_current_ui == ui) {
            g_current_ui = NULL;
        }
    }
}

/**
 * @brief Callback при показе экрана
 */
static esp_err_t notif_screen_on_show(lv_obj_t *scr, void *user_data)
{
    if (!scr) {
        ESP_LOGE(TAG, "Invalid screen object for on_show");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!user_data) {
        ESP_LOGE(TAG, ">>> on_show called without params - THIS IS A BUG!");
        // Экран был показан без параметров (не должно происходить)
        // НЕ вызываем screen_go_back() - это создает рекурсию!
        // Просто возвращаем ошибку, и screen_lifecycle обработает
        return ESP_ERR_INVALID_ARG;
    }
    
    notif_screen_ui_t *ui = lv_obj_get_user_data(scr);
    if (!ui) {
        ESP_LOGE(TAG, "UI data not found");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Копируем параметры
    notif_screen_params_t *params = (notif_screen_params_t *)user_data;
    memcpy(&ui->params, params, sizeof(notif_screen_params_t));
    free(params); // Освобождаем параметры
    
    // Настраиваем цвет контейнера
    lv_color_t color = get_color(ui->params.type, &ui->params);
    lv_obj_set_style_bg_color(ui->container, color, 0);
    
    // Устанавливаем иконку
    const char *icon = get_icon(ui->params.type, &ui->params);
    lv_label_set_text(ui->icon_label, icon);
    
    // Форматируем и устанавливаем сообщение
    char msg_buffer[256];
    format_message(msg_buffer, sizeof(msg_buffer), &ui->params);
    lv_label_set_text(ui->msg_label, msg_buffer);
    
    // Добавляем кнопку в encoder group
    screen_instance_t *instance = screen_get_by_id("notification");
    if (instance && instance->encoder_group) {
        lv_group_add_obj(instance->encoder_group, ui->ok_button);
        lv_group_focus_obj(ui->ok_button);
        ESP_LOGD(TAG, "OK button added to encoder group");
    }
    
    // Запускаем таймер автозакрытия (если указан)
    if (ui->params.timeout_ms > 0) {
        ui->close_timer = lv_timer_create(close_timer_cb, ui->params.timeout_ms, NULL);
        ESP_LOGD(TAG, "Auto-close timer started: %lu ms", (unsigned long)ui->params.timeout_ms);
    }
    
    ESP_LOGD(TAG, "Notification screen shown");
    return ESP_OK;
}

/**
 * @brief Callback при скрытии экрана
 */
static esp_err_t notif_screen_on_hide(lv_obj_t *scr)
{
    if (!scr) {
        return ESP_ERR_INVALID_ARG;
    }
    
    notif_screen_ui_t *ui = lv_obj_get_user_data(scr);
    if (!ui) {
        return ESP_OK; // Уже очищено
    }
    
    ESP_LOGI(TAG, ">>> Notification screen hiding");
    
    // Удаляем таймер
    if (ui->close_timer) {
        lv_timer_del(ui->close_timer);
        ui->close_timer = NULL;
        ESP_LOGD(TAG, "Auto-close timer deleted");
    }
    
    // НЕ удаляем кнопку из encoder_group вручную!
    // Это делает screen_lifecycle.c автоматически при уничтожении группы
    // Ручное удаление может вызвать конфликты и потерю фокуса
    
    // Очищаем UI
    g_current_ui = NULL;
    free(ui);
    lv_obj_set_user_data(scr, NULL);
    
    ESP_LOGI(TAG, ">>> Notification screen hidden (encoder group managed by lifecycle)");
    return ESP_OK;
}

/**
 * @brief Callback кнопки OK
 */
static void ok_button_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // КРИТИЧНО: Обрабатываем ТОЛЬКО LV_EVENT_CLICKED для избежания дублирования
    // Encoder отправляет KEY, которое LVGL автоматически преобразует в CLICKED
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, ">>> OK button CLICKED - activating cooldown");
        
        // Сохраняем время закрытия для cooldown
        g_last_close_time = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, ">>> Cooldown started: %lld ms", g_last_close_time);
        
        // КРИТИЧНО: Возвращаемся на parent (main экран)
        screen_go_to_parent();
    }
    // Игнорируем PRESSED и KEY для избежания двойного срабатывания
}

/**
 * @brief Callback таймера автозакрытия
 */
static void close_timer_cb(lv_timer_t *timer)
{
    ESP_LOGI(TAG, ">>> Auto-close timer triggered (no cooldown)");
    
    // НЕ активируем cooldown при автозакрытии
    // Cooldown только при нажатии OK
    
    // КРИТИЧНО: Возвращаемся на parent (main экран)
    screen_go_to_parent();
}

/**
 * @brief Получить цвет для типа уведомления/ошибки
 */
static lv_color_t get_color(notif_screen_type_t type, const notif_screen_params_t *params)
{
    if (type == NOTIF_SCREEN_TYPE_NOTIFICATION) {
        switch (params->data.notification.type) {
            case NOTIF_TYPE_INFO:    return lv_color_hex(0x4CAF50); // Зелёный
            case NOTIF_TYPE_WARNING: return lv_color_hex(0xFF9800); // Оранжевый
            case NOTIF_TYPE_ERROR:   return lv_color_hex(0xF44336); // Красный
            case NOTIF_TYPE_CRITICAL: return lv_color_hex(0xB71C1C); // Тёмно-красный
            default: return lv_color_hex(0x2196F3); // Синий
        }
    } else { // NOTIF_SCREEN_TYPE_ERROR
        switch (params->data.error.level) {
            case ERROR_LEVEL_WARNING: return lv_color_hex(0xFF9800);
            case ERROR_LEVEL_ERROR:   return lv_color_hex(0xF44336);
            case ERROR_LEVEL_CRITICAL: return lv_color_hex(0xB71C1C);
            default: return lv_color_hex(0x2196F3);
        }
    }
}

/**
 * @brief Получить иконку для типа уведомления/ошибки
 */
static const char* get_icon(notif_screen_type_t type, const notif_screen_params_t *params)
{
    if (type == NOTIF_SCREEN_TYPE_NOTIFICATION) {
        switch (params->data.notification.type) {
            case NOTIF_TYPE_INFO:     return "OK";
            case NOTIF_TYPE_WARNING:  return "!";
            case NOTIF_TYPE_ERROR:    return "X";
            case NOTIF_TYPE_CRITICAL: return "X";
            default: return "!";
        }
    } else {
        switch (params->data.error.level) {
            case ERROR_LEVEL_WARNING:  return "!";
            case ERROR_LEVEL_ERROR:    return "X";
            case ERROR_LEVEL_CRITICAL: return "X";
            default: return "!";
        }
    }
}

/**
 * @brief Форматировать сообщение
 */
static void format_message(char *buffer, size_t size, const notif_screen_params_t *params)
{
    if (params->type == NOTIF_SCREEN_TYPE_NOTIFICATION) {
        snprintf(buffer, size, "%s", params->data.notification.message);
    } else {
        snprintf(buffer, size, "%s\n%s\nКод: %d", 
                 params->data.error.component,
                 params->data.error.message,
                 params->data.error.code);
    }
}

/**
 * @brief Обработка очереди уведомлений (вызывается из LVGL задачи)
 * 
 * @return ESP_OK если уведомление обработано, ESP_ERR_NOT_FOUND если очередь пуста
 */
esp_err_t notification_screen_process_queue(void)
{
    if (!g_notif_queue) {
        ESP_LOGE(TAG, "Notification queue not initialized!");
        return ESP_ERR_INVALID_STATE;
    }
    
    notif_screen_params_t queue_item;
    BaseType_t result = xQueueReceive(g_notif_queue, &queue_item, 0); // Неблокирующий
    
    if (result != pdTRUE) {
        // Очередь пуста - это нормально
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, ">>> Processing queued notification/error from LVGL task");
    
    // Обрабатываем в зависимости от типа
    if (queue_item.type == NOTIF_SCREEN_TYPE_NOTIFICATION) {
        // Проверка cooldown
        int64_t now = esp_timer_get_time() / 1000;
        int64_t time_since_close = now - g_last_close_time;
        
        if (g_last_close_time > 0 && time_since_close < g_cooldown_ms) {
            ESP_LOGW(TAG, "Cooldown active (%lld ms remaining), queued notification suppressed", 
                     g_cooldown_ms - time_since_close);
            return ESP_OK;
        }
        
        ESP_LOGI(TAG, "Showing queued notification: [%d] %s", 
                 queue_item.data.notification.type, queue_item.data.notification.message);
    } else {
        // Проверка cooldown (критичные ошибки игнорируют)
        int64_t now = esp_timer_get_time() / 1000;
        int64_t time_since_close = now - g_last_close_time;
        
        if (queue_item.data.error.level < ERROR_LEVEL_CRITICAL && 
            g_last_close_time > 0 && 
            time_since_close < g_cooldown_ms) {
            ESP_LOGW(TAG, "Cooldown active (%lld ms remaining), queued error suppressed", 
                     g_cooldown_ms - time_since_close);
            return ESP_OK;
        }
        
        // Критичные ошибки сбрасывают cooldown
        if (queue_item.data.error.level >= ERROR_LEVEL_CRITICAL) {
            ESP_LOGI(TAG, "Critical error - bypassing cooldown");
            g_last_close_time = 0;
        }
        
        ESP_LOGI(TAG, "Showing queued error: [%d] %s: %s", 
                 queue_item.data.error.level, 
                 queue_item.data.error.component, 
                 queue_item.data.error.message);
    }
    
    // Показываем экран
    notif_screen_params_t *params = malloc(sizeof(notif_screen_params_t));
    if (!params) {
        ESP_LOGE(TAG, "Failed to allocate params for queued item");
        return ESP_FAIL;
    }
    
    memcpy(params, &queue_item, sizeof(notif_screen_params_t));
    
    esp_err_t ret = screen_show("notification", params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show queued notification: %s", esp_err_to_name(ret));
        free(params);
        return ret;
    }
    
    return ESP_OK;
}

