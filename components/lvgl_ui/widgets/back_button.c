/**
 * @file back_button.c
 * @brief Реализация виджета кнопки "Назад"
 */

#include "back_button.h"
#include "event_helpers.h"
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
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем и клик мышью и нажатие Enter от энкодера
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "Back button pressed - navigating back (event: %d)", code);
        
        // Используем Screen Manager для возврата
        esp_err_t ret = screen_go_back();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to go back: %s", esp_err_to_name(ret));
        }
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
    
    // Создаем компактную кнопку
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_style(btn, &style_card, 0);
    lv_obj_set_size(btn, 45, 26);  // Компактный размер
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -2, 2);  // Небольшие отступы от края
    lv_obj_set_style_pad_all(btn, 2, 0);  // Минимальные внутренние отступы
    
    // БАГ ИСПРАВЛЕН: Если callback не передан, используем дефолтный!
    // Обработка и клика мышью и нажатия энкодера
    if (callback) {
        widget_add_click_handler(btn, callback, user_data);
        ESP_LOGD(TAG, "Back button created with custom callback");
    } else {
        widget_add_click_handler(btn, default_back_callback, NULL);
        ESP_LOGD(TAG, "Back button created with default callback (screen_go_back)");
    }
    
    // Создаем лейбл со стрелкой
    lv_obj_t *label = lv_label_create(btn);
    // Используем встроенный шрифт LVGL для иконок (fallback)
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
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

