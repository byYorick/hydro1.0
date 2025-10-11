#pragma once

/**
 * @file status_bar.h
 * @brief Виджет статус-бара
 * 
 * Переиспользуемый статус-бар для всех экранов с заголовком.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создать статус-бар с заголовком
 * 
 * @param parent Родительский объект
 * @param title Текст заголовка
 * @return Созданный статус-бар
 */
lv_obj_t* widget_create_status_bar(lv_obj_t *parent, const char *title);

/**
 * @brief Обновить текст заголовка
 * 
 * @param status_bar Статус-бар
 * @param title Новый текст
 */
void widget_status_bar_set_title(lv_obj_t *status_bar, const char *title);

/**
 * @brief Обновить счетчик уведомлений
 * 
 * @param status_bar Статус-бар
 * @param count Количество непрочитанных уведомлений
 */
void widget_status_bar_update_notifications(lv_obj_t *status_bar, uint32_t count);

#ifdef __cplusplus
}
#endif

