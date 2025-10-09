/**
 * @file network_manager.h
 * @brief Заголовочный файл менеджера сетевых функций ESP32S3
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include "esp_http_server.h"
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
 * @brief Статусы сетевого подключения
 */
typedef enum {
    NETWORK_STATUS_DISCONNECTED = 0,  ///< Отключено
    NETWORK_STATUS_CONNECTING,        ///< Подключение
    NETWORK_STATUS_CONNECTED,         ///< Подключено
    NETWORK_STATUS_AP_MODE,           ///< Режим точки доступа
    NETWORK_STATUS_ERROR              ///< Ошибка
} network_status_t;

/**
 * @brief Конфигурация WiFi станции
 */
typedef struct {
    char ssid[32];           ///< SSID сети
    char password[64];       ///< Пароль сети
    uint8_t channel;         ///< Канал (0 - автовыбор)
    bool auto_reconnect;     ///< Автоматическое переподключение
} network_wifi_config_t;

/**
 * @brief Конфигурация точки доступа
 */
typedef struct {
    char ssid[32];           ///< SSID точки доступа
    char password[64];       ///< Пароль точки доступа
    uint8_t channel;         ///< Канал WiFi
    uint8_t max_connection;  ///< Максимум подключений
    bool ssid_hidden;        ///< Скрытая сеть
} ap_config_t;

/**
 * @brief Статистика сети
 */
typedef struct {
    uint32_t packets_sent;       ///< Отправлено пакетов
    uint32_t packets_received;   ///< Получено пакетов
    uint32_t bytes_sent;         ///< Отправлено байт
    uint32_t bytes_received;     ///< Получено байт
    uint32_t wifi_reconnects;    ///< Количество переподключений WiFi
    int8_t rssi;                 ///< Уровень сигнала (dBm)
    uint32_t uptime_seconds;     ///< Время работы (секунды)
} network_stats_t;

/**
 * @brief Обработчик HTTP запросов
 * 
 * @param ctx Контекст пользователя
 */
typedef void (*http_handler_func_t)(void *ctx);

/**
 * @brief Обработчик BLE событий
 * 
 * @param ctx Контекст пользователя
 */
typedef void (*ble_handler_func_t)(void *ctx);

/**
 * @brief Инициализация сетевого менеджера
 * 
 * @param mode Режим сетевого подключения
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_init(network_mode_t mode);

/**
 * @brief Деинициализация сетевого менеджера
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_deinit(void);

/**
 * @brief Подключение к WiFi сети
 * 
 * @param config Конфигурация WiFi
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_connect_wifi(const network_wifi_config_t *config);

/**
 * @brief Запуск точки доступа
 * 
 * @param config Конфигурация точки доступа
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_start_ap(const ap_config_t *config);

/**
 * @brief Отключение от WiFi сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_disconnect_wifi(void);

/**
 * @brief Получение статистики сети
 * 
 * @param stats Указатель на структуру для записи статистики
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_stats(network_stats_t *stats);

/**
 * @brief Сканирование доступных WiFi сетей
 * 
 * @param networks Массив для записи имён сетей
 * @param max_networks Максимальное количество сетей
 * @return Количество найденных сетей
 */
int network_manager_scan_wifi(char (*networks)[32], int max_networks);

/**
 * @brief Проверка доступности интернета
 * 
 * @return true если интернет доступен
 */
bool network_manager_is_internet_available(void);

/**
 * @brief Синхронизация времени через NTP
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_sync_time(void);

/**
 * @brief Получение текущего времени
 * 
 * @param timestamp Указатель для записи временной метки (Unix timestamp)
 * @param time_str Буфер для записи времени (формат HH:MM:SS), может быть NULL
 * @param date_str Буфер для записи даты (формат YYYY-MM-DD), может быть NULL
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_time(uint32_t *timestamp, char *time_str, char *date_str);

/**
 * @brief Получение IP адреса
 * 
 * @param ip_str Буфер для записи IP адреса
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_ip(char *ip_str);

/**
 * @brief Получение MAC адреса
 * 
 * @param mac_str Буфер для записи MAC адреса
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_mac(char *mac_str);

/**
 * @brief Сохранение конфигурации сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_save_config(void);

/**
 * @brief Загрузка конфигурации сети
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_load_config(void);

/**
 * @brief Сброс конфигурации сети к значениям по умолчанию
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_reset_config(void);

/**
 * @brief Запуск HTTP сервера
 * 
 * @param port Порт сервера
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_start_http_server(uint16_t port);

/**
 * @brief Остановка HTTP сервера
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_stop_http_server(void);

/**
 * @brief Регистрация обработчика HTTP запросов
 * 
 * @param uri URI пути
 * @param method HTTP метод (GET, POST, и т.д.)
 * @param handler Функция-обработчик
 * @param user_ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_register_http_handler(const char *uri, const char *method,
                                               http_handler_func_t handler, void *user_ctx);

/**
 * @brief Запуск mDNS сервиса
 * 
 * @param hostname Имя хоста
 * @param service_name Имя сервиса
 * @param port Порт сервиса
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_start_mdns(const char *hostname, const char *service_name, uint16_t port);

/**
 * @brief Остановка mDNS сервиса
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_stop_mdns(void);

/**
 * @brief Запуск BLE сервиса
 * 
 * @param device_name Имя BLE устройства
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_start_ble(const char *device_name);

/**
 * @brief Остановка BLE сервиса
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_stop_ble(void);

/**
 * @brief Проверка наличия обновления по OTA
 * 
 * @param current_version Текущая версия прошивки
 * @return true если доступно обновление
 */
bool network_manager_check_ota_update(const char *current_version);

/**
 * @brief Запуск обновления по OTA
 * 
 * @param url URL файла обновления
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_start_ota_update(const char *url);

/**
 * @brief Регистрация обработчика BLE событий
 * 
 * @param event Имя события
 * @param handler Функция-обработчик
 * @param user_ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_register_ble_handler(const char *event,
                                               ble_handler_func_t handler, void *user_ctx);

/**
 * @brief Отправка данных через BLE
 * 
 * @param data Указатель на данные
 * @param length Длина данных
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_send_ble(const uint8_t *data, size_t length);

/**
 * @brief Регистрация обработчика WebSocket событий
 * 
 * @param event Имя события
 * @param handler Функция-обработчик
 * @param user_ctx Контекст пользователя
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_register_ws_handler(const char *event,
                                              http_handler_func_t handler, void *user_ctx);

/**
 * @brief Отправка данных через WebSocket
 * 
 * @param data Указатель на данные
 * @param length Длина данных
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_send_websocket(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_MANAGER_H

