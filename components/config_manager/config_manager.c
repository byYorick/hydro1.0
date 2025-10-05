#include "config_manager.h"
#include "esp_log.h"

static const char *TAG = "CONFIG_MANAGER";

esp_err_t config_manager_init(void)
{
    ESP_LOGI(TAG, "Config manager initialized (stub)");
    return ESP_OK;
}

// Загрузка конфигурации (заглушка)
esp_err_t config_load(system_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Config load (stub)");
    return ESP_OK;
}