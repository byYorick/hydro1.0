/**
 * @file screen_lifecycle.c
 * @brief Реализация управления жизненным циклом экранов
 */

#include "screen_lifecycle.h"
#include "screen_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SCREEN_LIFECYCLE";

/* =============================
 *  ВНУТРЕННИЕ ФУНКЦИИ
 * ============================= */

/**
 * @brief Проверить, является ли объект интерактивным элементом
 */
static bool is_interactive_element(lv_obj_t *obj)
{
    if (!obj) return false;
    
    // Проверяем, что элемент не скрыт
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }
    
    if (lv_obj_check_type(obj, &lv_label_class)) {
        return false;
    }
    
    if (lv_obj_check_type(obj, &lv_button_class) || 
        lv_obj_check_type(obj, &lv_buttonmatrix_class)) {
        return true;
    }
    
    uint32_t event_count = lv_obj_get_event_count(obj);
    if (event_count == 0) {
        return false;
    }
    
    if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
        return false;
    }
    
    lv_coord_t width = lv_obj_get_width(obj);
    lv_coord_t height = lv_obj_get_height(obj);
    
    if (width < 30 || height < 20) {
        return false;
    }
    
    return true;
}

// Предварительные объявления функций
int screen_lifecycle_add_interactive_iterative(lv_obj_t *root_obj, lv_group_t *group, int max_depth);
int cleanup_hidden_elements(lv_group_t *group);
int screen_lifecycle_add_main_screen_elements(lv_obj_t *screen_obj, lv_group_t *group);


/**
 * @brief Добавить интерактивные элементы в группу энкодера
 */
int screen_lifecycle_add_interactive_iterative(lv_obj_t *root_obj, lv_group_t *group, int max_depth)
{
    if (!root_obj || !group) return 0;
    
    int added = 0;
    
    // УЛУЧШЕНИЕ 2: Увеличенная очередь для сложных экранов (tabview, графики)
    #define MAX_WIDGET_QUEUE 200  // Было 100, увеличено вдвое
    lv_obj_t *queue[MAX_WIDGET_QUEUE];
    int queue_head = 0;
    int queue_tail = 0;
    
    // Добавляем детей корневого объекта в очередь
    uint32_t child_count = lv_obj_get_child_count(root_obj);
    for (uint32_t i = 0; i < child_count && queue_tail < MAX_WIDGET_QUEUE; i++) {
        lv_obj_t *child = lv_obj_get_child(root_obj, i);
        if (child) {
            queue[queue_tail++] = child;
        }
    }
    
    // Обрабатываем очередь
    while (queue_head < queue_tail) {
        lv_obj_t *obj = queue[queue_head++];
        
        // Проверяем текущий объект
        if (is_interactive_element(obj)) {
            lv_group_add_obj(group, obj);
            added++;
            ESP_LOGD(TAG, "Added element %p to encoder group", obj);
        }
        
        // Добавляем детей в очередь
        child_count = lv_obj_get_child_count(obj);
        for (uint32_t i = 0; i < child_count && queue_tail < MAX_WIDGET_QUEUE; i++) {
            lv_obj_t *child = lv_obj_get_child(obj, i);
            if (child) {
                queue[queue_tail++] = child;
            }
        }
    }
    
    #undef MAX_WIDGET_QUEUE
    
    return added;
}

/**
 * @brief Очистить группу энкодера от скрытых элементов
 * 
 * Улучшенная версия с дополнительными проверками:
 * - Проверка флага HIDDEN
 * - Проверка валидности объекта
 * - Проверка скрытых родителей
 */
int cleanup_hidden_elements(lv_group_t *group)
{
    if (!group) return 0;
    
    int removed = 0;
    uint32_t obj_count = lv_group_get_obj_count(group);
    
    // Проходим в обратном порядке для безопасного удаления
    for (int i = obj_count - 1; i >= 0; i--) {
        lv_obj_t *obj = lv_group_get_obj_by_index(group, i);
        if (!obj) continue;
        
        bool should_remove = false;
        
        // Проверка 1: Элемент скрыт
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
            should_remove = true;
            ESP_LOGD(TAG, "Element %p is hidden", obj);
        }
        
        // Проверка 2: Элемент удален (invalid)
        else if (!lv_obj_is_valid(obj)) {
            should_remove = true;
            ESP_LOGD(TAG, "Element %p is invalid", obj);
        }
        
        // Проверка 3: Родитель скрыт
        else {
            lv_obj_t *parent = obj;
            while ((parent = lv_obj_get_parent(parent)) != NULL) {
                if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) {
                    should_remove = true;
                    ESP_LOGD(TAG, "Element %p has hidden parent", obj);
                    break;
                }
            }
        }
        
        if (should_remove) {
            lv_group_remove_obj(obj);
            removed++;
        }
    }
    
    if (removed > 0) {
        ESP_LOGI(TAG, "Removed %d hidden/invalid elements from encoder group", removed);
    }
    
    return removed;
}

