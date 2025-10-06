#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация менеджера конфигурации.
 *
 * Создает область в NVS для хранения настроек системы и гарантирует, что
 * существует валидная конфигурация по умолчанию. Функция может быть вызвана
 * повторно – инициализация будет выполнена только один раз.
 */
esp_err_t config_manager_init(void);

/**
 * @brief Загрузка конфигурации системы из NVS.
 *
 * Если сохраненных настроек ещё нет, будет возвращена конфигурация по
 * умолчанию и автоматически сохранена во флеше.
 */
esp_err_t config_load(system_config_t *config);

/**
 * @brief Сохранение конфигурации системы в NVS.
 */
esp_err_t config_save(const system_config_t *config);

/**
 * @brief Сброс конфигурации к значениям по умолчанию.
 *
 * @param out_config Если не NULL – сюда будет скопирована конфигурация по
 *                   умолчанию.
 */
esp_err_t config_manager_reset_to_defaults(system_config_t *out_config);

/**
 * @brief Получение последней загруженной конфигурации.
 *
 * Возвращает кэшированную копию конфигурации, если она была загружена через
 * config_load(). Может вернуть NULL, если конфигурация ещё не загружалась.
 */
const system_config_t *config_manager_get_cached(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
