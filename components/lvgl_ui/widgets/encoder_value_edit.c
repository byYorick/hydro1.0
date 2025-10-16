/**
 * @file encoder_value_edit.c
 * @brief Реализация виджета редактирования числовых значений энкодером
 */

#include "encoder_value_edit.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>

static const char *TAG = "ENCODER_VALUE";

// Структура данных виджета
typedef struct {
    float value;
    float min_value;
    float max_value;
    float step;
    uint8_t decimals;
    char unit[16];
    bool editing;
    lv_color_t normal_bg_color;
    lv_color_t edit_bg_color;
    lv_obj_t *label;  // Ссылка на внутренний label
} encoder_value_data_t;

/**
 * @brief Обновить отображение значения
 */
static void update_display(lv_obj_t *obj)
{
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    if (!data) return;
    
    char text[64];
    
    if (data->decimals == 0) {
        // Целое число
        if (data->unit[0] != '\0') {
            snprintf(text, sizeof(text), "%d %s", (int)data->value, data->unit);
        } else {
            snprintf(text, sizeof(text), "%d", (int)data->value);
        }
    } else {
        // Дробное число
        if (data->unit[0] != '\0') {
            snprintf(text, sizeof(text), "%.*f %s", data->decimals, data->value, data->unit);
        } else {
            snprintf(text, sizeof(text), "%.*f", data->decimals, data->value);
        }
    }
    
    // Обновляем текст в label (внутри кнопки)
    if (data->label) {
        lv_label_set_text(data->label, text);
    }
    
    // Изменение стиля кнопки в зависимости от режима
    if (data->editing) {
        lv_obj_set_style_bg_color(obj, data->edit_bg_color, 0);
        if (data->label) lv_obj_set_style_text_color(data->label, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(obj, data->normal_bg_color, 0);
        if (data->label) lv_obj_set_style_text_color(data->label, lv_color_white(), 0);
    }
}

/**
 * @brief Обработчик событий
 */
static void value_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    
    if (!data) return;
    
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        // Переключение режима редактирования
        data->editing = !data->editing;
        update_display(obj);
        
        ESP_LOGD(TAG, "Режим редактирования: %s, значение=%.2f", 
                 data->editing ? "ВКЛ" : "ВЫКЛ", data->value);
    }
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (data->editing) {
            if (key == LV_KEY_UP) {
                // Увеличение
                float new_value = data->value + data->step;
                if (new_value <= data->max_value) {
                    data->value = new_value;
                    update_display(obj);
                }
            }
            else if (key == LV_KEY_DOWN) {
                // Уменьшение
                float new_value = data->value - data->step;
                if (new_value >= data->min_value) {
                    data->value = new_value;
                    update_display(obj);
                }
            }
            else if (key == LV_KEY_ENTER) {
                // Выход из режима редактирования
                data->editing = false;
                update_display(obj);
                
                ESP_LOGI(TAG, "Значение сохранено: %.2f", data->value);
            }
        }
    }
}

/**
 * @brief Деструктор виджета
 */
static void value_delete_event(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    
    if (data) {
        free(data);
        lv_obj_set_user_data(obj, NULL);
    }
}

lv_obj_t* widget_encoder_value_create(lv_obj_t *parent, const encoder_value_config_t *config)
{
    if (!parent || !config) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }
    
    // КРИТИЧНО: Создание КНОПКИ для правильной работы с энкодером
    lv_obj_t *obj = lv_btn_create(parent);
    
    // Выделение памяти для данных
    encoder_value_data_t *data = (encoder_value_data_t *)malloc(sizeof(encoder_value_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        lv_obj_del(obj);
        return NULL;
    }
    
    // Инициализация данных
    data->value = config->initial_value;
    data->min_value = config->min_value;
    data->max_value = config->max_value;
    data->step = config->step;
    data->decimals = config->decimals;
    data->editing = false;
    data->normal_bg_color = lv_color_hex(0x3a3a3a);
    data->edit_bg_color = config->edit_color;
    
    if (config->unit) {
        snprintf(data->unit, sizeof(data->unit), "%s", config->unit);
    } else {
        data->unit[0] = '\0';
    }
    
    lv_obj_set_user_data(obj, data);
    
    // Настройка стиля
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, data->normal_bg_color, 0);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);  // Увеличено для кнопки
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    
    // Создаем label внутри кнопки для отображения текста
    lv_obj_t *label = lv_label_create(obj);
    lv_obj_center(label);
    data->label = label;  // Сохраняем ссылку на label
    
    // Добавление обработчиков
    lv_obj_add_event_cb(obj, value_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(obj, value_delete_event, LV_EVENT_DELETE, NULL);
    
    // Первоначальное отображение
    update_display(obj);
    
    return obj;
}

float widget_encoder_value_get(lv_obj_t *obj)
{
    if (!obj) return 0.0f;
    
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    if (!data) return 0.0f;
    
    return data->value;
}

void widget_encoder_value_set(lv_obj_t *obj, float value)
{
    if (!obj) return;
    
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    if (!data) return;
    
    // Ограничение значения
    if (value < data->min_value) value = data->min_value;
    if (value > data->max_value) value = data->max_value;
    
    data->value = value;
    update_display(obj);
}

bool widget_encoder_value_is_editing(lv_obj_t *obj)
{
    if (!obj) return false;
    
    encoder_value_data_t *data = (encoder_value_data_t *)lv_obj_get_user_data(obj);
    if (!data) return false;
    
    return data->editing;
}

