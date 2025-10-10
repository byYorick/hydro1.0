#pragma once

/**
 * @file pumps_manual_screen.h
 * @brief Экран ручного управления насосами
 * 
 * Позволяет запускать/останавливать насосы вручную
 */

#include "esp_err.h"
#include "lvgl.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана ручного управления
 * 
 * @param context Контекст (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* pumps_manual_screen_create(void *context);

#ifdef __cplusplus
}
#endif

