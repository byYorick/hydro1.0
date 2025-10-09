/**
 * @file notification_popup.c
 * @brief Реализация виджета всплывающих уведомлений
 */

#include "notification_popup.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

// Внешние стили
extern lv_style_t style_card;
extern lv_style_t style_label;

// Цвета для разных типов уведомлений
#define COLOR_INFO      lv_color_hex(0x2196F3)  // Синий
#define COLOR_WARNING   lv_color_hex(0xFF9800)  // Оранжевый
#define COLOR_ERROR     lv_color_hex(0xF44336)  // Красный
#define COLOR_CRITICAL  lv_color_hex(0xB71C1C)  // Темно-красный
#define COLOR_TEXT      lv_color_hex(0xFFFFFF)  // Белый

static const char *TAG = "NOTIF_POPUP";

#define MAX_ACTIVE_POPUPS 3
#define POPUP_AUTO_HIDE_MS 5000  // Автоскрытие через 5 секунд

// Активные попапы
static lv_obj_t *active_popups[MAX_ACTIVE_POPUPS] = {NULL};
static lv_timer_t *popup_timers[MAX_ACTIVE_POPUPS] = {NULL};

/**
 * @brief Callback для автоскрытия попапа
 */
static void popup_hide_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *popup = (lv_obj_t*)lv_timer_get_user_data(timer);
    
    // Находим индекс попапа
    for (int i = 0; i < MAX_ACTIVE_POPUPS; i++) {
        if (active_popups[i] == popup) {
            // Удаляем таймер
            if (popup_timers[i]) {
                lv_timer_del(popup_timers[i]);
                popup_timers[i] = NULL;
            }
            
            // Удаляем попап
            if (active_popups[i]) {
                lv_obj_del_async(active_popups[i]);
                active_popups[i] = NULL;
            }
            
            ESP_LOGD(TAG, "Popup auto-hidden (slot %d)", i);
            break;
        }
    }
}

/**
 * @brief Callback при клике на попап (закрыть)
 */
static void popup_click_cb(lv_event_t *e)
{
    lv_obj_t *popup = lv_event_get_target(e);
    
    // Находим и удаляем
    for (int i = 0; i < MAX_ACTIVE_POPUPS; i++) {
        if (active_popups[i] == popup) {
            if (popup_timers[i]) {
                lv_timer_del(popup_timers[i]);
                popup_timers[i] = NULL;
            }
            lv_obj_del_async(active_popups[i]);
            active_popups[i] = NULL;
            ESP_LOGD(TAG, "Popup closed by click (slot %d)", i);
            break;
        }
    }
}

/**
 * @brief Получить цвет для типа уведомления
 */
static lv_color_t get_notification_color(notification_type_t type)
{
    switch (type) {
        case NOTIF_TYPE_INFO:     return COLOR_INFO;
        case NOTIF_TYPE_WARNING:  return COLOR_WARNING;
        case NOTIF_TYPE_ERROR:    return COLOR_ERROR;
        case NOTIF_TYPE_CRITICAL: return COLOR_CRITICAL;
        default:                  return COLOR_INFO;
    }
}

/**
 * @brief Получить иконку для типа уведомления
 */
static const char* get_notification_icon(notification_type_t type)
{
    switch (type) {
        case NOTIF_TYPE_INFO:     return LV_SYMBOL_OK;
        case NOTIF_TYPE_WARNING:  return LV_SYMBOL_WARNING;
        case NOTIF_TYPE_ERROR:    return LV_SYMBOL_CLOSE;
        case NOTIF_TYPE_CRITICAL: return LV_SYMBOL_CLOSE;
        default:                  return LV_SYMBOL_BELL;
    }
}

