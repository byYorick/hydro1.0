#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include "system_config.h"

// Заглушка для config_manager
esp_err_t config_manager_init(void);
esp_err_t config_load(system_config_t *config);
esp_err_t config_save(const system_config_t *config);

#endif // CONFIG_MANAGER_H
