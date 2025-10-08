#pragma once

/**
 * @file sensor_card.h
 * @brief Виджет карточки датчика
 * 
 * Компактная карточка для отображения данных датчика на главном экране.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация карточки датчика
 */
typedef struct {
    const char *name;              ///< Название датчика (pH, EC, Temp...)
    const char *unit;              ///< Единицы измерения
    float current_value;           ///< Текущее значение
    uint8_t decimals;              ///< Количество знаков после запятой
    lv_event_cb_t on_click;        ///< Callback при клике
    void *user_data;               ///< Данные для callback
} sensor_card_config_t;

/**
 * @brief Создать карточку датчика
 * 
 * @param parent Родительский объект
 * @param config Конфигурация карточки
 * @return Созданная карточка
 */
lv_obj_t* widget_create_sensor_card(lv_obj_t *parent, 
                                     const sensor_card_config_t *config);

/**
 * @brief Обновить значение на карточке
 * 
 * @param card Карточка
 * @param value Новое значение
 */
void widget_sensor_card_update_value(lv_obj_t *card, float value);

/**
 * @brief Добавить карточку в группу энкодера
 * 
 * @param card Карточка
 * @param group Группа
 */
void widget_sensor_card_add_to_group(lv_obj_t *card, lv_group_t *group);

#ifdef __cplusplus
}
#endif

