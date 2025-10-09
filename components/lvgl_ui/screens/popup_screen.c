#include "popup_screen.h"
#include "screen_manager/screen_manager.h"
#include "esp_log.h"
#include <string.h>

// Объявление русского шрифта
LV_FONT_DECLARE(montserrat_ru);

static const char *TAG = "POPUP_SCREEN";

// Константы
#define POPUP_WIDTH  240
#define POPUP_HEIGHT 160

// UI элементы
typedef struct {
    lv_obj_t *container;
    lv_obj_t *icon_label;
    lv_obj_t *msg_label;
    lv_obj_t *ok_button;
    lv_timer_t *close_timer;
    popup_config_t config;
} popup_ui_t;

// Текущий попап
static popup_ui_t *g_current_popup = NULL;
static lv_group_t *g_popup_group = NULL;

// Прототипы
static lv_obj_t* popup_create(void *user_data);
static esp_err_t popup_on_show(lv_obj_t *scr, void *user_data);
static esp_err_t popup_on_hide(lv_obj_t *scr);
static void ok_button_cb(lv_event_t *e);
static void close_timer_cb(lv_timer_t *timer);

// Вспомогательные функции
static lv_color_t get_popup_color(popup_type_t type, const popup_config_t *config);
static const char* get_popup_icon(popup_type_t type, const popup_config_t *config);
static void format_popup_message(char *buffer, size_t size, const popup_config_t *config);

/**
 * @brief Регистрация popup экрана в Screen Manager
 */
void popup_screen_register(void)
{
    screen_config_t config = {
        .id = "popup",
        .title = "Popup",
        .category = SCREEN_CATEGORY_INFO, // Информационный экран вместо диалога
        .can_go_back = true,
        .is_root = false,
        .lazy_load = false,
        .cache_on_hide = false,
        .destroy_on_hide = true,
        .has_status_bar = false,
        .has_back_button = false,
        .create_fn = popup_create,
        .on_show = popup_on_show,
        .on_hide = popup_on_hide,
        .user_data = NULL
    };
    
    screen_register(&config);
    ESP_LOGI(TAG, "Popup screen registered");
}

/**
 * @brief Показать попап с уведомлением
 */
void popup_show_notification(const notification_t *notification, uint32_t timeout_ms)
{
    if (!notification) {
        ESP_LOGW(TAG, "NULL notification");
        return;
    }
    
    ESP_LOGI(TAG, "Showing notification popup: [%d] %s", 
             notification->type, notification->message);
    
    // Создаем конфигурацию
    popup_config_t *config = heap_caps_malloc(sizeof(popup_config_t), MALLOC_CAP_8BIT);
    if (!config) {
        ESP_LOGE(TAG, "Failed to allocate popup config");
        return;
    }
    
    config->type = POPUP_TYPE_NOTIFICATION;
    memcpy(&config->data.notification, notification, sizeof(notification_t));
    config->timeout_ms = timeout_ms;
    config->has_ok_button = true;
    
    // Показываем через screen manager (params используется как user_data)
    screen_show("popup", config);
}

/**
 * @brief Показать попап с ошибкой
 */
void popup_show_error(const error_info_t *error, uint32_t timeout_ms)
{
    if (!error) {
        ESP_LOGW(TAG, "NULL error");
        return;
    }
    
    ESP_LOGI(TAG, "Showing error popup: [%d] %s: %s", 
             error->level, error->component, error->message);
    
    // Создаем конфигурацию
    popup_config_t *config = heap_caps_malloc(sizeof(popup_config_t), MALLOC_CAP_8BIT);
    if (!config) {
        ESP_LOGE(TAG, "Failed to allocate popup config");
        return;
    }
    
    config->type = POPUP_TYPE_ERROR;
    memcpy(&config->data.error, error, sizeof(error_info_t));
    config->timeout_ms = timeout_ms;
    config->has_ok_button = (error->level >= ERROR_LEVEL_ERROR); // OK только для ошибок
    
    // Показываем через screen manager (params используется как user_data)
    ESP_LOGI(TAG, "Calling screen_show('popup', config=%p)", config);
    esp_err_t ret = screen_show("popup", config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show popup screen: %s", esp_err_to_name(ret));
        free(config);
    } else {
        ESP_LOGI(TAG, "Popup screen show request sent successfully");
    }
}

