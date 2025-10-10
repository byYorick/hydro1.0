#pragma once

/**
 * @file pid_thresholds_screen.h
 * @brief Экран настройки порогов срабатывания PID
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана настройки порогов
 * 
 * @param context pump_index_t (индекс насоса)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_thresholds_screen_create(void *context);

#ifdef __cplusplus
}
#endif

