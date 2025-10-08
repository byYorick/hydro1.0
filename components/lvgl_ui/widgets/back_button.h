#pragma once

/**
 * @file back_button.h
 * @brief Виджет кнопки "Назад"
 * 
 * Переиспользуемая кнопка возврата для всех экранов.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создать кнопку "Назад"
 * 
 * @param parent Родительский объект
 * @param callback Callback при нажатии (опционально)
 * @param user_data Данные для callback
 * @return Созданная кнопка
 */
lv_obj_t* widget_create_back_button(lv_obj_t *parent, 
                                     lv_event_cb_t callback,
                                     void *user_data);

/**
 * @brief Добавить кнопку назад в группу энкодера
 * 
 * @param btn Кнопка
 * @param group Группа энкодера
 */
void widget_back_button_add_to_group(lv_obj_t *btn, lv_group_t *group);

#ifdef __cplusplus
}
#endif