/**
 * @brief Закрыть попап
 */
void popup_close(void)
{
    ESP_LOGI(TAG, "Closing popup");
    screen_go_back();
}

/**
 * @brief Создание UI попапа
 */
static lv_obj_t* popup_create(void *user_data)
{
    ESP_LOGI(TAG, "Creating popup screen (user_data=%p)", user_data);
    
    // ИСПРАВЛЕНО: Непрозрачный фон на весь экран (overlay)
    lv_obj_t *bg = lv_obj_create(NULL);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x0F1419), 0);  // Темный фон как у главного экрана
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0); // Полная непрозрачность - избегаем искажений
    
    // Контейнер попапа (по центру)
    lv_obj_t *container = lv_obj_create(bg);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, POPUP_WIDTH, POPUP_HEIGHT);
    lv_obj_center(container);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(container, 10, 0);
    lv_obj_set_style_pad_all(container, 12, 0);
    lv_obj_set_style_border_width(container, 3, 0);
    lv_obj_set_style_border_color(container, lv_color_white(), 0);
    lv_obj_set_style_border_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(container, 20, 0);
    lv_obj_set_style_shadow_opa(container, LV_OPA_60, 0);
    
    // Flex layout
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container, 10, 0);
    
    // Иконка
    lv_obj_t *icon = lv_label_create(container);
    lv_obj_set_style_text_font(icon, &montserrat_ru, 0); // ИСПРАВЛЕНО: русский шрифт
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_label_set_text(icon, "!"); // Default icon
    
    // Текст сообщения
    lv_obj_t *msg = lv_label_create(container);
    lv_obj_set_style_text_font(msg, &montserrat_ru, 0); // ИСПРАВЛЕНО: русский шрифт
    lv_obj_set_style_text_color(msg, lv_color_white(), 0);
    lv_label_set_text(msg, "Message");
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, POPUP_WIDTH - 30);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    
    // Кнопка OK (создаём, но скроем если не нужна)
    lv_obj_t *ok_btn = lv_btn_create(container);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_set_style_bg_color(ok_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ok_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ok_btn, 8, 0);
    lv_obj_add_event_cb(ok_btn, ok_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "OK");
    lv_obj_set_style_text_color(ok_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(ok_label, &montserrat_ru, 0); // ИСПРАВЛЕНО: русский шрифт
    lv_obj_center(ok_label);
    
    // ИСПРАВЛЕНО: Добавляем обработчик нажатия энкодера
    lv_obj_add_flag(ok_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ok_btn, ok_button_cb, LV_EVENT_PRESSED, NULL); // Добавляем обработчик PRESSED
    lv_obj_add_event_cb(ok_btn, ok_button_cb, LV_EVENT_KEY, NULL); // Добавляем обработчик KEY (для энкодера)
    
    // Сохраняем указатели на элементы в user_data контейнера
    popup_ui_t *ui = heap_caps_malloc(sizeof(popup_ui_t), MALLOC_CAP_8BIT);
    if (ui) {
        ui->container = container;
        ui->icon_label = icon;
        ui->msg_label = msg;
        ui->ok_button = ok_btn;
        ui->close_timer = NULL;
        memset(&ui->config, 0, sizeof(popup_config_t));
        lv_obj_set_user_data(bg, ui);
        g_current_popup = ui;
    }
    
    ESP_LOGI(TAG, "Popup UI created: bg=%p, container=%p", bg, container);
    return bg;
}

/**
 * @brief Callback ON_SHOW - настройка контента попапа
 */
