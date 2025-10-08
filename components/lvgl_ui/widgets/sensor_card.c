/**
 * @file sensor_card.c
 * @brief Реализация виджета карточки датчика
 */

#include "sensor_card.h"
#include "lvgl_styles.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "WIDGET_SENSOR_CARD";

lv_obj_t* widget_create_sensor_card(lv_obj_t *parent, 
                                     const sensor_card_config_t *config)
{
    if (!parent || !config) {
        ESP_LOGE(TAG, "Parent or config is NULL");
        return NULL;
    }
    
    // Создаем контейнер карточки - компактный размер для сетки 2x3
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, 110, 80);  // 2 в ряд: (240-20)/2 = 110, высота: (264-20)/3 = 81
    
    // ВАЖНО: Применяем стиль фокуса при получении состояния FOCUSED
    extern lv_style_t style_card_focused;
    lv_obj_add_style(card, &style_card_focused, LV_STATE_FOCUSED);
    
    // Настраиваем flex layout
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    
    // Делаем кликабельной
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    
    // Добавляем callback если есть
    if (config->on_click) {
        lv_obj_add_event_cb(card, config->on_click, LV_EVENT_CLICKED, 
                           config->user_data);
    }
    
    // Название датчика
    lv_obj_t *name_label = lv_label_create(card);
    lv_obj_add_style(name_label, &style_unit, 0);
    lv_label_set_text(name_label, config->name ? config->name : "Sensor");
    
    // Текущее значение (большой шрифт)
    lv_obj_t *value_label = lv_label_create(card);
    lv_obj_add_style(value_label, &style_value_large, 0);
    
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%.*f", 
             config->decimals, config->current_value);
    lv_label_set_text(value_label, value_text);
    
    // Сохраняем ссылку на value_label для обновления
    lv_obj_set_user_data(card, value_label);
    
    // Единицы измерения
    lv_obj_t *unit_label = lv_label_create(card);
    lv_obj_add_style(unit_label, &style_unit, 0);
    lv_label_set_text(unit_label, config->unit ? config->unit : "");
    
    // Статус (позже можно добавить логику)
    lv_obj_t *status_label = lv_label_create(card);
    lv_obj_add_style(status_label, &style_unit, 0);
    lv_label_set_text(status_label, "Normal");
    
    ESP_LOGD(TAG, "Sensor card created for '%s'", config->name);
    
    return card;
}

void widget_sensor_card_update_value(lv_obj_t *card, float value)
{
    if (!card) {
        ESP_LOGW(TAG, "Card is NULL");
        return;
    }
    
    // Получаем value_label из user data
    lv_obj_t *value_label = (lv_obj_t*)lv_obj_get_user_data(card);
    if (value_label) {
        char value_text[32];
        snprintf(value_text, sizeof(value_text), "%.2f", value);
        lv_label_set_text(value_label, value_text);
        ESP_LOGD(TAG, "Card value updated to %.2f", value);
    }
}

void widget_sensor_card_add_to_group(lv_obj_t *card, lv_group_t *group)
{
    if (!card || !group) {
        ESP_LOGW(TAG, "Card or group is NULL");
        return;
    }
    
    lv_group_add_obj(group, card);
    ESP_LOGD(TAG, "Sensor card added to group");
}

