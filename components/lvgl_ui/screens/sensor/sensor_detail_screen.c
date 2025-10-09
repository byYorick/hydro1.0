/**
 * @file sensor_detail_screen.c
 * @brief Реализация шаблона экрана детализации датчика
 */

#include "sensor_detail_screen.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SENSOR_DETAIL";

/* =============================
 *  МЕТАДАННЫЕ ДАТЧИКОВ
 * ============================= */

typedef struct {
    const char *id;
    const char *name;
    const char *unit;
    const char *description;
    uint8_t decimals;
    float default_target;
} sensor_meta_t;

static const sensor_meta_t SENSOR_META[6] = {
    {
        .id = "detail_ph",
        .name = "pH",
        .unit = "",
        .description = "Keep the nutrient solution balanced for optimal uptake.",
        .decimals = 2,
        .default_target = 6.8f,
    },
    {
        .id = "detail_ec",
        .name = "EC",
        .unit = "mS/cm",
        .description = "Electrical conductivity shows nutrient strength.",
        .decimals = 2,
        .default_target = 1.5f,
    },
    {
        .id = "detail_temp",
        .name = "Temperature",
        .unit = "°C",
        .description = "Keep solution and air temperature comfortable.",
        .decimals = 1,
        .default_target = 24.0f,
    },
    {
        .id = "detail_humidity",
        .name = "Humidity",
        .unit = "%",
        .description = "Stable humidity reduces stress and supports growth.",
        .decimals = 1,
        .default_target = 70.0f,
    },
    {
        .id = "detail_lux",
        .name = "Light",
        .unit = "lux",
        .description = "Monitor light levels for healthy photosynthesis.",
        .decimals = 0,
        .default_target = 500.0f,
    },
    {
        .id = "detail_co2",
        .name = "CO2",
        .unit = "ppm",
        .description = "Avoid excessive CO2 for comfort.",
        .decimals = 0,
        .default_target = 450.0f,
    },
};

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки настроек
 */
static void on_settings_click(lv_event_t *e)
{
    int sensor_id = (int)(intptr_t)lv_event_get_user_data(e);
    
    const char *settings_screens[] = {
        "settings_ph", "settings_ec", "settings_temp",
        "settings_humidity", "settings_lux", "settings_co2"
    };
    
    if (sensor_id >= 0 && sensor_id < 6) {
        ESP_LOGI(TAG, "Opening settings for sensor %d", sensor_id);
        screen_show(settings_screens[sensor_id], NULL);
    }
}

/**
 * @brief Callback при показе экрана детализации - настройка группы
 */
static esp_err_t sensor_detail_on_show(lv_obj_t *screen_obj, void *params)
{
    int sensor_id = (int)(intptr_t)params;
    if (sensor_id < 0 || sensor_id >= 6) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const sensor_meta_t *meta = &SENSOR_META[sensor_id];
    ESP_LOGI(TAG, "Detail screen '%s' shown - configuring encoder", meta->name);
    
    // Получаем instance для доступа к encoder_group
    screen_instance_t *inst = screen_get_by_id(meta->id);
    if (!inst || !inst->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available");
        return ESP_OK;
    }
    
    lv_group_t *group = inst->encoder_group;
    
    // ВАЖНО: Добавляем все интерактивные элементы в группу
    // Ищем кнопки на экране и добавляем их
    lv_obj_t *child = lv_obj_get_child(screen_obj, 0);
    int added = 0;
    
    while (child != NULL) {
        // Добавляем кнопки
        if (lv_obj_check_type(child, &lv_button_class)) {
            lv_group_add_obj(group, child);
            added++;
            ESP_LOGD(TAG, "  Added button to encoder group");
        }
        
        // Проверяем дочерние элементы
        lv_obj_t *grandchild = lv_obj_get_child(child, 0);
        while (grandchild != NULL) {
            if (lv_obj_check_type(grandchild, &lv_button_class)) {
                lv_group_add_obj(group, grandchild);
                added++;
                ESP_LOGD(TAG, "  Added nested button to encoder group");
            }
            grandchild = lv_obj_get_child(child, lv_obj_get_index(grandchild) + 1);
        }
        
        child = lv_obj_get_child(screen_obj, lv_obj_get_index(child) + 1);
    }
    
    int obj_count = lv_group_get_obj_count(group);
    ESP_LOGI(TAG, "  Encoder group has %d objects (added %d)", obj_count, added);
    
    // Устанавливаем начальный фокус
    if (obj_count > 0) {
        lv_group_focus_next(group);
        ESP_LOGI(TAG, "  Initial focus set");
    }
    
    return ESP_OK;
}

