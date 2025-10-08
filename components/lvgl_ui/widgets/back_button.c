/**
 * @file back_button.c
 * @brief Реализация виджета кнопки "Назад"
 */

#include "back_button.h"
#include "esp_log.h"

// Внешние стили из lvgl_ui.c
extern lv_style_t style_card;

static const char *TAG = "WIDGET_BACK_BTN";

lv_obj_t* widget_create_back_button(lv_obj_t *parent, 
                                     lv_event_cb_t callback,
                                     void *user_data)
{
    if (!parent) {
        ESP_LOGE(TAG, "Parent is NULL");
        return NULL;
    }
    
    // Создаем кнопку
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_style(btn, &style_card, 0);
    lv_obj_set_size(btn, 60, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    // Добавляем callback если передан
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, user_data);
    }
    
    // Создаем лейбл со стрелкой
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_LEFT);  // ←
    lv_obj_center(label);
    
    ESP_LOGD(TAG, "Back button created");
    return btn;
}

void widget_back_button_add_to_group(lv_obj_t *btn, lv_group_t *group)
{
    if (!btn || !group) {
        ESP_LOGW(TAG, "btn or group is NULL");
        return;
    }
    
    lv_group_add_obj(group, btn);
    ESP_LOGD(TAG, "Back button added to group");
}