lv_obj_t* widget_show_notification_popup(const notification_t *notification)
{
    if (!notification) {
        ESP_LOGE(TAG, "Notification is NULL");
        return NULL;
    }
    
    // Ищем свободный слот или удаляем самый старый
    int slot = -1;
    for (int i = 0; i < MAX_ACTIVE_POPUPS; i++) {
        if (active_popups[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    // Если нет свободных, удаляем первый (самый старый)
    if (slot == -1) {
        slot = 0;
        if (popup_timers[0]) {
            lv_timer_del(popup_timers[0]);
            popup_timers[0] = NULL;
        }
        if (active_popups[0]) {
            lv_obj_del(active_popups[0]);
            active_popups[0] = NULL;
        }
        
        // Сдвигаем остальные
        for (int i = 0; i < MAX_ACTIVE_POPUPS - 1; i++) {
            active_popups[i] = active_popups[i + 1];
            popup_timers[i] = popup_timers[i + 1];
        }
        slot = MAX_ACTIVE_POPUPS - 1;
        active_popups[slot] = NULL;
        popup_timers[slot] = NULL;
    }
    
    // Создаем попап
    lv_obj_t *popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(popup);
    lv_obj_add_style(popup, &style_card, 0);
    
    // Размер и позиция (компактный для 240x320)
    lv_obj_set_size(popup, 220, 70);
    
    // Позиция: сверху с отступом, каждый следующий ниже
    int y_offset = 10 + (slot * 75);
    lv_obj_set_pos(popup, 10, y_offset);
    
    // Цвет фона по типу уведомления
    lv_color_t bg_color = get_notification_color(notification->type);
    lv_obj_set_style_bg_color(popup, bg_color, 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_90, 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_border_color(popup, lv_color_white(), 0);
    lv_obj_set_style_radius(popup, 8, 0);
    lv_obj_set_style_pad_all(popup, 12, 0);
    
    // Тень для выделения
    lv_obj_set_style_shadow_width(popup, 12, 0);
    lv_obj_set_style_shadow_opa(popup, LV_OPA_30, 0);
    
    // Flex layout
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(popup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(popup, 8, 0);
    
    // Иконка
    lv_obj_t *icon = lv_label_create(popup);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(icon, COLOR_TEXT, 0);
    lv_label_set_text(icon, get_notification_icon(notification->type));
    
    // Сообщение
    lv_obj_t *msg_label = lv_label_create(popup);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg_label, COLOR_TEXT, 0);
    lv_label_set_text(msg_label, notification->message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(msg_label, 170);
    
    // Делаем кликабельным для закрытия
    lv_obj_add_flag(popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(popup, popup_click_cb, LV_EVENT_CLICKED, NULL);
    
    // Появление с анимацией
    lv_obj_set_style_opa(popup, LV_OPA_TRANSP, 0);
    lv_obj_fade_in(popup, 300, 0);
    
    // Сохраняем в слот
    active_popups[slot] = popup;
    
    // Создаем таймер автоскрытия
    popup_timers[slot] = lv_timer_create(popup_hide_timer_cb, POPUP_AUTO_HIDE_MS, popup);
    
    ESP_LOGI(TAG, "Popup shown (slot %d): %s", slot, notification->message);
    
    return popup;
}

void widget_hide_all_popups(void)
{
    for (int i = 0; i < MAX_ACTIVE_POPUPS; i++) {
        if (popup_timers[i]) {
            lv_timer_del(popup_timers[i]);
            popup_timers[i] = NULL;
        }
        if (active_popups[i]) {
            lv_obj_del(active_popups[i]);
            active_popups[i] = NULL;
        }
    }
    ESP_LOGI(TAG, "All popups hidden");
}

/**
 * @brief Callback от notification_system - показывает попап автоматически
 */
static void notification_callback(const notification_t *notification)
{
    if (!notification) {
        return;
    }
    
    // Показываем только важные уведомления
    if (notification->priority >= NOTIF_PRIORITY_NORMAL) {
        widget_show_notification_popup(notification);
    }
}

void widget_notification_popup_init(void)
{
    // Регистрируем callback в notification_system
    esp_err_t ret = notification_register_callback(notification_callback);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Notification popup system initialized");
    } else {
        ESP_LOGW(TAG, "Failed to register notification callback: %s", 
                 esp_err_to_name(ret));
    }
}

