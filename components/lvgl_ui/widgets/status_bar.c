/**
 * @file status_bar.c
 * @brief Реализация виджета статус-бара
 */

#include "status_bar.h"
#include "esp_log.h"
#include <stdio.h>

// Внешние стили из lvgl_ui.c  
extern lv_style_t style_card;
extern lv_style_t style_title;

static const char *TAG = "WIDGET_STATUS_BAR";

// Структура данных статус-бара
typedef struct {
    lv_obj_t *title_label;
    lv_obj_t *notif_icon;
    lv_obj_t *notif_badge;
    uint32_t notif_count;
} status_bar_data_t;

lv_obj_t* widget_create_status_bar(lv_obj_t *parent, const char *title)
{
    if (!parent) {
        ESP_LOGE(TAG, "Parent is NULL");
        return NULL;
    }
    
    // Создаем контейнер статус-бара (компактная высота)
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_add_style(bar, &style_card, 0);
    lv_obj_set_size(bar, LV_PCT(100), 30);  // Компактный хедер
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(bar, 4, 0);  // Меньше отступов
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Создаем структуру данных
    status_bar_data_t *data = lv_malloc(sizeof(status_bar_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate status bar data");
        return bar;
    }
    data->notif_count = 0;
    
    // Создаем лейбл заголовка (центр)
    lv_obj_t *label = lv_label_create(bar);
    lv_obj_add_style(label, &style_title, 0);
    
    if (title) {
        lv_label_set_text(label, title);
    } else {
        lv_label_set_text(label, "");
    }
    
    lv_obj_center(label);
    data->title_label = label;
    
    // Создаем иконку уведомлений (справа)
    lv_obj_t *notif_icon = lv_label_create(bar);
    lv_label_set_text(notif_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(notif_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(notif_icon, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_flag(notif_icon, LV_OBJ_FLAG_HIDDEN); // Скрываем по умолчанию
    data->notif_icon = notif_icon;
    
    // Создаем badge с количеством (рядом с иконкой)
    lv_obj_t *badge = lv_label_create(bar);
    lv_label_set_text(badge, "0");
    lv_obj_set_style_text_color(badge, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xF44336), 0); // Красный фон
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 8, 0);
    lv_obj_set_style_pad_hor(badge, 4, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_align_to(badge, notif_icon, LV_ALIGN_OUT_TOP_RIGHT, 2, -2);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN); // Скрываем по умолчанию
    data->notif_badge = badge;
    
    // Сохраняем структуру данных
    lv_obj_set_user_data(bar, data);
    
    ESP_LOGD(TAG, "Status bar created with title: '%s'", title ? title : "(empty)");
    return bar;
}

void widget_status_bar_set_title(lv_obj_t *status_bar, const char *title)
{
    if (!status_bar) {
        ESP_LOGW(TAG, "Status bar is NULL");
        return;
    }
    
    // Получаем данные из user data
    status_bar_data_t *data = (status_bar_data_t*)lv_obj_get_user_data(status_bar);
    if (data && data->title_label) {
        lv_label_set_text(data->title_label, title ? title : "");
        ESP_LOGD(TAG, "Status bar title updated to: '%s'", title ? title : "(empty)");
    } else {
        ESP_LOGW(TAG, "Status bar data or label not found");
    }
}

void widget_status_bar_update_notifications(lv_obj_t *status_bar, uint32_t count)
{
    if (!status_bar) {
        ESP_LOGW(TAG, "Status bar is NULL");
        return;
    }
    
    // Получаем данные из user data
    status_bar_data_t *data = (status_bar_data_t*)lv_obj_get_user_data(status_bar);
    if (!data) {
        ESP_LOGW(TAG, "Status bar data not found");
        return;
    }
    
    data->notif_count = count;
    
    // Показываем или скрываем индикатор в зависимости от количества
    if (count > 0) {
        // Показываем иконку и badge
        lv_obj_clear_flag(data->notif_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(data->notif_badge, LV_OBJ_FLAG_HIDDEN);
        
        // Обновляем текст badge (ограничиваем до 99+)
        char badge_text[8];
        if (count > 99) {
            snprintf(badge_text, sizeof(badge_text), "99+");
        } else {
            snprintf(badge_text, sizeof(badge_text), "%lu", (unsigned long)count);
        }
        lv_label_set_text(data->notif_badge, badge_text);
        
        ESP_LOGD(TAG, "Status bar notifications updated: %lu", (unsigned long)count);
    } else {
        // Скрываем индикатор если нет уведомлений
        lv_obj_add_flag(data->notif_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(data->notif_badge, LV_OBJ_FLAG_HIDDEN);
        
        ESP_LOGD(TAG, "Status bar notifications hidden (count=0)");
    }
}

