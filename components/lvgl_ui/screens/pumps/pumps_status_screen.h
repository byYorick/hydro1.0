#pragma once

/**
 * @file pumps_status_screen.h
 * @brief Экран статуса насосов
 * 
 * Отображает текущее состояние всех 6 насосов, статистику и управление
 */

#include "esp_err.h"
#include "lvgl.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание экрана статуса насосов
 * 
 * @param context Контекст (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* pumps_status_screen_create(void *context);

/**
 * @brief Обновление статуса насоса на экране
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pumps_status_screen_update(pump_index_t pump_idx);

/**
 * @brief Обновление всех насосов на экране
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pumps_status_screen_update_all(void);

#ifdef __cplusplus
}
#endif

