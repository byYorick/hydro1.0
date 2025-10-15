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

// ===== Стили для виджетов калибровки =====
extern lv_style_t style_pump_widget;        // Контейнер виджета калибровки

// ===== Стиль фокуса =====
extern lv_style_t style_focus;

// ===== Стили для PID экранов =====
extern lv_style_t style_pid_card;           // Карточка PID
extern lv_style_t style_pid_active;         // Активный PID (желтый)
extern lv_style_t style_pid_idle;           // Неактивный PID (серый)
extern lv_style_t style_pid_learning;       // Режим обучения (синий)
extern lv_style_t style_pid_predicting;     // Упреждающая коррекция (фиолетовый)
extern lv_style_t style_pid_tuning;         // Автонастройка (оранжевый)
extern lv_style_t style_pid_target;         // Цель достигнута (зеленый)
extern lv_style_t style_pid_error;          // Ошибка PID (красный)

extern lv_style_t style_param_normal;       // Параметр в нормальном режиме
extern lv_style_t style_param_focused;      // Параметр в фокусе
extern lv_style_t style_param_editing;      // Параметр редактируется

extern lv_style_t style_progress_bg;        // Фон прогресс-бара
extern lv_style_t style_progress_indicator; // Индикатор прогресс-бара

// ===== Функция инициализации стилей =====
void init_styles(void);
void init_pid_styles(void); // Инициализация PID стилей

#ifdef __cplusplus
}
#endif

#endif // LVGL_STYLES_H

