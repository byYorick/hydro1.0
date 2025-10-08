/**
 * @file menu_list.c
 * @brief Реализация виджета списка меню
 */

#include "menu_list.h"
#include "esp_log.h"
#include <stdio.h>

// Внешние стили
extern lv_style_t style_card;
extern lv_style_t style_label;

static const char *TAG = "WIDGET_MENU_LIST";

lv_obj_t* widget_create_menu_list(lv_obj_t *parent,
                                   const menu_item_config_t *items,
                                   uint8_t item_count,
                                   lv_group_t *group)
{
    if (!parent || !items) {
        ESP_LOGE(TAG, "Parent or items is NULL");
        return NULL;
    }
    
    if (item_count == 0) {
        ESP_LOGW(TAG, "Item count is 0");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Creating menu list with %d items", item_count);
    
    // Создаем контейнер для списка
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_add_style(list, &style_card, 0);
    lv_obj_set_size(list, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
    
    // Настраиваем flex layout для вертикального списка
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(list, 16, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    
    // Создаем кнопки меню
    for (uint8_t i = 0; i < item_count; i++) {
        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_add_style(btn, &style_card, 0);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        
        // Добавляем callback если есть
        if (items[i].callback) {
            lv_obj_add_event_cb(btn, items[i].callback, 
                              LV_EVENT_CLICKED, items[i].user_data);
        }
        
        // Создаем лейбл с текстом
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_add_style(label, &style_label, 0);
        
        // Если есть иконка, добавляем ее
        if (items[i].icon && items[i].text) {
            char text_with_icon[64];
            snprintf(text_with_icon, sizeof(text_with_icon), "%s %s", 
                     items[i].icon, items[i].text);
            lv_label_set_text(label, text_with_icon);
        } else if (items[i].text) {
            lv_label_set_text(label, items[i].text);
        }
        
        lv_obj_center(label);
        
        // Добавляем индикатор "→" справа
        lv_obj_t *arrow = lv_label_create(btn);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);  // →
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);
        
        // Добавляем кнопку в группу энкодера если передана
        if (group) {
            lv_group_add_obj(group, btn);
        }
    }
    
    ESP_LOGI(TAG, "Menu list created with %d items%s", 
             item_count, group ? " (added to encoder group)" : "");
    
    return list;
}

