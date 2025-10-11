/**
 * @file screen_template.c
 * @brief Реализация шаблонов экранов
 */

#include "screen_template.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/menu_list.h"
#include "esp_log.h"
#include <stdio.h>

// Внешние стили
extern lv_style_t style_bg;
extern lv_style_t style_card;
extern lv_style_t style_label;
extern lv_style_t style_value_large;
extern lv_style_t style_unit;

static const char *TAG = "SCREEN_TEMPLATE";

/* =============================
 *  ШАБЛОН МЕНЮ
 * ============================= */

lv_obj_t* template_create_menu_screen(const template_menu_config_t *config,
                                       lv_group_t *group)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Creating menu screen '%s' with %d items", 
             config->title, config->item_count);
    
    // Создаем базовый экран
    screen_base_config_t base_cfg = {
        .title = config->title,
        .has_status_bar = true,
        .has_back_button = config->has_back_button,
        .back_callback = config->back_callback,
        .back_user_data = NULL,
    };
    
    screen_base_t base = screen_base_create(&base_cfg);
    if (!base.screen) {
        ESP_LOGE(TAG, "Failed to create base screen");
        return NULL;
    }
    
    // Добавляем кнопку назад в группу если есть
    if (base.back_button && group) {
        lv_group_add_obj(group, base.back_button);
    }
    
    // Создаем список меню в контентной области
    widget_create_menu_list(base.content, 
                            config->items,
                            config->item_count,
                            group);
    
    ESP_LOGD(TAG, "Menu screen created");
    
    return base.screen;
}

/* =============================
 *  ШАБЛОН ДЕТАЛИЗАЦИИ
 * ============================= */

lv_obj_t* template_create_detail_screen(const template_detail_config_t *config,
                                         lv_group_t *group)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Creating detail screen '%s'", config->title);
    
    // Создаем базовый экран
    screen_base_config_t base_cfg = {
        .title = config->title,
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = config->back_callback,
        .back_user_data = NULL,
    };
    
    screen_base_t base = screen_base_create(&base_cfg);
    if (!base.screen) {
        ESP_LOGE(TAG, "Failed to create base screen");
        return NULL;
    }
    
    // Добавляем кнопку назад в группу
    if (base.back_button && group) {
        lv_group_add_obj(group, base.back_button);
    }
    
    // ИСПРАВЛЕН LAYOUT: Используем flex вместо абсолютного позиционирования
    lv_obj_set_flex_flow(base.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(base.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(base.content, 8, 0);
    
    // Компактная информационная панель с описанием
    if (config->description) {
        lv_obj_t *info_panel = lv_obj_create(base.content);
        lv_obj_add_style(info_panel, &style_card, 0);
        lv_obj_set_size(info_panel, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(info_panel, 8, 0);  // Компактнее
        
        lv_obj_t *desc_label = lv_label_create(info_panel);
        lv_obj_add_style(desc_label, &style_unit, 0);
        lv_label_set_text(desc_label, config->description);
        lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(desc_label, LV_PCT(95));
        lv_obj_center(desc_label);
    }
    
    // Компактная панель значений в одну строку
    lv_obj_t *values_panel = lv_obj_create(base.content);
    lv_obj_remove_style_all(values_panel);
    lv_obj_add_style(values_panel, &style_card, 0);
    lv_obj_set_size(values_panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(values_panel, 8, 0);
    
    lv_obj_set_flex_flow(values_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(values_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Текущее значение
    lv_obj_t *current_container = lv_obj_create(values_panel);
    lv_obj_remove_style_all(current_container);
    lv_obj_set_size(current_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(current_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(current_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(current_container, 0, 0);
    
    lv_obj_t *current_label = lv_label_create(current_container);
    lv_obj_add_style(current_label, &style_unit, 0);
    lv_label_set_text(current_label, "Current");
    
    lv_obj_t *current_value = lv_label_create(current_container);
    lv_obj_add_style(current_value, &style_value_large, 0);
    char curr_text[32];
    snprintf(curr_text, sizeof(curr_text), "%.*f%s", 
             config->decimals, config->current_value, 
             config->unit ? config->unit : "");
    lv_label_set_text(current_value, curr_text);
    
    // Разделитель
    lv_obj_t *separator = lv_label_create(values_panel);
    lv_obj_add_style(separator, &style_unit, 0);
    lv_label_set_text(separator, "|");
    
    // Целевое значение
    lv_obj_t *target_container = lv_obj_create(values_panel);
    lv_obj_remove_style_all(target_container);
    lv_obj_set_size(target_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(target_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(target_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(target_container, 0, 0);
    
    lv_obj_t *target_label = lv_label_create(target_container);
    lv_obj_add_style(target_label, &style_unit, 0);
    lv_label_set_text(target_label, "Target");
    
    lv_obj_t *target_value = lv_label_create(target_container);
    lv_obj_add_style(target_value, &style_value_large, 0);
    char targ_text[32];
    snprintf(targ_text, sizeof(targ_text), "%.*f%s",
             config->decimals, config->target_value,
             config->unit ? config->unit : "");
    lv_label_set_text(target_value, targ_text);
    
    // Компактная кнопка настроек
    if (config->settings_callback) {
        lv_obj_t *settings_btn = lv_btn_create(base.content);
        lv_obj_add_style(settings_btn, &style_card, 0);
        lv_obj_set_size(settings_btn, LV_PCT(100), 35);  // Полная ширина, компактная высота
        
        // ИСПРАВЛЕНО: Правильно передаем settings_user_data
        lv_obj_add_event_cb(settings_btn, config->settings_callback, 
                           LV_EVENT_CLICKED, config->settings_user_data);
        
        lv_obj_t *settings_label = lv_label_create(settings_btn);
        lv_label_set_text(settings_label, "Settings");
        lv_obj_center(settings_label);
        
        ESP_LOGD(TAG, "Settings button created (user_data: %p)", config->settings_user_data);
    }
    
    ESP_LOGD(TAG, "Detail screen created");
    
    return base.screen;
}

