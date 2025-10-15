/**
 * @file sensor_card.c
 * @brief Реализация виджета карточки датчика
 */

#include "sensor_card.h"
#include "event_helpers.h"
#include "lvgl_styles.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "WIDGET_SENSOR_CARD";

// Callback для освобождения памяти при удалении карточки
static void sensor_card_delete_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    
    typedef struct {
        lv_obj_t *value_label;
        sensor_card_config_t config;
    } card_data_t;
    
    card_data_t *card_data = (card_data_t*)lv_obj_get_user_data(card);
    if (card_data) {
        ESP_LOGD(TAG, "Freeing sensor card data for '%s'", card_data->config.name);
        free(card_data);
        lv_obj_set_user_data(card, NULL);
    }
}

lv_obj_t* widget_create_sensor_card(lv_obj_t *parent, 
                                     const sensor_card_config_t *config)
{
    if (!parent || !config) {
        ESP_LOGE(TAG, "Parent or config is NULL");
        return NULL;
    }
    
    // КРИТИЧНО: Создаем КНОПКУ для правильной обработки KEY_ENTER и фокуса энкодера
    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, 115, 85);  // Оптимизированные размеры
    
    // ВАЖНО: Применяем стиль фокуса при получении состояния FOCUSED
    extern lv_style_t style_card_focused;
    lv_obj_add_style(card, &style_card_focused, LV_STATE_FOCUSED);
    
    // Настраиваем flex layout для максимальной информативности
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 8, 0);  // Компактнее отступы
    lv_obj_set_style_pad_row(card, 4, 0);  // Меньше между строками
    
    // Добавляем callback если есть (клик мышью и нажатие энкодера)
    if (config->on_click) {
        widget_add_click_handler(card, config->on_click, config->user_data);
    }
    
    // КРИТИЧНО: Добавляем обработчик удаления для освобождения памяти
    lv_obj_add_event_cb(card, sensor_card_delete_cb, LV_EVENT_DELETE, NULL);
    
    // Название датчика - компактное
    lv_obj_t *name_label = lv_label_create(card);
    lv_obj_add_style(name_label, &style_unit, 0);
    lv_label_set_text(name_label, config->name ? config->name : "Sensor");
    
    // Текущее значение (большой шрифт) с единицами в одной строке
    lv_obj_t *value_label = lv_label_create(card);
    lv_obj_add_style(value_label, &style_value_large, 0);
    
    char value_text[32];
    // Проверка на валидность начального значения
    if (isnan(config->current_value) || isinf(config->current_value) || config->current_value < -999.0f) {
        snprintf(value_text, sizeof(value_text), "--%s", 
                 config->unit ? config->unit : "");
    } else {
        snprintf(value_text, sizeof(value_text), "%.*f%s", 
                 config->decimals, config->current_value,
                 config->unit ? config->unit : "");
    }
    lv_label_set_text(value_label, value_text);
    
    // ИСПРАВЛЕНО: Сохраняем и value_label, и config в user_data
    // Создаем структуру для хранения обоих указателей
    typedef struct {
        lv_obj_t *value_label;
        sensor_card_config_t config;
    } card_data_t;
    
    card_data_t *card_data = malloc(sizeof(card_data_t));
    if (card_data) {
        card_data->value_label = value_label;
        card_data->config = *config;
        lv_obj_set_user_data(card, card_data);
    } else {
        ESP_LOGE(TAG, "Failed to allocate card_data");
    }
    
    // Статус с индикатором - компактный
    lv_obj_t *status_container = lv_obj_create(card);
    lv_obj_remove_style_all(status_container);
    lv_obj_set_size(status_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(status_container, 0, 0);
    
    // Индикатор статуса (цветная точка)
    lv_obj_t *status_dot = lv_obj_create(status_container);
    lv_obj_set_size(status_dot, 6, 6);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x4CAF50), 0);  // Зеленый по умолчанию
    
    // Текст статуса
    lv_obj_t *status_label = lv_label_create(status_container);
    lv_obj_add_style(status_label, &style_unit, 0);
    lv_label_set_text(status_label, "OK");
    
    ESP_LOGD(TAG, "Sensor card created for '%s'", config->name);
    
    return card;
}

void widget_sensor_card_update_value(lv_obj_t *card, float value)
{
    if (!card) {
        ESP_LOGW(TAG, "Card is NULL");
        return;
    }
    
    typedef struct {
        lv_obj_t *value_label;
        sensor_card_config_t config;
    } card_data_t;
    
    card_data_t *card_data = (card_data_t*)lv_obj_get_user_data(card);
    if (card_data && card_data->value_label) {
        char value_text[32];
        
        // Проверка на валидность значения
        if (isnan(value) || isinf(value) || value < -999.0f) {
            // Нет данных или невалидное значение
            snprintf(value_text, sizeof(value_text), "--%s", 
                     card_data->config.unit ? card_data->config.unit : "");
            ESP_LOGD(TAG, "Card value: no data (--), raw=%.2f", value);
        } else {
            // Нормальное значение
            snprintf(value_text, sizeof(value_text), "%.*f%s", 
                     card_data->config.decimals, value,
                     card_data->config.unit ? card_data->config.unit : "");
            ESP_LOGD(TAG, "Card value updated to %.2f", value);
        }
        
        lv_label_set_text(card_data->value_label, value_text);
    } else {
        ESP_LOGW(TAG, "Card data is NULL or invalid");
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

