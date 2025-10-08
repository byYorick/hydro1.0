/**
 * @file screen_lifecycle.c
 * @brief Реализация управления жизненным циклом экранов
 */

#include "screen_lifecycle.h"
#include "screen_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SCREEN_LIFECYCLE";

/* =============================
 *  ВНУТРЕННИЕ ФУНКЦИИ
 * ============================= */

/**
 * @brief Найти экземпляр экрана по ID
 */
static screen_instance_t* find_instance_by_id(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    for (int i = 0; i < manager->instance_count; i++) {
        if (manager->instances[i] && 
            manager->instances[i]->config &&
            strcmp(manager->instances[i]->config->id, screen_id) == 0) {
            return manager->instances[i];
        }
    }
    return NULL;
}

/**
 * @brief Получить текущее время в миллисекундах
 */
static uint32_t get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* =============================
 *  СОЗДАНИЕ/УНИЧТОЖЕНИЕ
 * ============================= */

esp_err_t screen_create_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Проверяем, не создан ли уже
    screen_instance_t *existing = find_instance_by_id(screen_id);
    if (existing) {
        ESP_LOGW(TAG, "Screen '%s' already created", screen_id);
        return ESP_OK;  // Не ошибка, просто уже существует
    }
    
    // Получаем конфигурацию из реестра
    screen_config_t *config = screen_get_config(screen_id);
    if (!config) {
        ESP_LOGE(TAG, "Screen '%s' not registered", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Проверяем лимит экземпляров
    if (manager->instance_count >= MAX_INSTANCES) {
        ESP_LOGE(TAG, "Maximum instances reached (%d)", MAX_INSTANCES);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Creating screen '%s'...", screen_id);
    
    // Выделяем память для экземпляра
    screen_instance_t *instance = calloc(1, sizeof(screen_instance_t));
    if (!instance) {
        ESP_LOGE(TAG, "Failed to allocate memory for instance");
        return ESP_ERR_NO_MEM;
    }
    
    instance->config = config;
    instance->create_time = get_time_ms();
    
    // Вызываем функцию создания UI из конфигурации
    instance->screen_obj = config->create_fn(config->user_data);
    if (!instance->screen_obj) {
        ESP_LOGE(TAG, "create_fn failed for screen '%s'", screen_id);
        free(instance);
        return ESP_FAIL;
    }
    
    // Создаем группу энкодера для навигации
    instance->encoder_group = lv_group_create();
    if (!instance->encoder_group) {
        ESP_LOGW(TAG, "Failed to create encoder group for '%s'", screen_id);
        // Не критично, продолжаем
    } else {
        lv_group_set_wrap(instance->encoder_group, true);  // Циклическая навигация
    }
    
    instance->is_created = true;
    instance->is_visible = false;
    instance->is_cached = false;
    
    // Добавляем в массив активных экземпляров
    manager->instances[manager->instance_count] = instance;
    manager->instance_count++;
    
    ESP_LOGI(TAG, "Created screen '%s' (%d/%d instances active)", 
             screen_id, manager->instance_count, MAX_INSTANCES);
    
    return ESP_OK;
}

esp_err_t screen_destroy_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ищем экземпляр
    int index = -1;
    screen_instance_t *instance = NULL;
    
    for (int i = 0; i < manager->instance_count; i++) {
        if (manager->instances[i] && 
            manager->instances[i]->config &&
            strcmp(manager->instances[i]->config->id, screen_id) == 0) {
            index = i;
            instance = manager->instances[i];
            break;
        }
    }
    
    if (!instance) {
        ESP_LOGW(TAG, "Screen '%s' not found for destruction", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Нельзя уничтожить текущий видимый экран
    if (instance == manager->current_screen) {
        ESP_LOGE(TAG, "Cannot destroy current screen '%s'", screen_id);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Destroying screen '%s'...", screen_id);
    
    // Вызываем custom destroy callback если есть
    if (instance->config->destroy_fn) {
        instance->config->destroy_fn(instance->screen_obj);
    }
    
    // Уничтожаем LVGL группу энкодера
    if (instance->encoder_group) {
        lv_group_del(instance->encoder_group);
        instance->encoder_group = NULL;
    }
    
    // Уничтожаем LVGL объект экрана
    if (instance->screen_obj) {
        lv_obj_del(instance->screen_obj);
        instance->screen_obj = NULL;
    }
    
    // Освобождаем параметры показа
    if (instance->show_params) {
        free(instance->show_params);
        instance->show_params = NULL;
    }
    
    // Освобождаем сам экземпляр
    free(instance);
    
    // Сдвигаем массив экземпляров
    for (int i = index; i < manager->instance_count - 1; i++) {
        manager->instances[i] = manager->instances[i + 1];
    }
    manager->instance_count--;
    manager->instances[manager->instance_count] = NULL;
    
    ESP_LOGI(TAG, "Destroyed screen '%s' (%d instances left)", 
             screen_id, manager->instance_count);
    
    return ESP_OK;
}

/* =============================
 *  ПОКАЗ/СКРЫТИЕ
 * ============================= */

esp_err_t screen_show_instance(const char *screen_id, void *params)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Showing screen '%s'...", screen_id);
    
    // Ищем существующий экземпляр
    screen_instance_t *instance = find_instance_by_id(screen_id);
    
    // Если не создан, создаем
    if (!instance) {
        screen_config_t *config = screen_get_config(screen_id);
        if (!config) {
            ESP_LOGE(TAG, "Screen '%s' not registered", screen_id);
            return ESP_ERR_NOT_FOUND;
        }
        
        // Создаем экземпляр (независимо от флага lazy_load)
        ESP_LOGI(TAG, "Creating screen instance '%s' (lazy_load: %d)", 
                 screen_id, config->lazy_load);
        esp_err_t ret = screen_create_instance(screen_id);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create screen '%s': %s", 
                     screen_id, esp_err_to_name(ret));
            return ret;
        }
        
        instance = find_instance_by_id(screen_id);
        if (!instance) {
            ESP_LOGE(TAG, "Failed to find screen '%s' after creation", screen_id);
            return ESP_ERR_NOT_FOUND;
        }
        
        ESP_LOGI(TAG, "Screen instance '%s' created successfully", screen_id);
    }
    
    // Проверка прав/условий показа
    if (instance->config->can_show_fn && 
        !instance->config->can_show_fn()) {
        ESP_LOGW(TAG, "Screen '%s' cannot be shown (can_show_fn returned false)", 
                 screen_id);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // Скрываем текущий экран (если это другой экран)
    if (manager->current_screen && 
        manager->current_screen != instance) {
        esp_err_t ret = screen_hide_instance(manager->current_screen->config->id);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to hide previous screen: %s", 
                     esp_err_to_name(ret));
        }
    }
    
    // Сохраняем параметры показа
    if (params) {
        if (instance->show_params) {
            free(instance->show_params);
        }
        instance->show_params = params;  // Ownership переходит к экземпляру
    }
    
    // Вызываем on_show callback
    if (instance->config->on_show) {
        esp_err_t ret = instance->config->on_show(instance->screen_obj, params);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "on_show callback failed: %s", esp_err_to_name(ret));
        }
    }
    
    // Загружаем экран с анимацией
    ESP_LOGI(TAG, "Loading screen to display...");
    ESP_LOGI(TAG, "  Screen object: %p", instance->screen_obj);
    ESP_LOGI(TAG, "  Animations: %s", manager->config.enable_animations ? "enabled" : "disabled");
    
    if (manager->config.enable_animations) {
        ESP_LOGI(TAG, "  Using lv_scr_load_anim()");
        lv_scr_load_anim(instance->screen_obj, LV_SCR_LOAD_ANIM_MOVE_LEFT, 
                         manager->config.transition_time, 0, false);
    } else {
        ESP_LOGI(TAG, "  Using lv_scr_load()");
        lv_scr_load(instance->screen_obj);
    }
    ESP_LOGI(TAG, "  Screen loaded!");
    
    // Устанавливаем группу энкодера
    if (instance->encoder_group) {
        lv_indev_t *indev = lv_indev_get_next(NULL);
        while (indev) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
                lv_indev_set_group(indev, instance->encoder_group);
                // Устанавливаем фокус на первый элемент группы
                if (lv_group_get_obj_count(instance->encoder_group) > 0) {
                    lv_group_focus_next(instance->encoder_group);
                }
                ESP_LOGD(TAG, "Encoder group set for '%s' (%d objects)", 
                         screen_id, lv_group_get_obj_count(instance->encoder_group));
                break;
            }
            indev = lv_indev_get_next(indev);
        }
    }
    
    // Обновляем состояние
    instance->is_visible = true;
    instance->last_show_time = get_time_ms();
    manager->current_screen = instance;
    
    ESP_LOGI(TAG, "Screen '%s' shown successfully", screen_id);
    
    return ESP_OK;
}

