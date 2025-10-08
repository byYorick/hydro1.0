#pragma once

/**
 * @file screen_base.h
 * @brief Базовый экран - общие элементы для всех экранов
 * 
 * Предоставляет базовую структуру экрана с опциональными элементами:
 * - Статус-бар с заголовком
 * - Кнопка "Назад"
 * - Контентная область
 */

#include "lvgl.h"
#include "screen_manager/screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация базового экрана
 */
typedef struct {
    const char *title;             ///< Заголовок
    bool has_status_bar;           ///< Включить статус-бар
    bool has_back_button;          ///< Включить кнопку назад
    lv_event_cb_t back_callback;   ///< Callback для кнопки назад
    void *back_user_data;          ///< Данные для back callback
} screen_base_config_t;

/**
 * @brief Результат создания базового экрана
 */
typedef struct {
    lv_obj_t *screen;              ///< Корневой объект экрана
    lv_obj_t *status_bar;          ///< Статус-бар (NULL если не создан)
    lv_obj_t *back_button;         ///< Кнопка назад (NULL если не создана)
    lv_obj_t *content;             ///< Контентная область
} screen_base_t;

/**
 * @brief Создать базовый экран
 * 
 * Создает экран с общими элементами согласно конфигурации.
 * Возвращает структуру с ссылками на все созданные элементы.
 * 
 * @param config Конфигурация базового экрана
 * @return Структура с созданными элементами
 */
screen_base_t screen_base_create(const screen_base_config_t *config);

/**
 * @brief Уничтожить базовый экран
 * 
 * @param base Базовый экран для уничтожения
 */
void screen_base_destroy(screen_base_t *base);

#ifdef __cplusplus
}
#endif

