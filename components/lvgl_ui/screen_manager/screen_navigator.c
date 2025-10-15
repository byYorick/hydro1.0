/**
 * @file screen_navigator.c
 * @brief Реализация навигации между экранами
 */

#include "screen_navigator.h"
#include "screen_lifecycle.h"
#include "screen_registry.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NAVIGATOR";

/* =============================
 *  УПРАВЛЕНИЕ ИСТОРИЕЙ
 * ============================= */

/**
 * @brief Добавить экран в историю
 */
static esp_err_t push_history(screen_instance_t *instance)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!manager->config.enable_history) {
        return ESP_OK;  // История отключена
    }
    
    if (!instance) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем операции с историей
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for push_history");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Если история заполнена, сдвигаем (FIFO)
    if (manager->history_count >= MAX_HISTORY) {
        ESP_LOGD(TAG, "History full, shifting...");
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            manager->history[i] = manager->history[i + 1];
        }
        manager->history_count = MAX_HISTORY - 1;
    }
    
    // Добавляем в конец
    manager->history[manager->history_count] = instance;
    manager->history_count++;
    manager->history_index = manager->history_count - 1;
    
    ESP_LOGD(TAG, "Pushed '%s' to history (count: %d/%d)", 
             instance->config->id, manager->history_count, MAX_HISTORY);
    
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    return ESP_OK;
}

/**
 * @brief Извлечь экран из истории
 */
static screen_instance_t* pop_history(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем операции с историей
    screen_instance_t *instance = NULL;
    
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for pop_history");
            return NULL;
        }
    }
    
    if (manager->history_count == 0) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGD(TAG, "History is empty");
        return NULL;
    }
    
    // Извлекаем последний элемент (LIFO для back navigation)
    manager->history_count--;
    instance = manager->history[manager->history_count];
    manager->history[manager->history_count] = NULL;
    
    // Обновляем индекс
    if (manager->history_count > 0) {
        manager->history_index = manager->history_count - 1;
    } else {
        manager->history_index = 0;
    }
    
    if (instance) {
        ESP_LOGD(TAG, "Popped '%s' from history (count: %d)", 
                 instance->config->id, manager->history_count);
    }
    
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    return instance;
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ НАВИГАЦИИ
 * ============================= */

esp_err_t navigator_show(const char *screen_id, void *params)
{
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    screen_manager_t *manager = screen_manager_get_instance();
    
    ESP_LOGI(TAG, "Navigating to '%s'", screen_id);
    
    // Сохраняем текущий экран в историю (если есть)
    if (manager->current_screen) {
        push_history(manager->current_screen);
    }
    
    // Показываем новый экран
    esp_err_t ret = screen_show_instance(screen_id, params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show screen '%s': %s", 
                 screen_id, esp_err_to_name(ret));
        // Откатываем историю при ошибке
        pop_history();
        return ret;
    }
    
    ESP_LOGD(TAG, "Navigation to '%s' successful", screen_id);  // УЛУЧШЕНИЕ 3: Оптимизация логов
    return ESP_OK;
}

esp_err_t navigator_go_back(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (manager->history_count == 0) {
        ESP_LOGW(TAG, "Cannot go back: history is empty");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Извлекаем предыдущий экран из истории
    screen_instance_t *prev = pop_history();
    if (!prev) {
        ESP_LOGE(TAG, "Failed to pop from history");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Going back to '%s'", prev->config->id);
    
    // Показываем предыдущий экран (БЕЗ добавления в историю!)
    // ИСПРАВЛЕНО: Не передаем старые show_params, так как они могут быть невалидны
    // При навигации назад params не нужны
    esp_err_t ret = screen_show_instance(prev->config->id, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show previous screen: %s", esp_err_to_name(ret));
        // Восстанавливаем историю при ошибке
        push_history(prev);
        return ret;
    }
    
    ESP_LOGI(TAG, "Back navigation successful");
    return ESP_OK;
}

esp_err_t navigator_go_to_parent(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!manager->current_screen) {
        ESP_LOGW(TAG, "No current screen");
        return ESP_ERR_INVALID_STATE;
    }
    
    screen_config_t *config = manager->current_screen->config;
    
    // Проверяем наличие родителя
    if (!config->can_go_back || strlen(config->parent_id) == 0) {
        ESP_LOGW(TAG, "Screen '%s' has no parent", config->id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ESP_LOGI(TAG, "Going to parent '%s' from '%s'", config->parent_id, config->id);
    
    // Переходим к родительскому экрану
    return navigator_show(config->parent_id, NULL);
}

esp_err_t navigator_go_home(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    ESP_LOGI(TAG, "Going to home screen");
    
    // Ищем корневой экран (is_root=true)
    for (int i = 0; i < manager->screen_count; i++) {
        screen_config_t *config = manager->screens[i];
        if (config && config->is_root) {
            // Очищаем всю историю перед переходом на главный
            navigator_clear_history();
            
            ESP_LOGI(TAG, "Found root screen: '%s'", config->id);
            return screen_show_instance(config->id, NULL);
        }
    }
    
    ESP_LOGE(TAG, "Root screen not found! No screen with is_root=true");
    return ESP_ERR_NOT_FOUND;
}

/* =============================
 *  УТИЛИТЫ ИСТОРИИ
 * ============================= */

uint8_t navigator_get_history_count(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->history_count;
}

void navigator_clear_history(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем очистку истории
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for clear_history");
            return;
        }
    }
    
    manager->history_count = 0;
    manager->history_index = 0;
    memset(manager->history, 0, sizeof(manager->history));
    
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    ESP_LOGI(TAG, "Navigation history cleared");
}

