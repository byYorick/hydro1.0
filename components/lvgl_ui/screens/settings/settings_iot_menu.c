/**
 * @file settings_iot_menu.c
 * @brief Подменю IoT настроек (MQTT, Telegram, SD, Mesh)
 */

#include "settings_iot_menu.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"
#include "widgets/menu_list.h"
#include "esp_log.h"

static const char *TAG = "SETTINGS_IOT_MENU";

static void on_mqtt_click(lv_event_t *e) {
    ESP_LOGI(TAG, "MQTT Settings clicked");
    screen_show("settings_mqtt", NULL);
}

static void on_telegram_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Telegram Settings clicked");
    screen_show("settings_telegram", NULL);
}

static void on_sd_click(lv_event_t *e) {
    ESP_LOGI(TAG, "SD Settings clicked");
    screen_show("settings_sd", NULL);
}

static void on_mesh_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Mesh Settings clicked");
    screen_show("settings_mesh", NULL);
}

static lv_obj_t* settings_iot_menu_create(void *params) {
    ESP_LOGI(TAG, "Creating IoT settings menu");
    
    menu_item_config_t items[] = {
        {
            .text = "MQTT",
            .icon = LV_SYMBOL_UPLOAD,
            .callback = on_mqtt_click,
            .user_data = NULL,
        },
        {
            .text = "Telegram",
            .icon = LV_SYMBOL_CALL,
            .callback = on_telegram_click,
            .user_data = NULL,
        },
        {
            .text = "SD Card",
            .icon = LV_SYMBOL_SD_CARD,
            .callback = on_sd_click,
            .user_data = NULL,
        },
        {
            .text = "Mesh Network",
            .icon = LV_SYMBOL_WIFI,
            .callback = on_mesh_click,
            .user_data = NULL,
        },
    };
    
    template_menu_config_t menu_cfg = {
        .title = "IoT Settings",
        .items = items,
        .item_count = 4,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    return template_create_menu_screen(&menu_cfg, NULL);
}

esp_err_t settings_iot_menu_init(void) {
    ESP_LOGI(TAG, "Initializing IoT settings menu");
    
    screen_config_t config = {
        .id = "settings_iot",
        .title = "IoT Settings",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "settings_main",
        .can_go_back = true,
        .lazy_load = true,
        .cache_on_hide = true,
        .destroy_on_hide = false,
        .has_status_bar = true,
        .has_back_button = true,
        .create_fn = settings_iot_menu_create,
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IoT menu: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "IoT settings menu registered");
    return ESP_OK;
}

