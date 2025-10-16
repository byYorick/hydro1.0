/**
 * @file network_manager.h
 * @brief Network Manager - управление WiFi подключением
 * 
 * Менеджер сетевых функций для Hydro System 1.0
 * - Подключение к WiFi
 * - Сканирование сетей
 * - Получение статуса подключения
 * - Автопереподключение
 * - Сохранение настроек в NVS
 * 
 * @author Hydroponics Team
 * @date 2025-10-16
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  КОНСТАНТЫ
 * ============================= */

#define MAX_WIFI_SSID_LEN       32
#define MAX_WIFI_PASSWORD_LEN   64
#define MAX_SCAN_RESULTS        20

/* =============================
 *  ТИПЫ ДАННЫХ
 * ============================= */

/**
 * @brief Статус WiFi подключения
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,  ///< Отключено
    WIFI_STATUS_CONNECTING,        ///< Подключение...
    WIFI_STATUS_CONNECTED,         ///< Подключено
    WIFI_STATUS_ERROR              ///< Ошибка
} wifi_status_t;

/**
 * @brief Информация о WiFi подключении
 */
typedef struct {
    wifi_status_t status;       ///< Текущий статус
    int8_t rssi;                ///< Уровень сигнала (dBm)
    char ssid[33];              ///< Текущая сеть
    char ip[16];                ///< IP адрес
    char gateway[16];           ///< Шлюз
    char netmask[16];           ///< Маска сети
    uint32_t reconnect_count;   ///< Количество переподключений
    bool is_connected;          ///< Флаг подключения
} wifi_info_t;

/**
 * @brief Результат сканирования WiFi
 */
typedef struct {
    char ssid[33];              ///< SSID сети
    int8_t rssi;                ///< Уровень сигнала
    wifi_auth_mode_t authmode;  ///< Тип защиты
    uint8_t channel;            ///< Канал
} wifi_scan_result_t;

/* =============================
 *  ПУБЛИЧНЫЙ API
 * ============================= */

/**
 * @brief Инициализация network manager
 * 
 * Инициализирует WiFi, создает event loop, регистрирует обработчики
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_init(void);

/**
 * @brief Деинициализация network manager
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_deinit(void);

/**
 * @brief Подключение к WiFi сети
 * 
 * @param ssid SSID сети
 * @param password Пароль (может быть NULL для открытых сетей)
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_connect(const char *ssid, const char *password);

/**
 * @brief Отключение от WiFi
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_disconnect(void);

/**
 * @brief Сканирование доступных WiFi сетей
 * 
 * @param results Массив для записи результатов
 * @param max_results Максимальное количество результатов
 * @param actual_count Указатель для записи реального количества найденных сетей
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_scan(wifi_scan_result_t *results, uint16_t max_results, uint16_t *actual_count);

/**
 * @brief Получение информации о WiFi подключении
 * 
 * @param info Указатель на структуру для записи информации
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_info(wifi_info_t *info);

/**
 * @brief Проверка подключения к WiFi
 * 
 * @return true если подключено
 */
bool network_manager_is_connected(void);

/**
 * @brief Сохранение текущей WiFi конфигурации в NVS
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_save_credentials(void);

/**
 * @brief Загрузка WiFi конфигурации из NVS и автоподключение
 * 
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_load_and_connect(void);

/**
 * @brief Получение MAC адреса
 * 
 * @param mac_str Буфер для записи MAC (формат XX:XX:XX:XX:XX:XX)
 * @param len Размер буфера (минимум 18 байт)
 * @return ESP_OK при успехе
 */
esp_err_t network_manager_get_mac(char *mac_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_MANAGER_H
