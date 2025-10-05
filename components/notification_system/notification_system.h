#ifndef NOTIFICATION_SYSTEM_H
#define NOTIFICATION_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Типы уведомлений
typedef enum {
    NOTIF_TYPE_INFO,        // Информационное
    NOTIF_TYPE_WARNING,     // Предупреждение
    NOTIF_TYPE_ERROR,       // Ошибка
    NOTIF_TYPE_CRITICAL     // Критическое
} notification_type_t;

// Приоритеты уведомлений
typedef enum {
    NOTIF_PRIORITY_LOW,     // Низкий
    NOTIF_PRIORITY_NORMAL,  // Обычный
    NOTIF_PRIORITY_HIGH,    // Высокий
    NOTIF_PRIORITY_URGENT   // Срочный
} notification_priority_t;

// Источники уведомлений
typedef enum {
    NOTIF_SOURCE_SENSOR,    // Датчик
    NOTIF_SOURCE_PUMP,      // Насос
    NOTIF_SOURCE_RELAY,     // Реле
    NOTIF_SOURCE_SYSTEM     // Система
} notification_source_t;

// Структура уведомления
typedef struct {
    uint32_t id;                        // ID уведомления
    notification_type_t type;           // Тип
    notification_priority_t priority;   // Приоритет
    notification_source_t source;       // Источник
    char message[128];                  // Сообщение
    uint32_t timestamp;                 // Время создания
    bool acknowledged;                  // Подтверждено
} notification_t;

// Callback для обработки уведомлений
typedef void (*notification_callback_t)(const notification_t *notification);

/**
 * @brief Инициализация системы уведомлений
 * @param max_notifications Максимальное количество уведомлений
 * @return ESP_OK при успехе
 */
esp_err_t notification_system_init(uint32_t max_notifications);

/**
 * @brief Создание нового уведомления
 * @param type Тип уведомления
 * @param priority Приоритет
 * @param source Источник
 * @param message Текст сообщения
 * @return ID уведомления или 0 при ошибке
 */
uint32_t notification_create(notification_type_t type, 
                             notification_priority_t priority,
                             notification_source_t source,
                             const char *message);

/**
 * @brief Подтверждение уведомления
 * @param notification_id ID уведомления
 * @return ESP_OK при успехе
 */
esp_err_t notification_acknowledge(uint32_t notification_id);

/**
 * @brief Получение количества непрочитанных уведомлений
 * @return Количество непрочитанных уведомлений
 */
uint32_t notification_get_unread_count(void);

/**
 * @brief Регистрация callback для уведомлений
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t notification_register_callback(notification_callback_t callback);

/**
 * @brief Очистка всех уведомлений
 * @return ESP_OK при успехе
 */
esp_err_t notification_clear_all(void);

/**
 * @brief Создание системного уведомления (упрощенная версия)
 * @param type Тип уведомления
 * @param message Текст сообщения
 * @param source Источник уведомления
 * @return ID уведомления или 0 при ошибке
 */
uint32_t notification_system(notification_type_t type, const char *message, notification_source_t source);

/**
 * @brief Обработка уведомлений (вызывается в цикле)
 * @return ESP_OK при успехе
 */
esp_err_t notification_process(void);

/**
 * @brief Проверка наличия критических уведомлений
 * @return true если есть критические уведомления
 */
bool notification_has_critical(void);

/**
 * @brief Создание предупреждения о датчике
 * @param source Источник уведомления
 * @param current_value Текущее значение
 * @param alarm_low Нижний порог
 * @param alarm_high Верхний порог
 * @return ID уведомления или 0 при ошибке
 */
uint32_t notification_sensor_warning(notification_source_t source, float current_value, float alarm_low, float alarm_high);

/**
 * @brief Установка callback для уведомлений
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t notification_set_callback(notification_callback_t callback);

/**
 * @brief Преобразование типа уведомления в строку
 * @param type Тип уведомления
 * @return Строковое представление типа
 */
const char* notification_type_to_string(notification_type_t type);

// Константы для совместимости
#define NOTIFICATION_INFO NOTIF_TYPE_INFO
#define NOTIFICATION_WARNING NOTIF_TYPE_WARNING
#define NOTIFICATION_ERROR NOTIF_TYPE_ERROR
#define NOTIFICATION_CRITICAL NOTIF_TYPE_CRITICAL
#define NOTIFICATION_SOURCE_PH NOTIF_SOURCE_SENSOR
#define NOTIFICATION_SOURCE_EC NOTIF_SOURCE_SENSOR
#define NOTIFICATION_SOURCE_TEMPERATURE NOTIF_SOURCE_SENSOR
#define NOTIFICATION_SOURCE_HUMIDITY NOTIF_SOURCE_SENSOR
#define NOTIFICATION_SOURCE_LUX NOTIF_SOURCE_SENSOR
#define NOTIFICATION_SOURCE_CO2 NOTIF_SOURCE_SENSOR

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_SYSTEM_H

