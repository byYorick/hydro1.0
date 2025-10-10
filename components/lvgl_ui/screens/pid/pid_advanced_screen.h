#pragma once

/**
 * @file pid_advanced_screen.h
 * @brief Экран расширенных настроек PID
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана расширенных настроек PID
 * 
 * @param context pump_index_t (индекс насоса)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_advanced_screen_create(void *context);

#ifdef __cplusplus
}
#endif

