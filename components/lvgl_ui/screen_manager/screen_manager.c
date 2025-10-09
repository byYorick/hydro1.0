/**
 * @file screen_manager.c
 * @brief Реализация главного API Screen Manager
 */

#include "screen_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SCREEN_MANAGER";

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================= */

esp_err_t screen_manager_init(const screen_manager_config_t *config)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "   Screen Manager Initialization        ");
    ESP_LOGI(TAG, "==============================================");
    
    // Инициализируем реестр
    esp_err_t ret = screen_registry_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init registry: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Применяем пользовательскую конфигурацию если передана
    if (config) {
        screen_manager_t *manager = screen_manager_get_instance();
        memcpy(&manager->config, config, sizeof(screen_manager_config_t));
        ESP_LOGI(TAG, "Applied custom configuration");
        ESP_LOGI(TAG, "  - Cache: %s", config->enable_cache ? "ON" : "OFF");
        ESP_LOGI(TAG, "  - History: %s", config->enable_history ? "ON" : "OFF");
        ESP_LOGI(TAG, "  - Animations: %s", config->enable_animations ? "ON" : "OFF");
        ESP_LOGI(TAG, "  - Transition time: %lu ms", (unsigned long)config->transition_time);
    } else {
        ESP_LOGI(TAG, "Using default configuration");
    }
    
    ESP_LOGI(TAG, "Screen Manager initialized successfully");
    ESP_LOGI(TAG, "Ready to register screens...");
    
    return ESP_OK;
}

esp_err_t screen_manager_deinit(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    ESP_LOGI(TAG, "Deinitializing Screen Manager...");
    
    // Уничтожаем все активные экземпляры
    ESP_LOGI(TAG, "Destroying %d active instances", manager->instance_count);
    while (manager->instance_count > 0) {
        screen_instance_t *inst = manager->instances[0];
        if (inst && inst->config) {
            screen_destroy_instance(inst->config->id);
        } else {
            // Если конфиг NULL, просто удаляем из массива
            manager->instance_count--;
        }
    }
    
    // Освобождаем все конфигурации из реестра
    ESP_LOGI(TAG, "Unregistering %d screens", manager->screen_count);
    for (int i = 0; i < manager->screen_count; i++) {
        if (manager->screens[i]) {
            free(manager->screens[i]);
            manager->screens[i] = NULL;
        }
    }
    manager->screen_count = 0;
    
    // Очищаем историю
    navigator_clear_history();
    
    // Удаляем мьютекс
    if (manager->mutex) {
        vSemaphoreDelete(manager->mutex);
        manager->mutex = NULL;
    }
    
    // Обнуляем структуру
    memset(manager, 0, sizeof(screen_manager_t));
    
    ESP_LOGI(TAG, "Screen Manager deinitialized");
    return ESP_OK;
}

/* =============================
 *  НАВИГАЦИЯ (Wrappers для упрощения)
 * ============================= */

esp_err_t screen_show(const char *screen_id, void *params)
{
    ESP_LOGD(TAG, "screen_show() -> navigator_show()");
    return navigator_show(screen_id, params);
}

esp_err_t screen_hide(const char *screen_id)
{
    ESP_LOGD(TAG, "screen_hide() -> screen_hide_instance()");
    return screen_hide_instance(screen_id);
}

esp_err_t screen_go_back(void)
{
    ESP_LOGD(TAG, "screen_go_back() -> navigator_go_back()");
    return navigator_go_back();
}

esp_err_t screen_go_to_parent(void)
{
    ESP_LOGD(TAG, "screen_go_to_parent() -> navigator_go_to_parent()");
    return navigator_go_to_parent();
}

esp_err_t screen_go_home(void)
{
    ESP_LOGD(TAG, "screen_go_home() -> navigator_go_home()");
    return navigator_go_home();
}

esp_err_t screen_update(const char *screen_id, void *data)
{
    ESP_LOGD(TAG, "screen_update() -> screen_update_instance()");
    return screen_update_instance(screen_id, data);
}

/* =============================
 *  УПРАВЛЕНИЕ ЖИЗНЕННЫМ ЦИКЛОМ
 * ============================= */

esp_err_t screen_create(const char *screen_id)
{
    ESP_LOGD(TAG, "screen_create() -> screen_create_instance()");
    return screen_create_instance(screen_id);
}

esp_err_t screen_destroy(const char *screen_id)
{
    ESP_LOGD(TAG, "screen_destroy() -> screen_destroy_instance()");
    return screen_destroy_instance(screen_id);
}

esp_err_t screen_reload(const char *screen_id)
{
    ESP_LOGI(TAG, "Reloading screen '%s'", screen_id);
    
    // Уничтожаем если существует
    esp_err_t ret = screen_destroy_instance(screen_id);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to destroy for reload: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Создаем заново
    ret = screen_create_instance(screen_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to recreate: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Screen '%s' reloaded", screen_id);
    return ESP_OK;
}

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

screen_instance_t* screen_get_current(void)
{
    return screen_get_current_instance();
}

screen_instance_t* screen_get_by_id(const char *screen_id)
{
    return screen_get_instance_by_id(screen_id);
}

bool screen_is_visible_check(const char *screen_id)
{
    return screen_is_visible(screen_id);
}

uint8_t screen_get_history_count(void)
{
    return navigator_get_history_count();
}

/* =============================
 *  УПРАВЛЕНИЕ ГРУППОЙ ЭНКОДЕРА
 * ============================= */

esp_err_t screen_add_to_group(const char *screen_id, lv_obj_t *widget)
{
    ESP_LOGD(TAG, "screen_add_to_group() -> screen_add_to_encoder_group()");
    return screen_add_to_encoder_group(screen_id, widget);
}

// Функция screen_add_widget_tree реализована в screen_lifecycle.c

int screen_cleanup_hidden_elements(const char *screen_id)
{
    screen_instance_t *instance = NULL;
    
    if (screen_id) {
        instance = screen_get_by_id(screen_id);
    } else {
        instance = screen_get_current();
    }
    
    if (!instance || !instance->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available for cleanup");
        return 0;
    }
    
    // Используем внутреннюю функцию очистки
    extern int cleanup_hidden_elements(lv_group_t *group);
    return cleanup_hidden_elements(instance->encoder_group);
}

