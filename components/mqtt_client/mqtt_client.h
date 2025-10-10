/**
 * @file mqtt_client.h
 * @brief MQTT клиент для IoT гидропонной системы
 *
 * Обеспечивает связь с локальным MQTT брокером для:
 * - Публикации данных датчиков
 * - Получения команд управления
 * - Отправки алармов и телеметрии
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Конфигурация MQTT клиента
 */
typedef struct {
    char broker_uri[128];    ///< URI брокера (mqtt://ip:port)
    char client_id[32];      ///< ID клиента
    char username[32];       ///< Имя пользователя (опционально)
    char password[64];       ///< Пароль (опционально)
    uint16_t keepalive;      ///< Интервал keepalive (сек)
    bool auto_reconnect;     ///< Автопереподключение
} mqtt_client_config_t;

/**
 * @brief Данные датчиков для MQTT
 */
typedef struct {
    float ph;                ///< pH уровень
    float ec;                ///< Электропроводность
    float temperature;       ///< Температура (°C)
    float humidity;          ///< Влажность (%)
    float lux;               ///< Освещённость (lux)
    uint16_t co2;            ///< CO2 (ppm)
    uint32_t timestamp;      ///< Временная метка
    bool ph_alarm;           ///< Тревога pH
    bool ec_alarm;           ///< Тревога EC
    bool temp_alarm;         ///< Тревога температуры
} mqtt_sensor_data_t;

/**
 * @brief Тип команды MQTT
 */
typedef enum {
    MQTT_CMD_SET_PH_TARGET,      ///< Установить целевой pH
    MQTT_CMD_SET_EC_TARGET,      ///< Установить целевой EC
    MQTT_CMD_START_PUMP,         ///< Запустить насос
    MQTT_CMD_STOP_PUMP,          ///< Остановить насос
    MQTT_CMD_CALIBRATE,          ///< Калибровка
    MQTT_CMD_RESET,              ///< Сброс системы
    MQTT_CMD_ENABLE_AUTO,        ///< Включить автоматику
    MQTT_CMD_DISABLE_AUTO,       ///< Выключить автоматику
    MQTT_CMD_UNKNOWN             ///< Неизвестная команда
} mqtt_command_type_t;

/**
 * @brief Структура команды MQTT
 */
typedef struct {
    mqtt_command_type_t type;    ///< Тип команды
    char payload[256];           ///< Полезная нагрузка (JSON)
    uint32_t timestamp;          ///< Временная метка
} mqtt_command_t;

/**
 * @brief Callback для обработки команд
 * 
 * @param command Полученная команда
 * @param user_ctx Пользовательский контекст
 */
typedef void (*mqtt_command_callback_t)(const mqtt_command_t *command, void *user_ctx);

/**
 * @brief Callback для событий подключения
 * 
 * @param connected true если подключено
 * @param user_ctx Пользовательский контекст
 */
typedef void (*mqtt_connection_callback_t)(bool connected, void *user_ctx);

/**
 * @brief Инициализация MQTT клиента
 * 
 * @param config Конфигурация клиента
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_client_init(const mqtt_client_config_t *config);

/**
 * @brief Деинициализация MQTT клиента
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_client_deinit(void);

/**
 * @brief Запуск MQTT клиента
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_client_start(void);

/**
 * @brief Остановка MQTT клиента
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_client_stop(void);

/**
 * @brief Проверка подключения к брокеру
 * 
 * @return true если подключено
 */
bool mqtt_client_is_connected(void);

/**
 * @brief Публикация данных датчика pH
 * 
 * @param value Значение pH
 * @param status Статус ("ok", "alarm", "error")
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_ph(float value, const char *status);

/**
 * @brief Публикация данных датчика EC
 * 
 * @param value Значение EC
 * @param status Статус
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_ec(float value, const char *status);

/**
 * @brief Публикация температуры
 * 
 * @param value Температура
 * @param status Статус
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_temperature(float value, const char *status);

/**
 * @brief Публикация влажности
 * 
 * @param value Влажность
 * @param status Статус
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_humidity(float value, const char *status);

/**
 * @brief Публикация освещённости
 * 
 * @param value Освещённость (lux)
 * @param status Статус
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_lux(float value, const char *status);

/**
 * @brief Публикация CO2
 * 
 * @param value CO2 (ppm)
 * @param status Статус
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_co2(uint16_t value, const char *status);

/**
 * @brief Публикация всех данных датчиков
 * 
 * @param data Структура с данными датчиков
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_sensor_data(const mqtt_sensor_data_t *data);

/**
 * @brief Публикация аларма
 * 
 * @param type Тип аларма ("ph_critical", "ec_high", etc.)
 * @param message Сообщение
 * @param severity Уровень ("low", "medium", "high", "critical")
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_alarm(const char *type, const char *message, const char *severity);

/**
 * @brief Публикация телеметрии системы
 * 
 * @param uptime Время работы (секунды)
 * @param free_heap Свободная память (байты)
 * @param cpu_usage Загрузка CPU (%)
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_telemetry(uint32_t uptime, uint32_t free_heap, float cpu_usage);

/**
 * @brief Публикация статуса системы
 * 
 * @param status Статус ("online", "offline", "error")
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_publish_status(const char *status);

/**
 * @brief Подписка на топик команд
 * 
 * @param callback Callback для обработки команд
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_subscribe_commands(mqtt_command_callback_t callback, void *user_ctx);

/**
 * @brief Регистрация callback для событий подключения
 * 
 * @param callback Callback функция
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_register_connection_callback(mqtt_connection_callback_t callback, void *user_ctx);

/**
 * @brief Получение ID устройства
 * 
 * @param device_id Буфер для ID
 * @param max_len Максимальная длина
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_get_device_id(char *device_id, size_t max_len);

/**
 * @brief Установка ID устройства
 * 
 * @param device_id ID устройства
 * @return ESP_OK при успехе
 */
esp_err_t mqtt_set_device_id(const char *device_id);

#ifdef __cplusplus
}
#endif

#endif // MQTT_CLIENT_H

