#ifndef PH_EC_CONTROLLER_H
#define PH_EC_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Индексы насосов определены в system_config.h

// Конфигурация насоса определена в system_config.h

// Типы callback'ов
typedef void (*ph_ec_pump_callback_t)(pump_index_t pump, bool started);
typedef void (*ph_ec_correction_callback_t)(const char *type, float current, float target);

// Параметры коррекции pH
typedef struct {
    float target_ph;            // Целевое значение pH
    float deadband;             // Мертвая зона
    float max_correction_step;  // Максимальный шаг коррекции
    uint32_t correction_interval_ms; // Интервал между коррекциями
} ph_control_params_t;

// Параметры коррекции EC
typedef struct {
    float target_ec;            // Целевое значение EC
    float deadband;             // Мертвая зона
    float max_correction_step;  // Максимальный шаг коррекции
    uint32_t correction_interval_ms; // Интервал между коррекциями
    float ratio_a;              // Соотношение компонента A
    float ratio_b;              // Соотношение компонента B
    float ratio_c;              // Соотношение компонента C
} ec_control_params_t;

/**
 * @brief Инициализация контроллера pH/EC
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_init(void);

/**
 * @brief Установка конфигурации насоса
 * @param pump_idx Индекс насоса
 * @param config Конфигурация
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_pump_config(pump_index_t pump_idx,
                                           const pump_config_t *config);

/**
 * @brief Применение системной конфигурации контроллера
 * @param config Конфигурация системы
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_apply_config(const system_config_t *config);

/**
 * @brief Установка параметров контроля pH
 * @param params Параметры
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_ph_params(const ph_control_params_t *params);

/**
 * @brief Установка параметров контроля EC
 * @param params Параметры
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_ec_params(const ec_control_params_t *params);

/**
 * @brief Выполнение коррекции pH
 * @param current_ph Текущее значение pH
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_correct_ph(float current_ph);

/**
 * @brief Выполнение коррекции EC
 * @param current_ec Текущее значение EC
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_correct_ec(float current_ec);

/**
 * @brief Включение/выключение автоматического режима
 * @param ph_auto Автокоррекция pH
 * @param ec_auto Автокоррекция EC
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_auto_mode(bool ph_auto, bool ec_auto);

/**
 * @brief Получение имени насоса
 * @param pump_idx Индекс насоса
 * @return Имя насоса
 */
const char* ph_ec_controller_get_pump_name(pump_index_t pump_idx);

/**
 * @brief Обновление значений датчиков
 * @param ph Текущее значение pH
 * @param ec Текущее значение EC
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_update_values(float ph, float ec);

/**
 * @brief Обработка контроллера (вызывается в цикле)
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_process(void);

/**
 * @brief Установка callback для событий насосов
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_pump_callback(void (*callback)(pump_index_t pump, bool started));

/**
 * @brief Установка callback для событий коррекции
 * @param callback Функция обратного вызова
 * @return ESP_OK при успехе
 */
esp_err_t ph_ec_controller_set_correction_callback(void (*callback)(const char *type, float current, float target));

#ifdef __cplusplus
}
#endif

#endif // PH_EC_CONTROLLER_H

