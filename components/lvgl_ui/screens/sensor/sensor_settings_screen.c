/**
 * @file sensor_settings_screen.c
 * @brief Реализация экранов настроек датчиков
 */

#include "sensor_settings_screen.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"
#include "widgets/menu_list.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SENSOR_SETTINGS";

/* =============================
 *  МЕТАДАННЫЕ
 * ============================= */

typedef struct {
    const char *id;
    const char *name;
    const char *parent_id;  // Экран детализации
} settings_meta_t;

static const settings_meta_t SETTINGS_META[6] = {
    { .id = "settings_ph", .name = "pH Settings", .parent_id = "detail_ph" },
    { .id = "settings_ec", .name = "EC Settings", .parent_id = "detail_ec" },
    { .id = "settings_temp", .name = "Temperature Settings", .parent_id = "detail_temp" },
    { .id = "settings_humidity", .name = "Humidity Settings", .parent_id = "detail_humidity" },
    { .id = "settings_lux", .name = "Light Settings", .parent_id = "detail_lux" },
    { .id = "settings_co2", .name = "CO2 Settings", .parent_id = "detail_co2" },
};

/* =============================
 *  CALLBACKS
 * ============================= */

static void on_calibration_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Calibration clicked");
    // TODO: Открыть экран калибровки
}

static void on_alarms_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Alarm thresholds clicked");
    // TODO: Открыть экран настройки порогов
}

static void on_interval_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Update interval clicked");
    // TODO: Открыть экран настройки интервала
}

static void on_units_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Display units clicked");
    // TODO: Открыть экран настройки единиц
}

static void on_logging_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Data logging clicked");
    // TODO: Открыть экран настройки логирования
}

/* =============================
 *  СОЗДАНИЕ ЭКРАНА
 * ============================= */

static lv_obj_t* sensor_settings_create(void *params)
{
    int sensor_index = (int)(intptr_t)params;
    
    if (sensor_index < 0 || sensor_index >= 6) {
        ESP_LOGE(TAG, "Invalid sensor index: %d", sensor_index);
        return NULL;
    }
    
    const settings_meta_t *meta = &SETTINGS_META[sensor_index];
    
    ESP_LOGI(TAG, "Creating settings screen for %s", meta->name);
    
    // Пункты меню настроек
    menu_item_config_t items[] = {
        {
            .text = "Calibration",
            .icon = LV_SYMBOL_SETTINGS,
            .callback = on_calibration_click,
            .user_data = (void*)(intptr_t)sensor_index,
        },
        {
            .text = "Alarm Thresholds",
            .icon = LV_SYMBOL_WARNING,
            .callback = on_alarms_click,
            .user_data = (void*)(intptr_t)sensor_index,
        },
        {
            .text = "Update Interval",
            .icon = LV_SYMBOL_REFRESH,
            .callback = on_interval_click,
            .user_data = (void*)(intptr_t)sensor_index,
        },
        {
            .text = "Display Units",
            .icon = LV_SYMBOL_IMAGE,
            .callback = on_units_click,
            .user_data = (void*)(intptr_t)sensor_index,
        },
        {
            .text = "Data Logging",
            .icon = LV_SYMBOL_SD_CARD,
            .callback = on_logging_click,
            .user_data = (void*)(intptr_t)sensor_index,
        },
    };
    
    // Используем шаблон меню
    template_menu_config_t menu_cfg = {
        .title = meta->name,
        .items = items,
        .item_count = 5,
        .has_back_button = true,
        .back_callback = NULL,  // Автоматическая навигация к parent_id
    };
    
    // Получаем группу энкодера
    screen_instance_t *inst = screen_get_by_id(meta->id);
    lv_group_t *group = inst ? inst->encoder_group : NULL;
    
    return template_create_menu_screen(&menu_cfg, group);
}

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

esp_err_t sensor_settings_screens_register_all(void)
{
    ESP_LOGI(TAG, "Registering all sensor settings screens");
    
    for (int i = 0; i < 6; i++) {
        const settings_meta_t *meta = &SETTINGS_META[i];
        
        screen_config_t config = {
            .id = {0},
            .title = meta->name,
            .category = SCREEN_CATEGORY_SETTINGS,
            .parent_id = {0},
            .can_go_back = true,
            .lazy_load = true,              // Создавать при показе
            .cache_on_hide = false,         // Не кэшировать
            .destroy_on_hide = true,        // Уничтожать для экономии памяти
            .has_status_bar = true,
            .has_back_button = true,
            .create_fn = sensor_settings_create,
            .user_data = (void*)(intptr_t)i,
        };
        
        // Копируем строки
        strncpy(config.id, meta->id, MAX_SCREEN_ID_LEN - 1);
        strncpy(config.parent_id, meta->parent_id, MAX_SCREEN_ID_LEN - 1);
        
        esp_err_t ret = screen_register(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s: %s", 
                     meta->id, esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGI(TAG, "Registered '%s'", meta->id);
    }
    
    ESP_LOGI(TAG, "All 6 sensor settings screens registered");
    return ESP_OK;
}