/**
 * @brief Найти экземпляр экрана по ID
 * 
 * ВНИМАНИЕ: Эта функция НЕ берет mutex!
 * Вызывающая функция ДОЛЖНА обеспечить thread safety!
 */
static screen_instance_t* find_instance_by_id(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // ВНИМАНИЕ: Доступ к manager->instances[] БЕЗ ЗАЩИТЫ!
    // Вызывающий код должен держать mutex!
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
    
    // ИСПРАВЛЕНО: Добавляем thread safety
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for create_instance");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Проверяем, не создан ли уже
    screen_instance_t *existing = find_instance_by_id(screen_id);
    if (existing) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGW(TAG, "Screen '%s' already created", screen_id);
        return ESP_OK;  // Не ошибка, просто уже существует
    }
    
    // Получаем конфигурацию из реестра
    screen_config_t *config = screen_get_config(screen_id);
    if (!config) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Screen '%s' not registered", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Проверяем лимит экземпляров
    if (manager->instance_count >= MAX_INSTANCES) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Maximum instances reached (%d)", MAX_INSTANCES);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Creating screen '%s'...", screen_id);
    
    // Освобождаем mutex ПЕРЕД созданием UI объекта (долгая операция)
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    // КРИТИЧНО: Feed watchdog перед долгой операцией создания UI
    esp_task_wdt_reset();
    
    // Создаем UI объект БЕЗ блокировки менеджера
    lv_obj_t *screen_obj = config->create_fn(config->user_data);
    
    // КРИТИЧНО: Feed watchdog после создания UI
    esp_task_wdt_reset();
    
    if (!screen_obj) {
        ESP_LOGE(TAG, "create_fn failed for screen '%s'", screen_id);
        return ESP_FAIL;
    }
    
    // Создаем группу энкодера
    lv_group_t *encoder_group = lv_group_create();
    if (encoder_group) {
        lv_group_set_wrap(encoder_group, true);  // Циклическая навигация
        ESP_LOGD(TAG, "Encoder group created for '%s'", screen_id);  // УЛУЧШЕНИЕ 3: Оптимизация
    }
    
    // Теперь захватываем mutex для добавления instance
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for adding instance");
            // Очищаем созданные объекты
            if (encoder_group) lv_group_del(encoder_group);
            lv_obj_del(screen_obj);
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Повторно проверяем (могли создать пока мы не держали mutex)
    screen_instance_t *existing_check = find_instance_by_id(screen_id);
    if (existing_check) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGW(TAG, "Screen '%s' was created by another task", screen_id);
        // Очищаем дубликат
        if (encoder_group) lv_group_del(encoder_group);
        lv_obj_del(screen_obj);
        return ESP_OK;
    }
    
    // Проверяем лимит снова
    if (manager->instance_count >= MAX_INSTANCES) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Maximum instances reached (%d)", MAX_INSTANCES);
        if (encoder_group) lv_group_del(encoder_group);
        lv_obj_del(screen_obj);
        return ESP_ERR_NO_MEM;
    }
    
    // Выделяем память для экземпляра
    screen_instance_t *instance = calloc(1, sizeof(screen_instance_t));
    if (!instance) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for instance");
        if (encoder_group) lv_group_del(encoder_group);
        lv_obj_del(screen_obj);
        return ESP_ERR_NO_MEM;
    }
    
    instance->config = config;
    instance->create_time = get_time_ms();
    instance->screen_obj = screen_obj;
    instance->encoder_group = encoder_group;
    instance->is_created = true;
    instance->is_visible = false;
    instance->is_cached = false;
    
    // Добавляем в массив активных экземпляров
    manager->instances[manager->instance_count] = instance;
    manager->instance_count++;
    
    ESP_LOGD(TAG, "Created screen '%s' (%d/%d instances active, encoder group ready)", 
             screen_id, manager->instance_count, MAX_INSTANCES);
    
    // Освобождаем мьютекс перед выходом
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    return ESP_OK;
}

