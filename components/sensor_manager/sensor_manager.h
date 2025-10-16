/**
 * @file sensor_manager.h
 * @brief Менеджер датчиков - централизованное управление всеми датчиками
 * 
 * Управляет чтением, кэшированием, валидацией данных всех датчиков:
 * - SHT3x (температура, влажность)
 * - CCS811 (CO2, TVOC)
 * - Trema pH
 * - Trema EC
 * - Trema Lux
 * 
 * @author Hydro Team
 * @date 2025-10-16
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_config.h"  // Используем sensor_data_t из system_config.h

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * ТИПЫ ДАННЫХ
 ******************************************************************************/

/**
 * @brief Типы датчиков
 */
typedef enum {
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_PH,
    SENSOR_TYPE_EC,
    SENSOR_TYPE_LUX,
    SENSOR_TYPE_CO2,
    SENSOR_TYPE_TVOC,
    SENSOR_TYPE_COUNT
} sensor_type_t;

// sensor_data_t уже определен в system_config.h
// Используем существующую структуру с полями:
// - float ph, ec, temperature, humidity, lux, co2
// - uint64_t timestamp
// - bool valid[SENSOR_COUNT]
// - float temp, hum (алиасы)

/**
 * @brief Статистика датчика
 */
typedef struct {
    uint32_t total_reads;       // Всего чтений
    uint32_t successful_reads;  // Успешных чтений
    uint32_t failed_reads;      // Неудачных чтений
    uint64_t last_success_time; // Время последнего успешного чтения
    uint64_t last_failure_time; // Время последней ошибки
    float success_rate;         // Процент успешных чтений
    bool is_healthy;            // Здоров ли датчик
} sensor_stats_t;

/**
 * @brief Калибровочные данные датчика
 */
typedef struct {
    float offset;           // Смещение
    float scale;            // Коэффициент масштабирования
    bool is_calibrated;     // Откалиброван ли
    uint64_t calibration_date; // Дата калибровки
} sensor_calibration_t;

/*******************************************************************************
 * API ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Инициализация менеджера датчиков
 * 
 * Создает мьютексы, инициализирует кэш данных.
 * НЕ инициализирует сами датчики (это делается в app_main).
 * 
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief Чтение всех датчиков
 * 
 * Читает данные со всех датчиков и обновляет кэш.
 * Использует retry логику при ошибках.
 * 
 * @param data Указатель на структуру для данных
 * @return ESP_OK при успехе, ESP_FAIL если все датчики недоступны
 */
esp_err_t sensor_manager_read_all(sensor_data_t *data);

/**
 * @brief Получение кэшированных данных (без чтения)
 * 
 * Возвращает последние считанные данные из кэша.
 * Быстро, не блокирует I2C шину.
 * 
 * @param data Указатель на структуру для данных
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_get_cached_data(sensor_data_t *data);

/**
 * @brief Чтение конкретного датчика pH
 * 
 * @param ph Указатель для значения pH
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_read_ph(float *ph);

/**
 * @brief Чтение конкретного датчика EC
 * 
 * @param ec Указатель для значения EC
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_read_ec(float *ec);

/**
 * @brief Чтение температуры и влажности
 * 
 * @param temperature Указатель для температуры
 * @param humidity Указатель для влажности
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_read_temp_humidity(float *temperature, float *humidity);

/**
 * @brief Чтение освещенности
 * 
 * @param lux Указатель для значения освещенности
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_read_lux(float *lux);

/**
 * @brief Чтение качества воздуха
 * 
 * @param co2 Указатель для CO2 (ppm)
 * @param tvoc Указатель для TVOC (ppb)
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_read_air_quality(float *co2, float *tvoc);

/**
 * @brief Проверка здоровья датчика
 * 
 * Проверяет процент успешных чтений за последнее время.
 * 
 * @param sensor Тип датчика
 * @return true если датчик здоров (>80% успешных чтений)
 */
bool sensor_manager_is_sensor_healthy(sensor_type_t sensor);

/**
 * @brief Получение статистики датчика
 * 
 * @param sensor Тип датчика
 * @param stats Указатель на структуру для статистики
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_get_stats(sensor_type_t sensor, sensor_stats_t *stats);

/**
 * @brief Калибровка датчика pH
 * 
 * Устанавливает offset для коррекции показаний.
 * 
 * @param measured_value Измеренное значение
 * @param actual_value Реальное значение (по эталону)
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_calibrate_ph(float measured_value, float actual_value);

/**
 * @brief Калибровка датчика EC
 * 
 * @param measured_value Измеренное значение
 * @param actual_value Реальное значение (по эталону)
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_calibrate_ec(float measured_value, float actual_value);

/**
 * @brief Получение калибровочных данных
 * 
 * @param sensor Тип датчика
 * @param calibration Указатель на структуру для данных
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_get_calibration(sensor_type_t sensor, sensor_calibration_t *calibration);

/**
 * @brief Сброс статистики датчика
 * 
 * @param sensor Тип датчика
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_reset_stats(sensor_type_t sensor);

/**
 * @brief Получение времени последнего обновления
 * 
 * @return Timestamp в миллисекундах
 */
uint64_t sensor_manager_get_last_update_time(void);

/**
 * @brief Установка интервала автоматического обновления
 * 
 * Если > 0, менеджер будет автоматически обновлять данные.
 * 
 * @param interval_ms Интервал в миллисекундах (0 = отключить)
 * @return ESP_OK при успехе
 */
esp_err_t sensor_manager_set_auto_update_interval(uint32_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_MANAGER_H

