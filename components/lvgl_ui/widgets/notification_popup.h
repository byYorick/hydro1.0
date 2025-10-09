#pragma once

/**
 * @file notification_popup.h
 * @brief Виджет всплывающего уведомления
 * 
 * Отображает уведомления из notification_system как попап на экране.
 */

#include "lvgl.h"
#include "notification_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Показать всплывающее уведомление
 * 
 * @param notification Уведомление для отображения
 * @return Созданный попап или NULL при ошибке
 */
lv_obj_t* widget_show_notification_popup(const notification_t *notification);

/**
 * @brief Скрыть все активные уведомления
 */
void widget_hide_all_popups(void);

/**
 * @brief Инициализация системы попапов
 * 
 * Регистрирует callback в notification_system для автоматического показа
 */
void widget_notification_popup_init(void);

#ifdef __cplusplus
}
#endif