esp_err_t screen_destroy_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск и удаление экземпляра
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for destroy_instance");
            return ESP_ERR_TIMEOUT;
        }
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
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGW(TAG, "Screen '%s' not found for destruction", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Нельзя уничтожить текущий ВИДИМЫЙ экран
    // Но разрешаем уничтожать скрытый экран (is_visible = false),
    // даже если он еще числится в current_screen (для destroy_on_hide)
    if (instance == manager->current_screen && instance->is_visible) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Cannot destroy visible current screen '%s'", screen_id);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Если уничтожаем скрытый текущий экран - очищаем ссылку
    if (instance == manager->current_screen && !instance->is_visible) {
        ESP_LOGD(TAG, "Clearing current_screen pointer for hidden screen '%s'", screen_id);
        manager->current_screen = NULL;
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
    
    // Освобождаем параметры показа (deprecated, но на всякий случай)
    if (instance->show_params) {
        free(instance->show_params);
        instance->show_params = NULL;
    }
    
    // ИСПРАВЛЕНО: Очищаем историю навигации от этого экземпляра
    // Предотвращаем dangling pointers!
    for (int i = 0; i < manager->history_count; i++) {
        if (manager->history[i] == instance) {
            ESP_LOGD(TAG, "Removing '%s' from history at index %d", screen_id, i);
            // Сдвигаем историю
            for (int j = i; j < manager->history_count - 1; j++) {
                manager->history[j] = manager->history[j + 1];
            }
            manager->history_count--;
            manager->history[manager->history_count] = NULL;
            i--; // Проверяем этот индекс снова (на случай дубликатов)
        }
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
    
    // БАГ ИСПРАВЛЕН: Освобождаем мьютекс перед возвратом!
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
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
    
    // УЛУЧШЕНИЕ 1: Защита от повторного показа уже видимого экрана
    if (manager->current_screen && 
        strcmp(manager->current_screen->config->id, screen_id) == 0 &&
        manager->current_screen->is_visible) {
        ESP_LOGD(TAG, "Screen '%s' already visible, skipping redundant show", screen_id);
        return ESP_OK;  // Экран уже показан, не делаем ничего
    }
    
    ESP_LOGD(TAG, "Showing screen '%s'...", screen_id);  // УЛУЧШЕНИЕ 3: Оптимизация логов
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск экземпляра
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for show_instance (find)");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Ищем существующий экземпляр (под защитой mutex)
    screen_instance_t *instance = find_instance_by_id(screen_id);
    
    // Освобождаем mutex для возможности вызова create_instance
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    // Если не создан, создаем
    if (!instance) {
        screen_config_t *config = screen_get_config(screen_id);
        if (!config) {
            ESP_LOGE(TAG, "Screen '%s' not registered", screen_id);
            return ESP_ERR_NOT_FOUND;
        }
        
        // Создаем экземпляр (независимо от флага lazy_load)
        ESP_LOGD(TAG, "Creating screen instance '%s' (lazy_load: %d)", 
                 screen_id, config->lazy_load);
        esp_err_t ret = screen_create_instance(screen_id);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create screen '%s': %s", 
                     screen_id, esp_err_to_name(ret));
            return ret;
        }
        
        // Снова ищем экземпляр под защитой mutex
        if (manager->mutex) {
            xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000));
        }
        instance = find_instance_by_id(screen_id);
        if (manager->mutex) {
            xSemaphoreGive(manager->mutex);
        }
        
        if (!instance) {
            ESP_LOGE(TAG, "Failed to find screen '%s' after creation", screen_id);
            return ESP_ERR_NOT_FOUND;
        }
        
        ESP_LOGD(TAG, "Screen instance '%s' created successfully", screen_id);
    }
    
    // Проверка прав/условий показа
    if (instance->config->can_show_fn && 
        !instance->config->can_show_fn()) {
        ESP_LOGW(TAG, "Screen '%s' cannot be shown (can_show_fn returned false)", 
                 screen_id);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем чтение current_screen и вызов hide
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for hiding previous screen");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Сохраняем ID предыдущего экрана для скрытия
    char prev_screen_id[MAX_SCREEN_ID_LEN] = {0};
    bool need_hide = false;
    
    if (manager->current_screen && manager->current_screen != instance) {
        strncpy(prev_screen_id, manager->current_screen->config->id, MAX_SCREEN_ID_LEN - 1);
        need_hide = true;
    }
    
    // Освобождаем mutex перед вызовом hide
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    // Скрываем предыдущий экран (если нужно)
    if (need_hide) {
        esp_err_t ret = screen_hide_instance(prev_screen_id);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to hide previous screen: %s", 
                     esp_err_to_name(ret));
        }
    }
    
    // ИСПРАВЛЕНО: Не сохраняем params, а только передаем в callback
    // Params должен оставаться в ответственности вызывающего кода
    // Это предотвращает dangling pointers и утечки памяти
    
    // Очищаем старые params если были (на всякий случай)
    if (instance->show_params) {
        free(instance->show_params);
        instance->show_params = NULL;
    }
    
    // ВАЖНО: params передается только в on_show callback,
    // но НЕ сохраняется в instance
    
    // Загружаем экран с анимацией ПЕРЕД on_show и настройкой группы
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
    
    // Устанавливаем группу энкодера для LVGL
    ESP_LOGD(TAG, "Encoder group check: instance=%p, group=%p",  // УЛУЧШЕНИЕ 3
             instance, instance ? instance->encoder_group : NULL);
    
    if (instance->encoder_group) {
        ESP_LOGD(TAG, "Configuring encoder group for '%s'", screen_id);  // УЛУЧШЕНИЕ 3
        
        // КРИТИЧНО: Очищаем группу от всех старых элементов перед добавлением новых
        // Это гарантирует чистое состояние группы для каждого экрана
        uint32_t old_count = lv_group_get_obj_count(instance->encoder_group);
        if (old_count > 0) {
            // Сначала пытаемся очистить скрытые элементы
            int removed = cleanup_hidden_elements(instance->encoder_group);
            if (removed > 0) {
                ESP_LOGI(TAG, "Cleaned %d hidden elements before adding new ones", removed);
            }
            
            // Затем удаляем все оставшиеся элементы для гарантии
            while (lv_group_get_obj_count(instance->encoder_group) > 0) {
                lv_obj_t *obj = lv_group_get_obj_by_index(instance->encoder_group, 0);
                lv_group_remove_obj(obj);
            }
            ESP_LOGD(TAG, "Cleared %d total elements from encoder group", old_count);
        }
        
        // Добавляем интерактивные элементы в группу после загрузки экрана
        // ИСПРАВЛЕНО: используем универсальную функцию для всех экранов
        int added = screen_lifecycle_add_interactive_iterative(instance->screen_obj, instance->encoder_group, 20);
        
        uint32_t obj_count = lv_group_get_obj_count(instance->encoder_group);
        ESP_LOGD(TAG, "Encoder group ready: %d elements added, total %d", added, obj_count);  // УЛУЧШЕНИЕ 3
        
        lv_indev_t *indev = lv_indev_get_next(NULL);
        while (indev) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
                // Привязываем группу к устройству ввода
                lv_indev_set_group(indev, instance->encoder_group);
                
                // LVGL автоматически управляет фокусом
                if (obj_count > 0) {
                    lv_obj_t *first_obj = lv_group_get_obj_by_index(instance->encoder_group, 0);
                    if (first_obj) {
                        lv_group_focus_obj(first_obj);
                    }
                }
                
                ESP_LOGD(TAG, "Encoder indev configured for '%s' (group=%p, obj_count=%d)",  // УЛУЧШЕНИЕ 3
                         screen_id, instance->encoder_group, obj_count);
                break;
            }
            indev = lv_indev_get_next(indev);
        }
    } else {
        ESP_LOGW(TAG, ">>> NO ENCODER GROUP for '%s' - cannot configure encoder!", screen_id);
    }
    
    // Вызываем on_show callback ПОСЛЕ настройки группы энкодера
    // Это позволяет callback'у работать с уже настроенной группой
    if (instance->config->on_show) {
        esp_err_t ret = instance->config->on_show(instance->screen_obj, params);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "on_show callback failed: %s", esp_err_to_name(ret));
        }
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Обновляем состояние с защитой
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for show state update");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Обновляем состояние
    instance->is_visible = true;
    instance->last_show_time = get_time_ms();
    manager->current_screen = instance;
    
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    ESP_LOGD(TAG, "Screen '%s' shown successfully", screen_id);
    
    return ESP_OK;
}

