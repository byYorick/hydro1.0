/**
 * @file mobile_app_interface.h
 * @brief Заголовочный файл интерфейса для мобильного приложения
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef MOBILE_APP_INTERFACE_H
#define MOBILE_APP_INTERFACE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Режимы сетевого подключения
 */
typedef enum {
    NETWORK_MODE_NONE = 0,   ///< Сеть не используется
    NETWORK_MODE_STA,        ///< Режим станции (клиент WiFi)
    NETWORK_MODE_AP,         ///< Режим точки доступа
    NETWORK_MODE_HYBRID      ///< Гибридный режим (STA + AP)
} network_mode_t;

/**
 * @brief Структура данных датчиков для отправки в мобильное приложение
 */
typedef struct {
    float temperature;       ///< Температура (°C)
    float humidity;          ///< Влажность (%)
    float ph;                ///< pH уровень
    float ec;                ///< Электропроводность (EC)
    float lux;               ///< Освещённость (Lux)
    uint16_t co2;            ///< CO2 (ppm)
    uint32_t timestamp;      ///< Временная метка
} mobile_sensor_data_t;

/**
 * @brief Структура управляющей команды от мобильного приложения
 */
typedef struct {
    char command[32];        ///< Название команды
    char param1[64];         ///< Параметр 1
    char param2[64];         ///< Параметр 2
    uint8_t priority;        ///< Приоритет команды
    uint32_t timestamp;      ///< Временная метка
} mobile_control_command_t;

/**
 * @brief Обработчик команд от мобильного приложения
 * 
 * @param command Указатель на структуру команды
 * @param ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
typedef esp_err_t (*mobile_command_handler_t)(const mobile_control_command_t *command, void *ctx);

/**
 * @brief Обработчик ошибок мобильного интерфейса
 * 
 * @param error Код ошибки
 * @param ctx Контекст пользователя
 */
typedef void (*mobile_error_handler_t)(esp_err_t error, void *ctx);

/**
 * @brief Инициализация мобильного интерфейса
 * 
 * @param mode Режим сетевого подключения
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_interface_init(network_mode_t mode);

/**
 * @brief Деинициализация мобильного интерфейса
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_interface_deinit(void);

/**
 * @brief Отправка данных датчиков в мобильное приложение
 * 
 * @param data Указатель на структуру данных датчиков
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_send_sensor_data(const mobile_sensor_data_t *data);

/**
 * @brief Аутентификация мобильного приложения по токену
 * 
 * @param token Токен аутентификации
 * @return true если токен валиден
 */
bool mobile_app_authenticate(const char *token);

/**
 * @brief Получение управляющей команды от мобильного приложения
 * 
 * @param command Указатель на структуру для записи команды
 * @return Количество полученных команд
 */
int mobile_app_get_control_commands(mobile_control_command_t *command);

/**
 * @brief Регистрация обработчика команд
 * 
 * @param handler Функция-обработчик
 * @param ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_register_command_handler(mobile_command_handler_t handler, void *ctx);

/**
 * @brief Регистрация обработчика ошибок
 * 
 * @param handler Функция-обработчик
 * @param ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_register_error_handler(mobile_error_handler_t handler, void *ctx);

/**
 * @brief Проверка, подключено ли мобильное приложение
 * 
 * @return true если подключено
 */
bool mobile_app_is_connected(void);

/**
 * @brief Получение информации об устройстве
 * 
 * @param device_info Буфер для записи информации
 * @param max_length Максимальная длина буфера
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_get_device_info(char *device_info, size_t max_length);

/**
 * @brief Отправка уведомления в мобильное приложение
 * 
 * @param type Тип уведомления
 * @param message Текст сообщения
 * @param priority Приоритет (0-255)
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_send_notification(const char *type, const char *message, uint8_t priority);

/**
 * @brief Включение/выключение синхронизации данных
 * 
 * @param enable true для включения
 * @param sync_interval Интервал синхронизации (мс)
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_enable_sync(bool enable, uint32_t sync_interval);

/**
 * @brief Сохранение настроек мобильного интерфейса
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_save_settings(void);

/**
 * @brief Загрузка настроек мобильного интерфейса
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_load_settings(void);

/**
 * @brief Сброс настроек мобильного интерфейса к значениям по умолчанию
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_reset_settings(void);

/**
 * @brief Включение/выключение автономного режима
 * 
 * @param enable true для включения
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_enable_offline_mode(bool enable);

/**
 * @brief Проверка наличия данных для синхронизации в автономном режиме
 * 
 * @return true если есть несинхронизированные данные
 */
bool mobile_app_has_offline_data(void);

/**
 * @brief Синхронизация данных автономного режима
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_sync_offline_data(void);

/**
 * @brief Валидация версии мобильного приложения
 * 
 * @param app_version Строка версии приложения
 * @return true если версия поддерживается
 */
bool mobile_app_validate_version(const char *app_version);

/**
 * @brief Получение рекомендуемой версии мобильного приложения
 * 
 * @param recommended_version Буфер для записи версии
 * @param max_length Максимальная длина буфера
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_get_recommended_version(char *recommended_version, size_t max_length);

/**
 * @brief Отправка логов в мобильное приложение
 * 
 * @param log_type Тип логов для отправки
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_send_logs(const char *log_type);

/**
 * @brief Получение диагностической информации
 * 
 * @param diagnostic_info Буфер для записи информации
 * @param max_length Максимальная длина буфера
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_get_diagnostic_info(char *diagnostic_info, size_t max_length);

/**
 * @brief Включение/выключение режима отладки
 * 
 * @param enable true для включения
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_enable_debug_mode(bool enable);

/**
 * @brief Тестирование подключения к мобильному приложению
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mobile_app_test_connection(void);

#ifdef __cplusplus
}
#endif

#endif // MOBILE_APP_INTERFACE_H

