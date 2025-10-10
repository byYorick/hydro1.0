/**
 * @file mesh_network.h
 * @brief Mesh-сеть через ESP-NOW для распределенной IoT системы
 *
 * Обеспечивает:
 * - Gateway узел с WiFi/MQTT
 * - Slave узлы только с ESP-NOW (без WiFi)
 * - Быстрый обмен данными датчиков
 * - Рассылку команд управления
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Роль устройства в mesh-сети
 */
typedef enum {
    MESH_ROLE_GATEWAY,   ///< Главный узел (с WiFi и MQTT)
    MESH_ROLE_SLAVE      ///< Подчиненный узел (только ESP-NOW)
} mesh_role_t;

/**
 * @brief Тип сообщения mesh
 */
typedef enum {
    MESH_MSG_SENSOR_DATA,    ///< Данные датчиков
    MESH_MSG_COMMAND,        ///< Команда управления
    MESH_MSG_HEARTBEAT,      ///< Heartbeat (проверка связи)
    MESH_MSG_ACK,            ///< Подтверждение
    MESH_MSG_ERROR           ///< Ошибка
} mesh_msg_type_t;

/**
 * @brief Данные датчиков для передачи по mesh
 */
typedef struct {
    uint8_t device_id;       ///< ID устройства (0-255)
    float ph;                ///< pH
    float ec;                ///< EC
    float temperature;       ///< Температура
    float humidity;          ///< Влажность
    uint16_t lux;            ///< Освещённость
    uint16_t co2;            ///< CO2
    uint32_t timestamp;      ///< Временная метка
} mesh_sensor_data_t;

/**
 * @brief Команда управления
 */
typedef struct {
    uint8_t target_device;   ///< ID целевого устройства (0xFF = broadcast)
    uint8_t command_type;    ///< Тип команды
    uint8_t param1;          ///< Параметр 1
    uint8_t param2;          ///< Параметр 2
    uint32_t timestamp;      ///< Временная метка
} mesh_command_t;

/**
 * @brief Heartbeat сообщение
 */
typedef struct {
    uint8_t device_id;       ///< ID устройства
    uint8_t battery_level;   ///< Уровень батареи (0-100%)
    int8_t rssi;             ///< Уровень сигнала
    uint32_t uptime;         ///< Время работы (секунды)
} mesh_heartbeat_t;

/**
 * @brief Mesh сообщение (общая структура)
 */
typedef struct {
    uint8_t device_id;       ///< ID отправителя
    mesh_msg_type_t msg_type;///< Тип сообщения
    uint32_t timestamp;      ///< Временная метка
    uint8_t payload[200];    ///< Полезная нагрузка
} mesh_message_t;

/**
 * @brief Callback для получения данных датчиков
 * 
 * @param device_id ID устройства
 * @param data Данные датчиков
 * @param user_ctx Пользовательский контекст
 */
typedef void (*mesh_sensor_callback_t)(uint8_t device_id, const mesh_sensor_data_t *data, void *user_ctx);

/**
 * @brief Callback для получения команд
 * 
 * @param command Команда
 * @param user_ctx Пользовательский контекст
 */
typedef void (*mesh_command_callback_t)(const mesh_command_t *command, void *user_ctx);

/**
 * @brief Callback для heartbeat
 * 
 * @param device_id ID устройства
 * @param heartbeat Heartbeat данные
 * @param user_ctx Пользовательский контекст
 */
typedef void (*mesh_heartbeat_callback_t)(uint8_t device_id, const mesh_heartbeat_t *heartbeat, void *user_ctx);

/**
 * @brief Инициализация mesh-сети
 * 
 * @param role Роль устройства
 * @param device_id ID устройства (1-254)
 * @return ESP_OK при успехе
 */
esp_err_t mesh_network_init(mesh_role_t role, uint8_t device_id);

/**
 * @brief Деинициализация mesh-сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mesh_network_deinit(void);

/**
 * @brief Запуск mesh-сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mesh_network_start(void);

/**
 * @brief Остановка mesh-сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t mesh_network_stop(void);

/**
 * @brief Регистрация peer устройства
 * 
 * @param peer_mac MAC адрес peer
 * @param device_id ID устройства
 * @return ESP_OK при успехе
 */
esp_err_t mesh_register_peer(const uint8_t *peer_mac, uint8_t device_id);

/**
 * @brief Отправка данных датчиков на gateway
 * 
 * @param data Данные датчиков
 * @return ESP_OK при успехе
 */
esp_err_t mesh_send_sensor_data(const mesh_sensor_data_t *data);

/**
 * @brief Broadcast команды всем устройствам
 * 
 * @param command Команда
 * @return ESP_OK при успехе
 */
esp_err_t mesh_broadcast_command(const mesh_command_t *command);

/**
 * @brief Отправка команды конкретному устройству
 * 
 * @param device_id ID целевого устройства
 * @param command Команда
 * @return ESP_OK при успехе
 */
esp_err_t mesh_send_command(uint8_t device_id, const mesh_command_t *command);

/**
 * @brief Отправка heartbeat
 * 
 * @param heartbeat Heartbeat данные
 * @return ESP_OK при успехе
 */
esp_err_t mesh_send_heartbeat(const mesh_heartbeat_t *heartbeat);

/**
 * @brief Регистрация callback для данных датчиков
 * 
 * @param callback Функция обработчик
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t mesh_register_sensor_callback(mesh_sensor_callback_t callback, void *user_ctx);

/**
 * @brief Регистрация callback для команд
 * 
 * @param callback Функция обработчик
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t mesh_register_command_callback(mesh_command_callback_t callback, void *user_ctx);

/**
 * @brief Регистрация callback для heartbeat
 * 
 * @param callback Функция обработчик
 * @param user_ctx Пользовательский контекст
 * @return ESP_OK при успехе
 */
esp_err_t mesh_register_heartbeat_callback(mesh_heartbeat_callback_t callback, void *user_ctx);

/**
 * @brief Получение количества подключенных peer
 * 
 * @return Количество peer
 */
uint8_t mesh_get_peer_count(void);

/**
 * @brief Получение роли устройства
 * 
 * @return Роль устройства
 */
mesh_role_t mesh_get_role(void);

/**
 * @brief Получение ID устройства
 * 
 * @return ID устройства
 */
uint8_t mesh_get_device_id(void);

/**
 * @brief Проверка подключения к gateway (для slave)
 * 
 * @return true если подключено
 */
bool mesh_is_connected_to_gateway(void);

#ifdef __cplusplus
}
#endif

#endif // MESH_NETWORK_H