esp_err_t screen_hide_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем доступ к экземпляру
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for hide_instance");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGW(TAG, "Screen '%s' not found for hiding", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->is_visible) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGD(TAG, "Screen '%s' already hidden", screen_id);
        return ESP_OK;  // Уже скрыт
    }
    
    ESP_LOGI(TAG, "Hiding screen '%s'...", screen_id);
    
    // КРИТИЧНО: Очищаем encoder group ПЕРЕД вызовом on_hide
    // Это предотвращает накопление скрытых элементов в группе
    if (instance->encoder_group) {
        // Отвязываем encoder indev от этой группы
        lv_indev_t *indev = lv_indev_get_next(NULL);
        while (indev) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
                if (lv_indev_get_group(indev) == instance->encoder_group) {
                    lv_indev_set_group(indev, NULL);
                    ESP_LOGD(TAG, "Unlinked encoder from group of '%s'", screen_id);
                }
            }
            indev = lv_indev_get_next(indev);
        }
        
        // Удаляем все элементы из группы
        uint32_t count = lv_group_get_obj_count(instance->encoder_group);
        while (lv_group_get_obj_count(instance->encoder_group) > 0) {
            lv_obj_t *obj = lv_group_get_obj_by_index(instance->encoder_group, 0);
            lv_group_remove_obj(obj);
        }
        
        if (count > 0) {
            ESP_LOGD(TAG, "Encoder group cleared for '%s' (%d elements removed)", screen_id, count);
        }
    }
    
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
        // БАГ ИСПРАВЛЕН: Освобождаем мьютекс ПЕРЕД вызовом destroy (чтобы избежать deadlock)
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        
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
    
    // БАГ ИСПРАВЛЕН: Освобождаем мьютекс перед возвратом
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    return ESP_OK;
}

