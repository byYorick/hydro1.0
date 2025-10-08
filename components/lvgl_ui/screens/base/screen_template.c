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
    
    ESP_LOGI(TAG, "Menu screen created");
    
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
    
    // Информационная панель с описанием
    if (config->description) {
        lv_obj_t *info_panel = lv_obj_create(base.content);
        lv_obj_add_style(info_panel, &style_card, 0);
        lv_obj_set_size(info_panel, LV_PCT(100), 100);
        lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, 0);
        
        lv_obj_t *desc_label = lv_label_create(info_panel);
        lv_obj_add_style(desc_label, &style_unit, 0);
        lv_label_set_text(desc_label, config->description);
        lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(desc_label, LV_PCT(90));
        lv_obj_center(desc_label);
    }
    
    // Панель значений
    lv_obj_t *values_panel = lv_obj_create(base.content);
    lv_obj_remove_style_all(values_panel);
    lv_obj_set_size(values_panel, LV_PCT(100), 80);
    lv_obj_align(values_panel, LV_ALIGN_TOP_MID, 0, 110);
    
    // Текущее значение (слева)
    lv_obj_t *current_label = lv_label_create(values_panel);
    lv_obj_add_style(current_label, &style_label, 0);
    lv_label_set_text(current_label, "Текущее:");
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *current_value = lv_label_create(values_panel);
    lv_obj_add_style(current_value, &style_value_large, 0);
    char curr_text[32];
    snprintf(curr_text, sizeof(curr_text), "%.*f %s", 
             config->decimals, config->current_value, 
             config->unit ? config->unit : "");
    lv_label_set_text(current_value, curr_text);
    lv_obj_align(current_value, LV_ALIGN_TOP_LEFT, 0, 25);
    
    // Целевое значение (справа)
    lv_obj_t *target_label = lv_label_create(values_panel);
    lv_obj_add_style(target_label, &style_label, 0);
    lv_label_set_text(target_label, "Целевое:");
    lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    lv_obj_t *target_value = lv_label_create(values_panel);
    lv_obj_add_style(target_value, &style_value_large, 0);
    char targ_text[32];
    snprintf(targ_text, sizeof(targ_text), "%.*f %s",
             config->decimals, config->target_value,
             config->unit ? config->unit : "");
    lv_label_set_text(target_value, targ_text);
    lv_obj_align(target_value, LV_ALIGN_TOP_RIGHT, 0, 25);
    
    // Кнопка настроек внизу
    if (config->settings_callback) {
        lv_obj_t *settings_btn = lv_btn_create(base.content);
        lv_obj_add_style(settings_btn, &style_card, 0);
        lv_obj_set_size(settings_btn, 120, 40);
        lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_add_event_cb(settings_btn, config->settings_callback, 
                           LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *settings_label = lv_label_create(settings_btn);
        lv_label_set_text(settings_label, "Настройки");
        lv_obj_center(settings_label);
        
        // Добавляем в группу
        if (group) {
            lv_group_add_obj(group, settings_btn);
        }
    }
    
    ESP_LOGI(TAG, "Detail screen created");
    
    return base.screen;
}

