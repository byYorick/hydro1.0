#pragma once

/**
 * @file menu_list.h
 * @brief Виджет списка меню
 * 
 * Создает вертикальный список кнопок для меню.
 */

#include "lvgl.h"

// Объявление русского шрифта
LV_FONT_DECLARE(montserrat_ru)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация элемента меню
 */
typedef struct {
    const char *text;              ///< Текст кнопки
    const char *icon;              ///< Иконка (опционально, LVGL symbol)
    lv_event_cb_t callback;        ///< Callback при нажатии
    void *user_data;               ///< Данные для callback
} menu_item_config_t;

/**
 * @brief Создать список меню
 * 
 * @param parent Родительский объект
 * @param items Массив конфигураций элементов меню
 * @param item_count Количество элементов
 * @param group Группа энкодера для добавления кнопок (опционально)
 * @return Созданный список
 */
lv_obj_t* widget_create_menu_list(lv_obj_t *parent,
                                   const menu_item_config_t *items,
                                   uint8_t item_count,
                                   lv_group_t *group);

#ifdef __cplusplus
}
#endif

