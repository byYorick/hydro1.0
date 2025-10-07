/**
 * @file error_handler.h
 * @brief Централизованная система обработки ошибок с уведомлениями на экран
 * 
 * Этот компонент обрабатывает все ошибки системы и выводит их через:
 * - Систему уведомлений (notification_system)
 * - Всплывающие окна на экране LVGL
 * - ESP_LOG для отладки
 * 
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Категории ошибок
 */
typedef enum {
    ERROR_CATEGORY_I2C,         ///< Ошибки I2C шины
    ERROR_CATEGORY_SENSOR,      ///< Ошибки датчиков
    ERROR_CATEGORY_DISPLAY,     ///< Ошибки дисплея
    ERROR_CATEGORY_STORAGE,     ///< Ошибки хранилища (NVS)
    ERROR_CATEGORY_SYSTEM,      ///< Системные ошибки
    ERROR_CATEGORY_PUMP,        ///< Ошибки насосов
    ERROR_CATEGORY_RELAY,       ///< Ошибки реле
    ERROR_CATEGORY_CONTROLLER,  ///< Ошибки контроллеров
    ERROR_CATEGORY_NETWORK,     ///< Сетевые ошибки (если включены)
    ERROR_CATEGORY_OTHER        ///< Прочие ошибки
} error_category_t;

/**
 * @brief Уровни критичности ошибок
 */
typedef enum {
    ERROR_LEVEL_DEBUG,      ///< Отладочная информация
    ERROR_LEVEL_INFO,       ///< Информация
    ERROR_LEVEL_WARNING,    ///< Предупреждение
    ERROR_LEVEL_ERROR,      ///< Ошибка
    ERROR_LEVEL_CRITICAL    ///< Критическая ошибка
} error_level_t;

/**
 * @brief Структура описания ошибки
 */
typedef struct {
    error_category_t category;  ///< Категория ошибки
    error_level_t level;        ///< Уровень критичности
    esp_err_t code;             ///< Код ошибки ESP-IDF
    char message[128];          ///< Описание ошибки
    uint32_t timestamp;         ///< Время возникновения
    char component[32];         ///< Имя компонента
} error_info_t;

/**
 * @brief Callback для обработки ошибок
 * 
 * @param error Информация об ошибке
 */
typedef void (*error_callback_t)(const error_info_t *error);

/**
 * @brief Инициализация системы обработки ошибок
 * 
 * @param show_popup Показывать ли всплывающие окна на экране
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_init(bool show_popup);

/**
 * @brief Регистрация ошибки
 * 
 * @param category Категория ошибки
 * @param level Уровень критичности
 * @param code Код ошибки ESP-IDF
 * @param component Имя компонента
 * @param format Форматная строка сообщения (как в printf)
 * @param ... Аргументы для форматной строки
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_report(error_category_t category, 
                               error_level_t level,
                               esp_err_t code,
                               const char *component,
                               const char *format, ...);

/**
 * @brief Регистрация callback для обработки ошибок
 * 
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_register_callback(error_callback_t callback);

/**
 * @brief Включение/выключение всплывающих окон
 * 
 * @param enable true для включения
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_set_popup(bool enable);

/**
 * @brief Установка шрифта для всплывающих окон
 * 
 * @param font Указатель на шрифт LVGL (должен поддерживать кириллицу)
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_set_font(const void *font);

/**
 * @brief Получение статистики ошибок
 * 
 * @param total Общее количество ошибок
 * @param critical Количество критических ошибок
 * @param errors Количество обычных ошибок
 * @param warnings Количество предупреждений
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_get_stats(uint32_t *total, uint32_t *critical, 
                                  uint32_t *errors, uint32_t *warnings);

/**
 * @brief Очистка статистики ошибок
 * 
 * @return ESP_OK при успехе
 */
esp_err_t error_handler_clear_stats(void);

/**
 * @brief Преобразование категории ошибки в строку
 * 
 * @param category Категория ошибки
 * @return Строковое представление
 */
const char* error_category_to_string(error_category_t category);

/**
 * @brief Преобразование уровня ошибки в строку
 * 
 * @param level Уровень ошибки
 * @return Строковое представление
 */
const char* error_level_to_string(error_level_t level);

// Макросы для упрощения вызова error_handler_report

/**
 * @brief Макрос для отладочной информации
 */
#define ERROR_DEBUG(component, format, ...) \
    error_handler_report(ERROR_CATEGORY_SYSTEM, ERROR_LEVEL_DEBUG, ESP_OK, component, format, ##__VA_ARGS__)

/**
 * @brief Макрос для информационных сообщений
 */
#define ERROR_INFO(component, format, ...) \
    error_handler_report(ERROR_CATEGORY_SYSTEM, ERROR_LEVEL_INFO, ESP_OK, component, format, ##__VA_ARGS__)

/**
 * @brief Макрос для предупреждений
 */
#define ERROR_WARN(category, component, format, ...) \
    error_handler_report(category, ERROR_LEVEL_WARNING, ESP_OK, component, format, ##__VA_ARGS__)

/**
 * @brief Макрос для ошибок
 */
#define ERROR_REPORT(category, code, component, format, ...) \
    error_handler_report(category, ERROR_LEVEL_ERROR, code, component, format, ##__VA_ARGS__)

/**
 * @brief Макрос для критических ошибок
 */
#define ERROR_CRITICAL(category, code, component, format, ...) \
    error_handler_report(category, ERROR_LEVEL_CRITICAL, code, component, format, ##__VA_ARGS__)

/**
 * @brief Макрос для проверки и обработки ошибок I2C
 */
#define ERROR_CHECK_I2C(err, component, format, ...) \
    do { \
        if ((err) != ESP_OK) { \
            error_handler_report(ERROR_CATEGORY_I2C, ERROR_LEVEL_ERROR, err, component, format, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Макрос для проверки и обработки ошибок датчиков
 */
#define ERROR_CHECK_SENSOR(err, component, format, ...) \
    do { \
        if ((err) != ESP_OK) { \
            error_handler_report(ERROR_CATEGORY_SENSOR, ERROR_LEVEL_ERROR, err, component, format, ##__VA_ARGS__); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // ERROR_HANDLER_H