static esp_err_t popup_on_show(lv_obj_t *scr, void *user_data)
{
    popup_ui_t *ui = (popup_ui_t *)lv_obj_get_user_data(scr);
    if (!ui || !user_data) {
        ESP_LOGE(TAG, "Invalid popup data!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Копируем конфигурацию
    popup_config_t *config = (popup_config_t *)user_data;
    memcpy(&ui->config, config, sizeof(popup_config_t));
    free(config); // Освобождаем переданную конфигурацию
    
    ESP_LOGI(TAG, "Popup ON_SHOW: type=%d, timeout=%lu ms", 
             ui->config.type, ui->config.timeout_ms);
    
    // Установить цвет фона контейнера
    lv_color_t bg_color = get_popup_color(ui->config.type, &ui->config);
    lv_obj_set_style_bg_color(ui->container, bg_color, 0);
    
    // Установить иконку
    const char *icon = get_popup_icon(ui->config.type, &ui->config);
    lv_label_set_text(ui->icon_label, icon);
    
    // Установить сообщение
    char msg_buffer[256];
    format_popup_message(msg_buffer, sizeof(msg_buffer), &ui->config);
    lv_label_set_text(ui->msg_label, msg_buffer);
    
    // Показать/скрыть кнопку OK
    if (ui->config.has_ok_button) {
        lv_obj_clear_flag(ui->ok_button, LV_OBJ_FLAG_HIDDEN);
        
        // ИСПРАВЛЕНО: Используем Screen Manager encoder_group вместо создания новой
        screen_instance_t *current = screen_get_current();
        if (current && current->encoder_group) {
            lv_group_add_obj(current->encoder_group, ui->ok_button);
            lv_group_focus_obj(ui->ok_button);
            ESP_LOGI(TAG, "OK button added to popup encoder group");
        } else {
            ESP_LOGW(TAG, "No encoder group available in popup screen instance!");
        }
    } else {
        lv_obj_add_flag(ui->ok_button, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Установить таймер автоскрытия
    if (ui->config.timeout_ms > 0) {
        ui->close_timer = lv_timer_create(close_timer_cb, ui->config.timeout_ms, ui);
        lv_timer_set_repeat_count(ui->close_timer, 1);
        ESP_LOGI(TAG, "Auto-close timer set: %lu ms", ui->config.timeout_ms);
    }
    
    // Принудительная перерисовка
    lv_obj_invalidate(scr);
    
    ESP_LOGI(TAG, "[OK] Popup shown: pos(%d,%d), size(%dx%d)", 
             lv_obj_get_x(ui->container), lv_obj_get_y(ui->container),
             lv_obj_get_width(ui->container), lv_obj_get_height(ui->container));
    
    return ESP_OK;
}

/**
 * @brief Callback ON_HIDE
 */
static esp_err_t popup_on_hide(lv_obj_t *scr)
{
    popup_ui_t *ui = (popup_ui_t *)lv_obj_get_user_data(scr);
    if (!ui) return ESP_OK;
    
    ESP_LOGI(TAG, "Popup ON_HIDE");
    
    // Удалить таймер если есть
    if (ui->close_timer) {
        lv_timer_del(ui->close_timer);
        ui->close_timer = NULL;
    }
    
    // ИСПРАВЛЕНО: Не удаляем группу - она управляется Screen Manager
    // Группа автоматически очищается при уничтожении экрана
    g_popup_group = NULL;
    g_current_popup = NULL;
    
    // Освобождаем UI данные
    ESP_LOGI(TAG, "Popup ON_HIDE: freeing UI data");
    free(ui);
    lv_obj_set_user_data(scr, NULL);
    
    return ESP_OK;
}

/**
 * @brief Callback кнопки OK
 */
static void ok_button_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    ESP_LOGI(TAG, ">>> OK button callback triggered! Event code: %d", code);
    
    // ИСПРАВЛЕНО: Обрабатываем все типы событий
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "OK button CLICKED - closing popup");
        popup_close();
    } else if (code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "OK button PRESSED - closing popup");
        popup_close();
    } else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        ESP_LOGI(TAG, "OK button KEY event: key=%lu", key);
        if (key == LV_KEY_ENTER) {
            ESP_LOGI(TAG, "OK button ENTER key - closing popup");
            popup_close();
        }
    } else {
        ESP_LOGD(TAG, "OK button unhandled event: %d", code);
    }
}

