#pragma once

/**
 * @file event_helpers.h
 * @brief Вспомогательные функции для обработки событий LVGL
 * 
 * Унифицированные helper'ы для работы с событиями, избегающие дублирование кода.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Добавить обработчик для клика мышью И нажатия энкодера
 * 
 * Унифицированная функция для обработки и CLICKED (клик мыши) и PRESSED (энкодер).
 * Использовать вместо дублирования двух lv_obj_add_event_cb() вызовов.
 * 
 * @param obj Объект LVGL
 * @param cb Callback функция
 * @param user_data Пользовательские данные для callback
 */
static inline void widget_add_click_handler(lv_obj_t *obj, lv_event_cb_t cb, void *user_data) {
    if (!obj || !cb) return;
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_PRESSED, user_data);
}

#ifdef __cplusplus
}
#endif

