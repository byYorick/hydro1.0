/**
 * @file screen_registry.c
 * @brief Реализация реестра экранов
 */

#include "screen_registry.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SCREEN_REGISTRY";

/* =============================
 *  SINGLETON МЕНЕДЖЕР
 * ============================= */

// Глобальный менеджер (единственная глобальная переменная во всей системе!)
static screen_manager_t g_manager = {0};

/* =============================
 *  ВНУТРЕННИЕ ФУНКЦИИ
 * ============================= */

/**
 * @brief Найти экран по ID
 */
static screen_config_t* find_screen_by_id(const char *screen_id)
{
    if (!screen_id) {
        return NULL;
    }
    
    for (int i = 0; i < g_manager.screen_count; i++) {
        if (g_manager.screens[i] && 
            strcmp(g_manager.screens[i]->id, screen_id) == 0) {
            return g_manager.screens[i];
        }
    }
    return NULL;
}

/**
 * @brief Проверка валидности ID экрана
 */
static bool is_screen_id_valid(const char *screen_id)
{
    if (!screen_id || strlen(screen_id) == 0) {
        return false;
    }
    if (strlen(screen_id) >= MAX_SCREEN_ID_LEN) {
        return false;
    }
    // Проверяем, что ID содержит только допустимые символы
    for (size_t i = 0; i < strlen(screen_id); i++) {
        char c = screen_id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================= */

esp_err_t screen_registry_init(void)
{
    if (g_manager.is_initialized) {
        ESP_LOGW(TAG, "Registry already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing Screen Registry");
    
    // Обнуляем структуру
    memset(&g_manager, 0, sizeof(screen_manager_t));
    
    // Создаем мьютекс для потокобезопасности
    g_manager.mutex = xSemaphoreCreateMutex();
    if (!g_manager.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Устанавливаем конфигурацию по умолчанию
    g_manager.config.enable_cache = true;
    g_manager.config.enable_history = true;
    g_manager.config.max_cache_size = 5;
    g_manager.config.transition_time = 300;  // 300 мс
    g_manager.config.enable_animations = false;  // Отключаем анимации для отладки
    
    g_manager.is_initialized = true;
    
    ESP_LOGI(TAG, "Screen Registry initialized (max screens: %d, max instances: %d)", 
             MAX_SCREENS, MAX_INSTANCES);
    return ESP_OK;
}

esp_err_t screen_register(const screen_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_screen_id_valid(config->id)) {
        ESP_LOGE(TAG, "Invalid screen ID: '%s'", config->id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!config->create_fn) {
        ESP_LOGE(TAG, "Screen '%s': create_fn is required", config->id);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Проверяем лимит
    if (g_manager.screen_count >= MAX_SCREENS) {
        ESP_LOGE(TAG, "Maximum number of screens reached (%d)", MAX_SCREENS);
        return ESP_ERR_NO_MEM;
    }
    
    // Проверяем дубликаты
    if (find_screen_by_id(config->id)) {
        ESP_LOGE(TAG, "Screen '%s' already registered", config->id);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Блокируем для потокобезопасности
    if (xSemaphoreTake(g_manager.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    // Выделяем память и копируем конфигурацию
    screen_config_t *new_config = malloc(sizeof(screen_config_t));
    if (!new_config) {
        xSemaphoreGive(g_manager.mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for config");
        return ESP_ERR_NO_MEM;
    }
    memcpy(new_config, config, sizeof(screen_config_t));
    
    // Добавляем в реестр
    g_manager.screens[g_manager.screen_count] = new_config;
    g_manager.screen_count++;
    
    xSemaphoreGive(g_manager.mutex);
    
    ESP_LOGI(TAG, "Registered screen '%s' (category: %d, lazy_load: %d)", 
             config->id, config->category, config->lazy_load);
    return ESP_OK;
}

esp_err_t screen_unregister(const char *screen_id)
{
    if (!is_screen_id_valid(screen_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Блокируем
    if (xSemaphoreTake(g_manager.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Ищем экран
    int index = -1;
    for (int i = 0; i < g_manager.screen_count; i++) {
        if (g_manager.screens[i] && 
            strcmp(g_manager.screens[i]->id, screen_id) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        xSemaphoreGive(g_manager.mutex);
        ESP_LOGW(TAG, "Screen '%s' not found", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Освобождаем память конфигурации
    free(g_manager.screens[index]);
    
    // Сдвигаем массив
    for (int i = index; i < g_manager.screen_count - 1; i++) {
        g_manager.screens[i] = g_manager.screens[i + 1];
    }
    g_manager.screen_count--;
    g_manager.screens[g_manager.screen_count] = NULL;
    
    xSemaphoreGive(g_manager.mutex);
    
    ESP_LOGI(TAG, "Unregistered screen '%s' (%d screens left)", 
             screen_id, g_manager.screen_count);
    return ESP_OK;
}

screen_config_t* screen_get_config(const char *screen_id)
{
    if (!is_screen_id_valid(screen_id)) {
        return NULL;
    }
    
    return find_screen_by_id(screen_id);
}

uint8_t screen_get_registered_count(void)
{
    return g_manager.screen_count;
}

screen_manager_t* screen_manager_get_instance(void)
{
    return &g_manager;
}

