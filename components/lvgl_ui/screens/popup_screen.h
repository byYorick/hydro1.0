#pragma once

#include "lvgl.h"
#include "../../notification_system/notification_system.h"
#include "../../error_handler/error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Тип попапа
 */
typedef enum {
    POPUP_TYPE_NOTIFICATION,  // Попап уведомления
    POPUP_TYPE_ERROR          // Попап ошибки
} popup_type_t;

/**
 * @brief Конфигурация попапа
 */
typedef struct {
    popup_type_t type;
    union {
        notification_t notification;
        error_info_t error;
    } data;
    uint32_t timeout_ms;      // Таймаут автоскрытия (0 = без таймера)
    bool has_ok_button;       // Показывать кнопку OK
} popup_config_t;

/**
 * @brief Инициализация и регистрация popup экрана
 */
void popup_screen_register(void);

/**
 * @brief Показать попап с уведомлением
 * @param notification Указатель на уведомление
 * @param timeout_ms Таймаут автоскрытия (мс), 0 = без таймера
 */
void popup_show_notification(const notification_t *notification, uint32_t timeout_ms);

/**
 * @brief Показать попап с ошибкой
 * @param error Указатель на ошибку
 * @param timeout_ms Таймаут автоскрытия (мс), 0 = без таймера
 */
void popup_show_error(const error_info_t *error, uint32_t timeout_ms);

/**
 * @brief Закрыть текущий попап
 */
void popup_close(void);

#ifdef __cplusplus
}
#endif