/**
 * @brief Callback таймера автоскрытия
 */
static void close_timer_cb(lv_timer_t *timer)
{
    ESP_LOGI(TAG, "Auto-close timer triggered");
    popup_close();
}

/**
 * @brief Получить цвет попапа
 */
static lv_color_t get_popup_color(popup_type_t type, const popup_config_t *config)
{
    if (type == POPUP_TYPE_NOTIFICATION) {
        switch (config->data.notification.type) {
            case NOTIF_TYPE_INFO:    return lv_color_hex(0x4CAF50); // Зелёный
            case NOTIF_TYPE_WARNING: return lv_color_hex(0xFFA726); // Оранжевый
            case NOTIF_TYPE_ERROR:   return lv_color_hex(0xF44336); // Красный
            case NOTIF_TYPE_CRITICAL:return lv_color_hex(0xD32F2F); // Тёмно-красный
            default:                 return lv_color_hex(0x2196F3); // Синий
        }
    } else if (type == POPUP_TYPE_ERROR) {
        switch (config->data.error.level) {
            case ERROR_LEVEL_DEBUG:   return lv_color_hex(0x9E9E9E); // Серый
            case ERROR_LEVEL_INFO:    return lv_color_hex(0x4CAF50); // Зелёный
            case ERROR_LEVEL_WARNING: return lv_color_hex(0xFFA726); // Оранжевый
            case ERROR_LEVEL_ERROR:   return lv_color_hex(0xF44336); // Красный
            case ERROR_LEVEL_CRITICAL:return lv_color_hex(0xD32F2F); // Тёмно-красный
            default:                  return lv_color_hex(0x607D8B); // Серый
        }
    }
    return lv_color_hex(0x607D8B); // Серый по умолчанию
}

/**
 * @brief Получить иконку попапа
 */
static const char* get_popup_icon(popup_type_t type, const popup_config_t *config)
{
    if (type == POPUP_TYPE_NOTIFICATION) {
        switch (config->data.notification.type) {
            case NOTIF_TYPE_INFO:     return "i";
            case NOTIF_TYPE_WARNING:  return "!";
            case NOTIF_TYPE_ERROR:    return "X";
            case NOTIF_TYPE_CRITICAL: return "!!";
            default:                  return "*";
        }
    } else if (type == POPUP_TYPE_ERROR) {
        switch (config->data.error.level) {
            case ERROR_LEVEL_DEBUG:   return "D";
            case ERROR_LEVEL_INFO:    return "I";
            case ERROR_LEVEL_WARNING: return "W";
            case ERROR_LEVEL_ERROR:   return "E";
            case ERROR_LEVEL_CRITICAL:return "C";
            default:                  return "?";
        }
    }
    return "?";
}

/**
 * @brief Форматировать сообщение попапа
 */
static void format_popup_message(char *buffer, size_t size, const popup_config_t *config)
{
    if (config->type == POPUP_TYPE_NOTIFICATION) {
        snprintf(buffer, size, "%s", config->data.notification.message);
    } else if (config->type == POPUP_TYPE_ERROR) {
        // ИСПРАВЛЕНО: Используем русский шрифт - поддерживает кириллицу
        snprintf(buffer, size, "%s\n%s\nКод: %d", 
                 config->data.error.component,
                 config->data.error.message,
                 config->data.error.code);
    } else {
        snprintf(buffer, size, "Unknown");
    }
}

