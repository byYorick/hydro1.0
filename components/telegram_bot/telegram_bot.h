/**
 * @file telegram_bot.h
 * @brief Telegram Bot для push-уведомлений и управления IoT системой
 *
 * Обеспечивает:
 * - Отправку критических алармов
 * - Отправку ежедневных отчетов
 * - Прием команд управления
 * - Отправку статуса системы
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include "esp_err.h"
#include "system_config.h" // telegram_config_t определен там
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// telegram_config_t определен в system_config.h для избежания дублирования

/**
 * @brief Уровень важности сообщения
 */
typedef enum {
    TELEGRAM_SEVERITY_INFO,     ///< Информация
    TELEGRAM_SEVERITY_WARNING,  ///< Предупреждение
    TELEGRAM_SEVERITY_ERROR,    ///< Ошибка
    TELEGRAM_SEVERITY_CRITICAL  ///< Критично
} telegram_severity_t;

/**
 * @brief Callback для обработки команд из Telegram
 * 
 * @param command Текст команды
 * @param user_ctx Пользовательский контекст
 */
typedef void (*telegram_command_callback_t)(const char *command, void *user_ctx);

/**
 * @brief Инициализация Telegram Bot
 * 
 * @param config Конфигурация бота
 * @return ESP_OK при успехе
 */
esp_err_t telegram_bot_init(const telegram_config_t *config);

/**
 * @brief Деинициализация Telegram Bot
 * 
 * @return ESP_OK при успехе
 */
esp_err_t telegram_bot_deinit(void);

/**
 * @brief Запуск Telegram Bot
 * 
 * @return ESP_OK при успехе
 */
esp_err_t telegram_bot_start(void);

/**
 * @brief Остановка Telegram Bot
 * 
 * @return ESP_OK при успехе
 */
esp_err_t telegram_bot_stop(void);

/**
 * @brief Отправка текстового сообщения
 * 
 * @param message Текст сообщения
 * @return ESP_OK при успехе
 */
esp_err_t telegram_send_message(const char *message);

/**
 * @brief Отправка аларма
 * 
 * @param type Тип аларма
 * @param message Сообщение
 * @param severity Уровень важности
 * @return ESP_OK при успехе
 */
esp_err_t telegram_send_alarm(const char *type, const char *message, telegram_severity_t severity);

/**
 * @brief Отправка форматированного сообщения
 * 
 * @param format Формат строки (printf style)
 * @param ... Аргументы
 * @return ESP_OK при успехе
 */
esp_err_t telegram_send_formatted(const char *format, ...);

/**
 * @brief Отправка статуса системы
 * 
 * @param ph Текущий pH
 * @param ec Текущий EC
 * @param temperature Температура
 * @param status Текстовый статус
 * @return ESP_OK при успехе
 */
esp_err_t telegram_send_status(float ph, float ec, float temperature, const char *status);

/**
 * @brief Отправка ежедневного отчета
 * 
 * @param summary Краткая сводка за день
 * @return ESP_OK при успехе
 */
esp_err_t telegram_send_daily_report(const char *summary);

/**
 * @brief Регистрация callback для команд
 * 
 * @param callback Функция обработчик
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t telegram_register_command_callback(telegram_command_callback_t callback, void *user_ctx);

/**
 * @brief Проверка подключения к Telegram API
 * 
 * @return true если подключено
 */
bool telegram_is_connected(void);

/**
 * @brief Установка ID чата
 * 
 * @param chat_id ID чата
 * @return ESP_OK при успехе
 */
esp_err_t telegram_set_chat_id(const char *chat_id);

/**
 * @brief Получение ID чата
 * 
 * @param chat_id Буфер для ID
 * @param max_len Максимальная длина
 * @return ESP_OK при успехе
 */
esp_err_t telegram_get_chat_id(char *chat_id, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // TELEGRAM_BOT_H

