#pragma once

/**
 * @file screen_template.h
 * @brief Шаблоны типовых экранов
 * 
 * Готовые шаблоны для быстрого создания экранов:
 * - Экран меню (список кнопок)
 * - Экран формы (поля ввода)
 * - Экран детализации (информация + кнопки)
 */

#include "lvgl.h"
#include "screen_base.h"
#include "../../widgets/menu_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  ШАБЛОН МЕНЮ
 * ============================= */

/**
 * @brief Конфигурация экрана меню
 */
typedef struct {
    const char *title;                    ///< Заголовок меню
    const menu_item_config_t *items;      ///< Массив элементов меню
    uint8_t item_count;                   ///< Количество элементов
    bool has_back_button;                 ///< Включить кнопку назад
    lv_event_cb_t back_callback;          ///< Callback для back
} template_menu_config_t;

/**
 * @brief Создать экран меню по шаблону
 * 
 * @param config Конфигурация
 * @param group Группа энкодера (для добавления кнопок)
 * @return Созданный экран
 */
lv_obj_t* template_create_menu_screen(const template_menu_config_t *config,
                                       lv_group_t *group);

/* =============================
 *  ШАБЛОН ДЕТАЛИЗАЦИИ
 * ============================= */

/**
 * @brief Конфигурация экрана детализации
 */
typedef struct {
    const char *title;                    ///< Заголовок
    const char *description;              ///< Описание
    float current_value;                  ///< Текущее значение
    float target_value;                   ///< Целевое значение
    const char *unit;                     ///< Единицы измерения
    uint8_t decimals;                     ///< Знаков после запятой
    lv_event_cb_t settings_callback;      ///< Callback для кнопки Settings
    lv_event_cb_t back_callback;          ///< Callback для кнопки Back
} template_detail_config_t;

/**
 * @brief Создать экран детализации по шаблону
 * 
 * @param config Конфигурация
 * @param group Группа энкодера
 * @return Созданный экран
 */
lv_obj_t* template_create_detail_screen(const template_detail_config_t *config,
                                         lv_group_t *group);

#ifdef __cplusplus
}
#endif