/* =============================
 *  ОБНОВЛЕНИЕ
 * ============================= */

esp_err_t screen_update_instance(const char *screen_id, void *data)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск экземпляра
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for update_instance");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    screen_instance_t *instance = find_instance_by_id(screen_id);
    
    if (!instance) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGW(TAG, "Screen '%s' not found for update", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->config->on_update) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGD(TAG, "Screen '%s' has no on_update callback", screen_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Сохраняем указатели перед освобождением mutex
    lv_obj_t *screen_obj = instance->screen_obj;
    screen_update_fn_t update_fn = instance->config->on_update;
    
    // Освобождаем mutex перед вызовом callback (может быть долгим)
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    ESP_LOGD(TAG, "Updating screen '%s'", screen_id);
    return update_fn(screen_obj, data);
}

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

screen_instance_t* screen_get_current_instance(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем чтение current_screen
    screen_instance_t *current = NULL;
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current = manager->current_screen;
            xSemaphoreGive(manager->mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for get_current (timeout)");
            // Возвращаем без защиты (риск race condition, но лучше чем зависание)
            current = manager->current_screen;
        }
    } else {
        current = manager->current_screen;
    }
    
    return current;
}

screen_instance_t* screen_get_instance_by_id(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск
    screen_instance_t *instance = NULL;
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            instance = find_instance_by_id(screen_id);
            xSemaphoreGive(manager->mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for get_instance_by_id (timeout)");
            // Fallback без защиты (риск race condition, но лучше чем зависание)
            instance = find_instance_by_id(screen_id);
        }
    } else {
        instance = find_instance_by_id(screen_id);
    }
    
    return instance;
}

bool screen_is_visible(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск и чтение состояния
    bool visible = false;
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            screen_instance_t *instance = find_instance_by_id(screen_id);
            visible = instance ? instance->is_visible : false;
            xSemaphoreGive(manager->mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for is_visible (timeout)");
            // Fallback без защиты
            screen_instance_t *instance = find_instance_by_id(screen_id);
            visible = instance ? instance->is_visible : false;
        }
    } else {
        screen_instance_t *instance = find_instance_by_id(screen_id);
        visible = instance ? instance->is_visible : false;
    }
    
    return visible;
}

