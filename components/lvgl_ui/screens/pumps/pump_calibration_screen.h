#pragma once

/**
 * @file pump_calibration_screen.h
 * @brief Экран калибровки насосов
 * 
 * Позволяет откалибровать реальный расход каждого насоса
 */

#include "esp_err.h"
#include "lvgl.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана калибровки насосов
 * 
 * @param context Контекст (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* pump_calibration_screen_create(void *context);

#ifdef __cplusplus
}
#endif

