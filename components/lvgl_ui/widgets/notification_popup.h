/**
 * @file notification_popup.h
 * @brief Адаптер для показа уведомлений через Screen Manager
 * 
 * Все попапы теперь управляются через popup_screen.c и Screen Manager.
 * Этот модуль только регистрирует callback в notification_system.
 */

#pragma once

#include "../../notification_system/notification_system.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация адаптера попапов уведомлений
 * 
 * Регистрирует callback в notification_system, который будет
 * показывать уведомления через popup_screen.c (Screen Manager).
 */
void widget_notification_popup_init(void);

#ifdef __cplusplus
}
#endif