uint8_t screen_get_instance_count(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем чтение instance_count
    uint8_t count = 0;
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            count = manager->instance_count;
            xSemaphoreGive(manager->mutex);
        } else {
            // Fallback без защиты
            count = manager->instance_count;
        }
    } else {
        count = manager->instance_count;
    }
    
    return count;
}

/* =============================
 *  УПРАВЛЕНИЕ ГРУППОЙ ЭНКОДЕРА
 * ============================= */

esp_err_t screen_add_to_encoder_group(const char *screen_id, lv_obj_t *widget)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!widget) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск экземпляра
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for add_to_encoder_group");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Если screen_id не указан, используем текущий экран
    screen_instance_t *instance = NULL;
    if (screen_id) {
        instance = find_instance_by_id(screen_id);
    } else {
        instance = manager->current_screen;
    }
    
    if (!instance) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Screen not found for adding widget to encoder group");
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->encoder_group) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Encoder group not created for screen '%s'", 
                 instance->config ? instance->config->id : "unknown");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Проверяем, не добавлен ли уже
    uint32_t obj_count = lv_group_get_obj_count(instance->encoder_group);
    bool already_added = false;
    for (uint32_t i = 0; i < obj_count; i++) {
        if (lv_group_get_obj_by_index(instance->encoder_group, i) == widget) {
            already_added = true;
            break;
        }
    }
    
    if (already_added) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGD(TAG, "Widget already in encoder group");
        return ESP_OK;
    }
    
    // Добавляем виджет в группу (под защитой mutex)
    lv_group_add_obj(instance->encoder_group, widget);
    
    int total = lv_group_get_obj_count(instance->encoder_group);
    
    // БАГ ИСПРАВЛЕН: Освобождаем mutex перед возвратом!
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    ESP_LOGD(TAG, "Added widget to encoder group (total: %d)", total);
    
    return ESP_OK;
}

int screen_add_widget_tree(const char *screen_id, lv_obj_t *widget)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!widget) {
        return 0;
    }
    
    // КРИТИЧЕСКАЯ СЕКЦИЯ: Защищаем поиск экземпляра
    if (manager->mutex) {
        if (xSemaphoreTake(manager->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for add_widget_recursive");
            return 0;
        }
    }
    
    // Если screen_id не указан, используем текущий экран
    screen_instance_t *instance = NULL;
    if (screen_id) {
        instance = find_instance_by_id(screen_id);
    } else {
        instance = manager->current_screen;
    }
    
    if (!instance) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Screen not found for adding widget tree to encoder group");
        return 0;
    }
    
    if (!instance->encoder_group) {
        if (manager->mutex) xSemaphoreGive(manager->mutex);
        ESP_LOGE(TAG, "Encoder group not created for screen '%s'", 
                 instance->config ? instance->config->id : "unknown");
        return 0;
    }
    
    lv_group_t *group = instance->encoder_group;
    
    // Освобождаем mutex перед долгим обходом дерева
    if (manager->mutex) {
        xSemaphoreGive(manager->mutex);
    }
    
    // Используем итеративный обход для добавления всех интерактивных элементов
    int added = screen_lifecycle_add_interactive_iterative(widget, group, 20);
    
    ESP_LOGI(TAG, "Added %d interactive elements from widget tree to encoder group", added);
    
    return added;
}

/**
 * @brief Автоматически настроить группу энкодера для экрана
 * 
 * Универсальная функция для автоматического обхода всех виджетов экрана
 * и добавления интерактивных элементов в группу энкодера.
 * 
 * @param screen_obj Объект экрана для обхода
 * @param group Группа энкодера для добавления элементов
 * @return Количество добавленных элементов (или отрицательное значение при ошибке)
 */
