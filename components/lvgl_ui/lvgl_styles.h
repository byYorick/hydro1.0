/**
 * @file lvgl_styles.h
 * @brief Shared LVGL styles for all screens
 */

#ifndef LVGL_STYLES_H
#define LVGL_STYLES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===== Основные стили =====
extern lv_style_t style_bg;          // Фон экрана
extern lv_style_t style_header;      // Заголовок
extern lv_style_t style_title;       // Текст заголовка
extern lv_style_t style_label;       // Обычные метки

// ===== Стили карточек датчиков =====
extern lv_style_t style_card;               // Карточка датчика
extern lv_style_t style_card_focused;       // Выделенная карточка
extern lv_style_t style_value_large;        // Большие значения
extern lv_style_t style_unit;               // Единицы измерения

// ===== Стили кнопок =====
extern lv_style_t style_button;              // Основная кнопка
extern lv_style_t style_button_pressed;      // Нажатая кнопка
extern lv_style_t style_button_secondary;    // Вторичная кнопка (назад)

// ===== Стили индикаторов =====
extern lv_style_t style_status_normal;    // Зеленый (OK)
extern lv_style_t style_status_warning;   // Оранжевый (предупреждение)
extern lv_style_t style_status_danger;    // Красный (опасность)
extern lv_style_t style_status_bar;       // Статусная панель

// ===== Стили детализации =====
extern lv_style_t style_detail_bg;          // Фон детализации
extern lv_style_t style_detail_container;   // Контейнер детализации
extern lv_style_t style_detail_title;       // Заголовок детализации
extern lv_style_t style_detail_value_big;   // Очень большое значение

// ===== Стиль фокуса =====
extern lv_style_t style_focus;

// ===== Функция инициализации стилей =====
void init_styles(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_STYLES_H

