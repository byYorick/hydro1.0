/**
 * @file encoder_value_edit.h
 * @brief Виджет редактирования числовых значений энкодером
 * 
 * Заменяет textarea для ввода чисел.
 * Управление:
 * - Нажатие Enter -> вход в режим редактирования (изменение цвета)
 * - Вращение энкодера -> изменение значения
 * - Нажатие Enter -> выход из режима и сохранение
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация виджета редактирования
 */
typedef struct {
    float min_value;        // Минимальное значение
    float max_value;        // Максимальное значение
    float step;             // Шаг изменения при вращении
    float initial_value;    // Начальное значение
    uint8_t decimals;       // Количество знаков после запятой (0 для целых)
    const char *unit;       // Единица измерения (может быть NULL)
    lv_color_t edit_color;  // Цвет в режиме редактирования
} encoder_value_config_t;

/**
 * @brief Создать виджет редактирования значения
 * 
 * @param parent Родительский объект
 * @param config Конфигурация виджета
 * @return Созданный объект (label с обработчиками)
 */
lv_obj_t* widget_encoder_value_create(lv_obj_t *parent, const encoder_value_config_t *config);

/**
 * @brief Получить текущее значение
 * 
 * @param obj Виджет
 * @return Текущее значение
 */
float widget_encoder_value_get(lv_obj_t *obj);

/**
 * @brief Установить значение
 * 
 * @param obj Виджет
 * @param value Новое значение
 */
void widget_encoder_value_set(lv_obj_t *obj, float value);

/**
 * @brief Проверить, находится ли виджет в режиме редактирования
 * 
 * @param obj Виджет
 * @return true если редактируется
 */
bool widget_encoder_value_is_editing(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

