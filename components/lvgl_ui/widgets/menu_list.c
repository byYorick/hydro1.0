/**
 * @file menu_list.c
 * @brief Реализация виджета списка меню
 */

#include "menu_list.h"
#include "event_helpers.h"
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
    
    // Создаем контейнер для списка - НА ВЕСЬ ЭКРАН
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_add_style(list, &style_card, 0);
    lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);  // 100% ширины
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);  // БЕЗ ОТСТУПА сверху
    
    // Настраиваем flex layout для вертикального списка
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(list, 6, 0);  // Меньше отступы по краям
    lv_obj_set_style_pad_row(list, 4, 0);  // Вернули 4
    
    // Создаем компактные кнопки меню
    for (uint8_t i = 0; i < item_count; i++) {
        // КРИТИЧНО: Feed watchdog при создании каждого виджета
        extern void esp_task_wdt_reset(void);
        esp_task_wdt_reset();
        
        extern lv_style_t style_card_focused;
        
        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_add_style(btn, &style_card, 0);
        lv_obj_add_style(btn, &style_card_focused, LV_STATE_FOCUSED);  // Стиль фокуса энкодера
        lv_obj_set_size(btn, LV_PCT(100), 32);  // Вернули высоту 32px
        
        // Добавляем callback если есть (клик мышью и нажатие энкодера)
        if (items[i].callback) {
            widget_add_click_handler(btn, items[i].callback, items[i].user_data);
        }
        
        // Если есть иконка, создаем отдельный лейбл для нее
        if (items[i].icon) {
            lv_obj_t *icon = lv_label_create(btn);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);  // Шрифт с иконками
            lv_label_set_text(icon, items[i].icon);
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        }
        
        // Создаем лейбл с текстом
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_add_style(label, &style_label, 0);
        
        if (items[i].text) {
            lv_label_set_text(label, items[i].text);
        }
        
        // Центрируем текст (или немного левее если есть иконка)
        if (items[i].icon) {
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 28, 0);  // Отступ для иконки
        } else {
            lv_obj_center(label);
        }
        
        // Добавляем индикатор "→" справа
        lv_obj_t *arrow = lv_label_create(btn);
        // Используем встроенный шрифт LVGL для иконок
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
        lv_label_set_text(arrow, ">");  // Простой символ вместо LV_SYMBOL_RIGHT
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

