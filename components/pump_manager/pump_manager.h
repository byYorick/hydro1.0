/**
 * @file pump_manager.h
 * @brief Менеджер управления перистальтическими насосами
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include "esp_err.h"
#include "system_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Статус насоса
 */
typedef enum {
    PUMP_STATUS_IDLE,       ///< Простой
    PUMP_STATUS_RUNNING,    ///< Работает
    PUMP_STATUS_COOLDOWN,   ///< Ожидание cooldown
    PUMP_STATUS_ERROR       ///< Ошибка
} pump_status_t;

/**
 * @brief Статистика насоса
 */
typedef struct {
    pump_status_t status;           ///< Текущий статус
    uint32_t total_doses;           ///< Всего доз
    float total_ml_dispensed;       ///< Всего мл дозировано
    uint32_t last_dose_timestamp;   ///< Время последней дозы (сек)
    uint32_t doses_in_last_hour;    ///< Доз в последний час
    uint32_t error_count;           ///< Количество ошибок
    uint32_t total_runtime_sec;     ///< Общее время работы
} pump_stats_t;

/**
 * @brief Инициализация менеджера насосов
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_init(void);

/**
 * @brief Дозирование жидкости насосом
 * 
 * @param pump_idx Индекс насоса
 * @param volume_ml Объем в мл
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_dose(pump_index_t pump_idx, float volume_ml);

/**
 * @brief Получение статуса насоса
 * 
 * @param pump_idx Индекс насоса
 * @return Статус насоса
 */
pump_status_t pump_manager_get_status(pump_index_t pump_idx);

/**
 * @brief Получение статистики насоса
 * 
 * @param pump_idx Индекс насоса
 * @param stats Указатель на структуру статистики
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_get_stats(pump_index_t pump_idx, pump_stats_t *stats);

/**
 * @brief Сброс статистики насоса
 * 
 * @param pump_idx Индекс насоса
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_reset_stats(pump_index_t pump_idx);

/**
 * @brief Экстренная остановка всех насосов
 * 
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_emergency_stop(void);

/**
 * @brief Проверка может ли насос работать (cooldown, лимиты)
 * 
 * @param pump_idx Индекс насоса
 * @param volume_ml Планируемый объем
 * @return true если можно дозировать
 */
bool pump_manager_can_dose(pump_index_t pump_idx, float volume_ml);

/**
 * @brief Тестовый запуск насоса
 * 
 * @param pump_idx Индекс насоса
 * @param duration_ms Длительность (мс)
 * @return ESP_OK при успехе
 */
esp_err_t pump_manager_test(pump_index_t pump_idx, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // PUMP_MANAGER_H