/**
 * @brief Функция создания экрана детализации (шаблон для всех датчиков)
 */
static lv_obj_t* sensor_detail_create(void *params)
{
    // Получаем индекс датчика из user_data конфигурации
    int sensor_index = (int)(intptr_t)params;
    
    if (sensor_index < 0 || sensor_index >= 6) {
        ESP_LOGE(TAG, "Invalid sensor index: %d", sensor_index);
        return NULL;
    }
    
    const sensor_meta_t *meta = &SENSOR_META[sensor_index];
    
    ESP_LOGI(TAG, "Creating detail screen for %s", meta->name);
    
    // Используем шаблон детализации
    template_detail_config_t detail_cfg = {
        .title = meta->name,
        .description = meta->description,
        .current_value = 0.0f,  // Будет обновлено позже
        .target_value = meta->default_target,
        .unit = meta->unit,
        .decimals = meta->decimals,
        .settings_callback = on_settings_click,
        .settings_user_data = (void*)(intptr_t)sensor_index,  // ← Передаем sensor_index!
        .back_callback = NULL,  // Автоматическая навигация
    };
    
    // Создаем экран без группы (группа будет настроена в on_show)
    lv_obj_t *screen = template_create_detail_screen(&detail_cfg, NULL);
    
    // Сохраняем sensor_index в user_data экрана для on_show callback
    if (screen) {
        lv_obj_set_user_data(screen, (void*)(intptr_t)sensor_index);
    }
    
    return screen;
}

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

esp_err_t sensor_detail_screens_register_all(void)
{
    ESP_LOGI(TAG, "Registering all sensor detail screens");
    
    for (int i = 0; i < 6; i++) {
        const sensor_meta_t *meta = &SENSOR_META[i];
        
        screen_config_t config = {
            .id = {0},
            .title = meta->name,
            .category = SCREEN_CATEGORY_DETAIL,
            .parent_id = "main",            // Возврат на главный экран
            .can_go_back = true,
            .lazy_load = true,              // Создавать при первом показе
            .cache_on_hide = true,          // Кэшировать для быстрого повторного показа
            .destroy_on_hide = false,
            .has_status_bar = true,
            .has_back_button = true,
            .create_fn = sensor_detail_create,
            .on_show = sensor_detail_on_show,  // Настройка группы энкодера
            .user_data = (void*)(intptr_t)i,  // Передаем индекс датчика
        };
        
        // Копируем ID
        strncpy(config.id, meta->id, MAX_SCREEN_ID_LEN - 1);
        strncpy(config.parent_id, "main", MAX_SCREEN_ID_LEN - 1);
        
        esp_err_t ret = screen_register(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s: %s", 
                     meta->id, esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGI(TAG, "Registered '%s'", meta->id);
    }
    
    ESP_LOGI(TAG, "All 6 sensor detail screens registered");
    return ESP_OK;
}

esp_err_t sensor_detail_screen_update(uint8_t sensor_index, 
                                       float current_value,
                                       float target_value)
{
    if (sensor_index >= 6) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Реализовать обновление через on_update callback
    // Пока просто логируем
    ESP_LOGD(TAG, "Update sensor %d: current=%.2f, target=%.2f",
             sensor_index, current_value, target_value);
    
    return ESP_OK;
}

