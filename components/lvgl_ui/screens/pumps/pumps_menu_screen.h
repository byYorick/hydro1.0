/**
 * @file pumps_menu_screen.h
 * @brief Главное меню управления насосами
 */

#ifndef PUMPS_MENU_SCREEN_H
#define PUMPS_MENU_SCREEN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрирует экран меню насосов в Screen Manager
 */
void pumps_menu_screen_register(void);

#ifdef __cplusplus
}
#endif

#endif // PUMPS_MENU_SCREEN_H

