#pragma once

/**
 * @file pid_graph_screen.h
 * @brief Экран графика PID в реальном времени
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана графика PID
 * 
 * @param context pump_index_t (индекс насоса)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_graph_screen_create(void *context);

#ifdef __cplusplus
}
#endif

