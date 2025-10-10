#pragma once

/**
 * @file pid_detail_screen.h
 * @brief Экран детальной информации о PID контроллере
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана детальной информации PID
 * 
 * @param context pump_index_t (индекс насоса)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_detail_screen_create(void *context);

#ifdef __cplusplus
}
#endif

