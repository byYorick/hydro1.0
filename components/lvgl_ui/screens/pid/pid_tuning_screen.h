#pragma once

/**
 * @file pid_tuning_screen.h
 * @brief Экран настройки базовых параметров PID (Kp, Ki, Kd)
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана настройки PID
 * 
 * @param context pump_index_t (индекс насоса)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_tuning_screen_create(void *context);

#ifdef __cplusplus
}
#endif

