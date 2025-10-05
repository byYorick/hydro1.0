#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Типы записей лога
typedef enum {
    LOG_TYPE_SENSOR_DATA,   // Данные датчиков
    LOG_TYPE_ALARM,         // Тревога
    LOG_TYPE_PUMP_ACTION,   // Действие насоса
    LOG_TYPE_SYSTEM_EVENT,  // Системное событие
    DATA_LOG_USER_ACTION,   // Действие пользователя
    DATA_LOG_SYSTEM_EVENT   // Системное событие (альтернативное)
} log_record_type_t;

// Уровни логирования
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} log_level_t;

// Структура записи лога
typedef struct {
    uint32_t id;
    log_record_type_t type;
    log_level_t level;
    uint32_t timestamp;
    char message[128];
    float ph;
    float ec;
    float temperature;
    float humidity;
    float lux;
    float co2;
} log_entry_t;

// Альтернативная структура записи для совместимости
typedef struct {
    uint32_t id;
    uint32_t timestamp;
    log_record_type_t type;
    uint8_t sensor_type;
    float value;
    char message[128];
} data_logger_entry_t;

// Callback для логирования
typedef void (*data_logger_callback_t)(const data_logger_entry_t *entry);

/**
 * @brief Инициализация системы логирования
 * @param max_entries Максимальное количество записей
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_init(uint32_t max_entries);

/**
 * @brief Логирование данных датчиков
 * @param ph Значение pH
 * @param ec Значение EC
 * @param temp Температура
 * @param hum Влажность
 * @param lux Освещенность
 * @param co2 CO2
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_log_sensor_data(float ph, float ec, float temp, 
                                      float hum, float lux, float co2);

/**
 * @brief Логирование тревоги
 * @param level Уровень
 * @param message Сообщение
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_log_alarm(log_level_t level, const char *message);

/**
 * @brief Логирование действия насоса
 * @param pump_id ID насоса
 * @param duration_ms Длительность в мс
 * @param message Сообщение
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_log_pump_action(uint8_t pump_id, uint32_t duration_ms, 
                                      const char *message);

/**
 * @brief Получение количества записей
 * @return Количество записей
 */
uint32_t data_logger_get_count(void);

/**
 * @brief Очистка всех записей
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_clear(void);

/**
 * @brief Сохранение логов в NVS
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_save_to_nvs(void);

/**
 * @brief Загрузка логов из NVS
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_load_from_nvs(void);

/**
 * @brief Логирование системного события
 * @param level Уровень логирования
 * @param message Сообщение
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_log_system_event(log_level_t level, const char *message);

/**
 * @brief Логирование действия пользователя
 * @param action Действие
 * @param details Детали
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_log_user_action(const char *action, const char *details);

/**
 * @brief Обработка логгера (вызывается в цикле)
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_process(void);

/**
 * @brief Установка callback для логгера
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_set_callback(data_logger_callback_t callback);

/**
 * @brief Установка автоматической очистки
 * @param enabled Включить автоматическую очистку
 * @param days Количество дней для хранения
 * @return ESP_OK при успехе
 */
esp_err_t data_logger_set_auto_cleanup(bool enabled, uint32_t days);

/**
 * @brief Преобразование типа лога в строку
 * @param type Тип лога
 * @return Строковое представление типа
 */
const char* data_logger_type_to_string(log_record_type_t type);

// Константы для совместимости
#define LOG_AUTO_CLEANUP_DAYS 7

#ifdef __cplusplus
}
#endif

#endif // DATA_LOGGER_H

