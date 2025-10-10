/**
 * @file settings_main_screen.c
 * @brief Главное меню настроек с иерархической структурой
 */

#include "settings_main_screen.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"
#include "widgets/menu_list.h"
#include "esp_log.h"

static const char *TAG = "SETTINGS_MAIN";

/* Callbacks для пунктов меню */

static void on_sensors_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Sensors Settings clicked");
    screen_show("settings_sensors", NULL);
}

static void on_pumps_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Pumps Settings clicked");
    screen_show("settings_pumps", NULL);
}

static void on_wifi_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "WiFi Settings clicked");
    screen_show("settings_wifi", NULL);
}

static void on_iot_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "IoT Settings clicked");
    screen_show("settings_iot", NULL);
}

static void on_ai_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "AI Settings clicked");
    screen_show("settings_ai", NULL);
}

static void on_system_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "System Settings clicked");
    screen_show("settings_system", NULL);
}

static esp_err_t settings_main_on_show(lv_obj_t *screen_obj, void *params) {
    ESP_LOGI(TAG, "Settings main menu shown");
    
    screen_instance_t *inst = screen_get_by_id("settings_main");
    if (!inst || !inst->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available");
        return ESP_OK;
    }
    
    int obj_count = lv_group_get_obj_count(inst->encoder_group);
    ESP_LOGI(TAG, "  Encoder group ready with %d interactive elements", obj_count);
    
    return ESP_OK;
}

static lv_obj_t* settings_main_create(void *params) {
    ESP_LOGI(TAG, "Creating settings main screen");
    
    menu_item_config_t items[] = {
        {
            .text = "Sensors",
            .icon = LV_SYMBOL_SETTINGS,
            .callback = on_sensors_settings_click,
            .user_data = NULL,
        },
        {
            .text = "Pumps",
            .icon = LV_SYMBOL_CHARGE,
            .callback = on_pumps_settings_click,
            .user_data = NULL,
        },
        {
            .text = "WiFi",
            .icon = LV_SYMBOL_WIFI,
            .callback = on_wifi_settings_click,
            .user_data = NULL,
        },
        {
            .text = "IoT",
            .icon = LV_SYMBOL_UPLOAD,
            .callback = on_iot_settings_click,
            .user_data = NULL,
        },
        {
            .text = "AI Control",
            .icon = LV_SYMBOL_EYE_OPEN,
            .callback = on_ai_settings_click,
            .user_data = NULL,
        },
        {
            .text = "System",
            .icon = LV_SYMBOL_SETTINGS,
            .callback = on_system_settings_click,
            .user_data = NULL,
        },
    };
    
    template_menu_config_t menu_cfg = {
        .title = "Settings",
        .items = items,
        .item_count = 6,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    return template_create_menu_screen(&menu_cfg, NULL);
}

esp_err_t settings_main_screen_init(void) {
    ESP_LOGI(TAG, "Initializing settings main screen");
    
    screen_config_t config = {
        .id = "settings_main",
        .title = "Settings",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .cache_on_hide = true,
        .destroy_on_hide = false,
        .has_status_bar = true,
        .has_back_button = true,
        .create_fn = settings_main_create,
        .on_show = settings_main_on_show,
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register settings main: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Settings main registered successfully");
    return ESP_OK;
}

