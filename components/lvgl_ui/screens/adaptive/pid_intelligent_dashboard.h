/**
 * @file pid_intelligent_dashboard.h
 * @brief Главный экран интеллектуальной адаптивной PID системы
 */

#ifndef PID_INTELLIGENT_DASHBOARD_H
#define PID_INTELLIGENT_DASHBOARD_H

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание главного dashboard экрана
 * 
 * @param context Контекст (не используется)
 * @return Указатель на созданный экран
 */
lv_obj_t* pid_intelligent_dashboard_create(void *context);

/**
 * @brief Callback при показе экрана
 */
esp_err_t pid_intelligent_dashboard_on_show(lv_obj_t *screen, void *params);

/**
 * @brief Callback при скрытии экрана
 */
esp_err_t pid_intelligent_dashboard_on_hide(lv_obj_t *screen);

/**
 * @brief Обновление экрана (вызывается периодически)
 */
esp_err_t pid_intelligent_dashboard_update(void);

#ifdef __cplusplus
}
#endif

#endif // PID_INTELLIGENT_DASHBOARD_H

