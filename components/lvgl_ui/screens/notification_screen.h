/**
 * @file notification_screen.h
 * @brief Экран уведомлений и ошибок через Screen Manager
 * 
 * Новая реализация системы попапов как обычных экранов.
 * Использует screen_manager для управления жизненным циклом.
 */

#pragma once

#include "lvgl.h"
#include "../../notification_system/notification_system.h"
#include "../../error_handler/error_handler.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрация экрана уведомлений в Screen Manager
 * 
 * Должна быть вызвана один раз при инициализации системы.
 */
void notification_screen_register(void);

/**
 * @brief Показать уведомление как экран
 * 
 * @param notification Указатель на уведомление
 * @param timeout_ms Таймаут автоскрытия (мс), 0 = без таймера
 * 
 * @note Если другое уведомление уже показано, или активен cooldown,
 *       новое уведомление будет подавлено.
 */
void notification_screen_show(const notification_t *notification, uint32_t timeout_ms);

/**
 * @brief Показать ошибку как экран
 * 
 * @param error Указатель на ошибку
 * @param timeout_ms Таймаут автоскрытия (мс), 0 = без таймера
 * 
 * @note Критичные ошибки показываются всегда (игнорируют cooldown).
 */
void error_screen_show(const error_info_t *error, uint32_t timeout_ms);

/**
 * @brief Установить cooldown между показом уведомлений
 * 
 * @param cooldown_ms Время cooldown в миллисекундах
 * 
 * @note По умолчанию: 30000 мс (30 секунд)
 */
void notification_screen_set_cooldown(uint32_t cooldown_ms);

/**
 * @brief Обработка очереди уведомлений (вызывается из LVGL задачи)
 * 
 * Обрабатывает отложенные уведомления/ошибки из других задач.
 * Должна вызываться периодически из LVGL задачи.
 * 
 * @return ESP_OK если уведомление обработано, ESP_ERR_NOT_FOUND если очередь пуста
 */
esp_err_t notification_screen_process_queue(void);

#ifdef __cplusplus
}
#endif

