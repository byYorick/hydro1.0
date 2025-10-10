/**
 * @file sd_storage.h
 * @brief SD-карта хранилище данных для IoT системы
 *
 * Обеспечивает:
 * - Локальное кэширование данных датчиков
 * - Хранение логов событий и алармов
 * - Хранение конфигурации
 * - Синхронизацию с облаком через MQTT
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Режим работы SD-карты
 */
typedef enum {
    SD_MODE_SPI,        ///< SPI режим
    SD_MODE_SDMMC_1BIT, ///< SDMMC 1-битный
    SD_MODE_SDMMC_4BIT  ///< SDMMC 4-битный
} sd_mode_t;

/**
 * @brief Конфигурация SD-карты
 */
typedef struct {
    sd_mode_t mode;          ///< Режим работы
    int mosi_pin;            ///< MOSI пин (для SPI)
    int miso_pin;            ///< MISO пин (для SPI)
    int sck_pin;             ///< SCK пин (для SPI)
    int cs_pin;              ///< CS пин (для SPI)
    uint32_t max_frequency;  ///< Максимальная частота (Hz)
    bool format_if_mount_failed; ///< Форматировать при ошибке монтирования
} sd_storage_config_t;

/**
 * @brief Запись данных датчика
 */
typedef struct {
    time_t timestamp;        ///< Временная метка
    float ph;                ///< pH
    float ec;                ///< EC
    float temperature;       ///< Температура
    float humidity;          ///< Влажность
    float lux;               ///< Освещённость
    uint16_t co2;            ///< CO2
} sd_sensor_record_t;

/**
 * @brief Запись события
 */
typedef struct {
    time_t timestamp;        ///< Временная метка
    char type[32];           ///< Тип события
    char message[128];       ///< Сообщение
    char severity[16];       ///< Уровень важности
} sd_event_record_t;

/**
 * @brief Статистика SD-карты
 */
typedef struct {
    uint64_t total_bytes;    ///< Общий размер (байты)
    uint64_t used_bytes;     ///< Использовано (байты)
    uint64_t free_bytes;     ///< Свободно (байты)
    uint32_t sensor_records; ///< Количество записей датчиков
    uint32_t event_records;  ///< Количество записей событий
} sd_storage_stats_t;

/**
 * @brief Инициализация SD-карты
 * 
 * @param config Конфигурация
 * @return ESP_OK при успехе
 */
esp_err_t sd_storage_init(const sd_storage_config_t *config);

/**
 * @brief Деинициализация SD-карты
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sd_storage_deinit(void);

/**
 * @brief Проверка монтирования SD-карты
 * 
 * @return true если смонтирована
 */
bool sd_storage_is_mounted(void);

/**
 * @brief Запись данных датчика в CSV файл
 * 
 * @param record Запись данных
 * @return ESP_OK при успехе
 */
esp_err_t sd_write_sensor_log(const sd_sensor_record_t *record);

/**
 * @brief Запись события в лог
 * 
 * @param event Запись события
 * @return ESP_OK при успехе
 */
esp_err_t sd_write_event_log(const sd_event_record_t *event);

/**
 * @brief Чтение истории датчиков
 * 
 * @param sensor_name Имя датчика ("ph", "ec", "temp")
 * @param start_time Начальное время
 * @param end_time Конечное время
 * @param records Массив для записей
 * @param max_records Максимальное количество
 * @param count Количество прочитанных записей (выход)
 * @return ESP_OK при успехе
 */
esp_err_t sd_read_sensor_history(const char *sensor_name, time_t start_time, time_t end_time,
                                  sd_sensor_record_t *records, size_t max_records, size_t *count);

/**
 * @brief Чтение событий
 * 
 * @param start_time Начальное время
 * @param end_time Конечное время
 * @param events Массив для событий
 * @param max_events Максимальное количество
 * @param count Количество прочитанных событий (выход)
 * @return ESP_OK при успехе
 */
esp_err_t sd_read_events(time_t start_time, time_t end_time,
                          sd_event_record_t *events, size_t max_events, size_t *count);

/**
 * @brief Получение статистики SD-карты
 * 
 * @param stats Структура для статистики
 * @return ESP_OK при успехе
 */
esp_err_t sd_get_storage_stats(sd_storage_stats_t *stats);

/**
 * @brief Синхронизация данных с облаком через MQTT
 * 
 * Отправляет несинхронизированные данные в MQTT
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sd_sync_to_cloud(void);

/**
 * @brief Очистка старых данных
 * 
 * @param days_to_keep Количество дней для хранения
 * @return ESP_OK при успехе
 */
esp_err_t sd_cleanup_old_data(uint32_t days_to_keep);

/**
 * @brief Сохранение конфигурации в JSON файл
 * 
 * @param config_name Имя конфигурации
 * @param json_data JSON строка с данными
 * @return ESP_OK при успехе
 */
esp_err_t sd_save_config(const char *config_name, const char *json_data);

/**
 * @brief Загрузка конфигурации из JSON файла
 * 
 * @param config_name Имя конфигурации
 * @param json_data Буфер для JSON данных
 * @param max_len Максимальная длина буфера
 * @return ESP_OK при успехе
 */
esp_err_t sd_load_config(const char *config_name, char *json_data, size_t max_len);

/**
 * @brief Экспорт данных в агрегированный формат
 * 
 * Агрегирует данные по часам для отправки в облако
 * 
 * @param start_time Начальное время
 * @param end_time Конечное время
 * @param output_file Файл для экспорта
 * @return ESP_OK при успехе
 */
esp_err_t sd_export_aggregated_data(time_t start_time, time_t end_time, const char *output_file);

/**
 * @brief Форматирование SD-карты
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sd_format(void);

/**
 * @brief Проверка целостности данных
 * 
 * @return ESP_OK если данные целостны
 */
esp_err_t sd_check_integrity(void);

#ifdef __cplusplus
}
#endif

#endif // SD_STORAGE_H

