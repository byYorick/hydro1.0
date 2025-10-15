/**
 * @file pid_intelligent_detail.h
 * @brief Детальный экран адаптивного PID контроллера
 * 
 * Экран с 3 вкладками:
 * - Обзор: текущее состояние, статус, компоненты
 * - Настройки: коэффициенты, пороги, лимиты
 * - График: визуализация тренда и прогноза
 */

#ifndef PID_INTELLIGENT_DETAIL_H
#define PID_INTELLIGENT_DETAIL_H

#include "lvgl.h"
#include "system_config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание детального экрана PID
 * @param params pump_index_t переданный как (void*)(intptr_t)pump_idx
 * @return LVGL объект экрана или NULL при ошибке
 */
lv_obj_t* pid_intelligent_detail_create(void *params);

/**
 * @brief Callback при показе экрана
 * @param screen_obj LVGL объект экрана
 * @param params pump_index_t переданный как (void*)(intptr_t)pump_idx
 * @return ESP_OK при успехе
 */
esp_err_t pid_intelligent_detail_on_show(lv_obj_t *screen_obj, void *params);

/**
 * @brief Callback при скрытии экрана
 * @param screen_obj LVGL объект экрана
 * @return ESP_OK при успехе
 */
esp_err_t pid_intelligent_detail_on_hide(lv_obj_t *screen_obj);

#ifdef __cplusplus
}
#endif

#endif // PID_INTELLIGENT_DETAIL_H