esp_err_t screen_hide_instance(const char *screen_id)
{
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        ESP_LOGW(TAG, "Screen '%s' not found for hiding", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->is_visible) {
        ESP_LOGD(TAG, "Screen '%s' already hidden", screen_id);
        return ESP_OK;  // Уже скрыт
    }
    
    ESP_LOGI(TAG, "Hiding screen '%s'...", screen_id);
    
    // Вызываем on_hide callback
    if (instance->config->on_hide) {
        esp_err_t ret = instance->config->on_hide(instance->screen_obj);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "on_hide callback failed: %s", esp_err_to_name(ret));
        }
    }
    
    instance->is_visible = false;
    
    // Решаем судьбу экрана на основе конфигурации
    if (instance->config->destroy_on_hide) {
        // Уничтожаем сразу для освобождения памяти
        ESP_LOGI(TAG, "Destroying screen '%s' (destroy_on_hide=true)", screen_id);
        return screen_destroy_instance(screen_id);
    } 
    else if (instance->config->cache_on_hide) {
        // Кэшируем для быстрого повторного показа
        instance->is_cached = true;
        instance->cache_time = get_time_ms();
        ESP_LOGI(TAG, "Cached screen '%s' for reuse", screen_id);
    }
    else {
        // Просто скрываем, оставляя в памяти
        ESP_LOGI(TAG, "Hidden screen '%s' (kept in memory)", screen_id);
    }
    
    return ESP_OK;
}

/* =============================
 *  ОБНОВЛЕНИЕ
 * ============================= */

esp_err_t screen_update_instance(const char *screen_id, void *data)
{
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        ESP_LOGW(TAG, "Screen '%s' not found for update", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->config->on_update) {
        ESP_LOGD(TAG, "Screen '%s' has no on_update callback", screen_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ESP_LOGD(TAG, "Updating screen '%s'", screen_id);
    return instance->config->on_update(instance->screen_obj, data);
}

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

screen_instance_t* screen_get_current_instance(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->current_screen;
}

screen_instance_t* screen_get_instance_by_id(const char *screen_id)
{
    return find_instance_by_id(screen_id);
}

bool screen_is_visible(const char *screen_id)
{
    screen_instance_t *instance = find_instance_by_id(screen_id);
    return instance ? instance->is_visible : false;
}

uint8_t screen_get_instance_count(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->instance_count;
}