int screen_auto_setup_encoder_group(lv_obj_t *screen_obj, lv_group_t *group)
{
    if (!screen_obj || !group) {
        ESP_LOGE(TAG, "Invalid arguments: screen_obj=%p, group=%p", screen_obj, group);
        return -1;
    }
    
    ESP_LOGD(TAG, "Auto-setting up encoder group for screen");
    
    // Используем существующую итеративную функцию обхода
    // max_depth=20 достаточно для большинства UI структур
    int added = screen_lifecycle_add_interactive_iterative(screen_obj, group, 20);
    
    if (added > 0) {
        ESP_LOGI(TAG, "Auto-setup: added %d interactive elements to encoder group", added);
        
        // Устанавливаем фокус на первый элемент если есть
        uint32_t obj_count = lv_group_get_obj_count(group);
        if (obj_count > 0) {
            lv_obj_t *first_obj = lv_group_get_obj_by_index(group, 0);
            if (first_obj) {
                lv_group_focus_obj(first_obj);
                ESP_LOGD(TAG, "Focus set to first element");
            }
        }
    } else if (added == 0) {
        ESP_LOGW(TAG, "Auto-setup: no interactive elements found on screen");
    }
    
    return added;
}

/**
 * @brief Добавить элементы главного экрана в правильном порядке
 * 
 * Добавляет элементы в следующем порядке:
 * 1. Карточка датчика 0 (pH)
 * 2. Карточка датчика 1 (EC)
 * 3. Карточка датчика 2 (Temperature)
 * 4. Карточка датчика 3 (Humidity)
 * 5. Карточка датчика 4 (Light)
 * 6. Карточка датчика 5 (CO2)
 * 7. Кнопка SET
 * 
 * @param screen_obj Объект главного экрана
 * @param group Группа энкодера
 * @return Количество добавленных элементов
 */
int screen_lifecycle_add_main_screen_elements(lv_obj_t *screen_obj, lv_group_t *group)
{
    if (!screen_obj || !group) return 0;
    
    int added = 0;
    
    ESP_LOGI(TAG, "Adding main screen elements to encoder group");
    
    // Ищем контейнер с карточками датчиков
    lv_obj_t *content = NULL;
    uint32_t child_count = lv_obj_get_child_count(screen_obj);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(screen_obj, i);
        if (child) {
            uint32_t grandchild_count = lv_obj_get_child_count(child);
            if (grandchild_count >= 6) {
                content = child;
                ESP_LOGD(TAG, "Found sensor cards container at child %d", i);
                break;
            }
        }
    }
    
    if (!content) {
        ESP_LOGW(TAG, "Could not find sensor cards container in main screen");
        return 0;
    }
    
    // Добавляем карточки датчиков в правильном порядке (0-5)
    for (int i = 0; i < 6; i++) {
        lv_obj_t *card = lv_obj_get_child(content, i);
        if (card) {
            if (is_interactive_element(card)) {
                lv_group_add_obj(group, card);
                added++;
                ESP_LOGD(TAG, "Added sensor card %d to encoder group", i);
            }
        } else {
            ESP_LOGW(TAG, "Sensor card %d is NULL", i);
        }
    }
    
    // Добавляем кнопку SET из status_bar
    lv_obj_t *status_bar = NULL;
    ESP_LOGD(TAG, "Looking for SET button in status_bar");
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(screen_obj, i);
        if (child && lv_obj_get_child_count(child) > 0) {
            // Ищем кнопку в этом контейнере
            uint32_t grandchild_count = lv_obj_get_child_count(child);
            for (uint32_t j = 0; j < grandchild_count; j++) {
                lv_obj_t *grandchild = lv_obj_get_child(child, j);
                if (grandchild && lv_obj_check_type(grandchild, &lv_button_class)) {
                    status_bar = child;
                    ESP_LOGD(TAG, "Found button in child %d, grandchild %d", i, j);
                    break;
                }
            }
            if (status_bar) break;
        }
    }
    
    if (status_bar) {
        uint32_t status_child_count = lv_obj_get_child_count(status_bar);
        for (uint32_t i = 0; i < status_child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(status_bar, i);
            if (child && lv_obj_check_type(child, &lv_button_class)) {
                if (is_interactive_element(child)) {
                    lv_group_add_obj(group, child);
                    added++;
                    ESP_LOGD(TAG, "Added SET button to encoder group");
                    break;
                }
            }
        }
    } else {
        ESP_LOGW(TAG, "Could not find status_bar with SET button");
    }
    
    ESP_LOGI(TAG, "Added %d elements to main screen encoder group in correct order", added);
    return added;
}

