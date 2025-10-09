/**
 * @file back_button.c
 * @brief Реализация виджета кнопки "Назад"
 */

#include "back_button.h"
#include "screen_manager/screen_manager.h"
#include "esp_log.h"

// Внешние стили из lvgl_ui.c
extern lv_style_t style_card;

static const char *TAG = "WIDGET_BACK_BTN";

/**
 * @brief Дефолтный callback для кнопки назад - использует Screen Manager
 */
static void default_back_callback(lv_event_t *e)
{
    ESP_LOGI(TAG, "Back button pressed - navigating back");
    
    // Используем Screen Manager для возврата
    esp_err_t ret = screen_go_back();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to go back: %s", esp_err_to_name(ret));
    }
}

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
    
    // БАГ ИСПРАВЛЕН: Если callback не передан, используем дефолтный!
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, user_data);
        ESP_LOGD(TAG, "Back button created with custom callback");
    } else {
        lv_obj_add_event_cb(btn, default_back_callback, LV_EVENT_CLICKED, NULL);
        ESP_LOGD(TAG, "Back button created with default callback (screen_go_back)");
    }
    
    // Создаем лейбл со стрелкой
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_LEFT);  // ←
    lv_obj_center(label);
    
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

