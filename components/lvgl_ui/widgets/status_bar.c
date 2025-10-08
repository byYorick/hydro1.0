/**
 * @file status_bar.c
 * @brief Реализация виджета статус-бара
 */

#include "status_bar.h"
#include "esp_log.h"

// Внешние стили из lvgl_ui.c  
extern lv_style_t style_card;
extern lv_style_t style_title;

static const char *TAG = "WIDGET_STATUS_BAR";

lv_obj_t* widget_create_status_bar(lv_obj_t *parent, const char *title)
{
    if (!parent) {
        ESP_LOGE(TAG, "Parent is NULL");
        return NULL;
    }
    
    // Создаем контейнер статус-бара
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_add_style(bar, &style_card, 0);
    lv_obj_set_size(bar, LV_PCT(100), 60);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Создаем лейбл заголовка
    lv_obj_t *label = lv_label_create(bar);
    lv_obj_add_style(label, &style_title, 0);
    
    if (title) {
        lv_label_set_text(label, title);
    } else {
        lv_label_set_text(label, "");
    }
    
    lv_obj_center(label);
    
    // Сохраняем ссылку на лейбл как user data для будущего обновления
    lv_obj_set_user_data(bar, label);
    
    ESP_LOGD(TAG, "Status bar created with title: '%s'", title ? title : "(empty)");
    return bar;
}

void widget_status_bar_set_title(lv_obj_t *status_bar, const char *title)
{
    if (!status_bar) {
        ESP_LOGW(TAG, "Status bar is NULL");
        return;
    }
    
    // Получаем лейбл из user data
    lv_obj_t *label = (lv_obj_t*)lv_obj_get_user_data(status_bar);
    if (label) {
        lv_label_set_text(label, title ? title : "");
        ESP_LOGD(TAG, "Status bar title updated to: '%s'", title ? title : "(empty)");
    } else {
        ESP_LOGW(TAG, "Label not found in status bar");
    }
}

