/**
 * @file settings_mqtt_screen.c
 * @brief Экран настроек MQTT с полным набором параметров
 */

#include "settings_mqtt_screen.h"
#include "screen_manager/screen_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "config_manager.h"
#include "system_config.h"
#include <string.h>

static const char *TAG = "SETTINGS_MQTT";

static lv_obj_t *switch_enabled;
static lv_obj_t *textarea_broker_uri;
static lv_obj_t *textarea_client_id;
static lv_obj_t *textarea_username;
static lv_obj_t *textarea_password;
static lv_obj_t *spinbox_keepalive;
static lv_obj_t *switch_auto_reconnect;
static lv_obj_t *spinbox_publish_interval;
static lv_obj_t *btn_save;

static void save_mqtt_settings(lv_event_t *e) {
    ESP_LOGI(TAG, "Saving MQTT settings...");
    
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load config");
        return;
    }
    
    // Обновляем MQTT конфигурацию
    sys_config.mqtt.enabled = lv_obj_has_state(switch_enabled, LV_STATE_CHECKED);
    strncpy(sys_config.mqtt.broker_uri, lv_textarea_get_text(textarea_broker_uri), 
            sizeof(sys_config.mqtt.broker_uri) - 1);
    strncpy(sys_config.mqtt.client_id, lv_textarea_get_text(textarea_client_id), 
            sizeof(sys_config.mqtt.client_id) - 1);
    strncpy(sys_config.mqtt.username, lv_textarea_get_text(textarea_username), 
            sizeof(sys_config.mqtt.username) - 1);
    strncpy(sys_config.mqtt.password, lv_textarea_get_text(textarea_password), 
            sizeof(sys_config.mqtt.password) - 1);
    sys_config.mqtt.keepalive = lv_spinbox_get_value(spinbox_keepalive);
    sys_config.mqtt.auto_reconnect = lv_obj_has_state(switch_auto_reconnect, LV_STATE_CHECKED);
    sys_config.mqtt.publish_interval = lv_spinbox_get_value(spinbox_publish_interval);
    
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI(TAG, "MQTT settings saved");
        
        // Уведомление об успехе
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "MQTT Saved!");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);
        lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
        lv_obj_del_delayed(label, 2000);
    }
}

static lv_obj_t* settings_mqtt_create(void *params) {
    ESP_LOGI(TAG, "Creating MQTT settings screen");
    
    // Загрузка конфигурации
    system_config_t sys_config;
    mqtt_config_t mqtt_cfg = {0};
    if (config_load(&sys_config) == ESP_OK) {
        mqtt_cfg = sys_config.mqtt;
    }
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(90));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    
    // Enabled
    lv_obj_t *sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *label = lv_label_create(sw_cont);
    lv_label_set_text(label, "MQTT Enabled:");
    
    switch_enabled = lv_switch_create(sw_cont);
    if (mqtt_cfg.enabled) lv_obj_add_state(switch_enabled, LV_STATE_CHECKED);
    
    // Broker URI
    label = lv_label_create(cont);
    lv_label_set_text(label, "Broker URI:");
    
    textarea_broker_uri = lv_textarea_create(cont);
    lv_obj_set_width(textarea_broker_uri, LV_PCT(90));
    lv_textarea_set_one_line(textarea_broker_uri, true);
    lv_textarea_set_text(textarea_broker_uri, mqtt_cfg.broker_uri);
    lv_textarea_set_placeholder_text(textarea_broker_uri, "mqtt://ip:port");
    
    // Client ID
    label = lv_label_create(cont);
    lv_label_set_text(label, "Client ID:");
    
    textarea_client_id = lv_textarea_create(cont);
    lv_obj_set_width(textarea_client_id, LV_PCT(90));
    lv_textarea_set_one_line(textarea_client_id, true);
    lv_textarea_set_text(textarea_client_id, mqtt_cfg.client_id);
    
    // Username
    label = lv_label_create(cont);
    lv_label_set_text(label, "Username (optional):");
    
    textarea_username = lv_textarea_create(cont);
    lv_obj_set_width(textarea_username, LV_PCT(90));
    lv_textarea_set_one_line(textarea_username, true);
    lv_textarea_set_text(textarea_username, mqtt_cfg.username);
    
    // Password
    label = lv_label_create(cont);
    lv_label_set_text(label, "Password (optional):");
    
    textarea_password = lv_textarea_create(cont);
    lv_obj_set_width(textarea_password, LV_PCT(90));
    lv_textarea_set_one_line(textarea_password, true);
    lv_textarea_set_text(textarea_password, mqtt_cfg.password);
    
    // Keepalive
    label = lv_label_create(cont);
    lv_label_set_text(label, "Keepalive (sec):");
    
    spinbox_keepalive = lv_spinbox_create(cont);
    lv_obj_set_width(spinbox_keepalive, LV_PCT(90));
    lv_spinbox_set_range(spinbox_keepalive, 30, 300);
    lv_spinbox_set_value(spinbox_keepalive, mqtt_cfg.keepalive);
    lv_spinbox_set_digit_format(spinbox_keepalive, 3, 0);
    lv_spinbox_set_step(spinbox_keepalive, 10);
    
    // Auto Reconnect
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Auto Reconnect:");
    
    switch_auto_reconnect = lv_switch_create(sw_cont);
    if (mqtt_cfg.auto_reconnect) lv_obj_add_state(switch_auto_reconnect, LV_STATE_CHECKED);
    
    // Publish Interval
    label = lv_label_create(cont);
    lv_label_set_text(label, "Publish Interval (sec):");
    
    spinbox_publish_interval = lv_spinbox_create(cont);
    lv_obj_set_width(spinbox_publish_interval, LV_PCT(90));
    lv_spinbox_set_range(spinbox_publish_interval, 1, 60);
    lv_spinbox_set_value(spinbox_publish_interval, mqtt_cfg.publish_interval);
    lv_spinbox_set_digit_format(spinbox_publish_interval, 2, 0);
    lv_spinbox_set_step(spinbox_publish_interval, 1);
    
    // Кнопка Сохранить
    btn_save = lv_btn_create(cont);
    lv_obj_set_width(btn_save, LV_PCT(90));
    lv_obj_add_event_cb(btn_save, save_mqtt_settings, LV_EVENT_CLICKED, NULL);
    
    label = lv_label_create(btn_save);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save MQTT");
    lv_obj_center(label);
    
    return screen;
}

esp_err_t settings_mqtt_screen_init(void) {
    ESP_LOGI(TAG, "Initializing MQTT settings screen");
    
    screen_config_t config = {
        .id = "settings_mqtt",
        .title = "MQTT Settings",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .has_status_bar = true,
        .has_back_button = true,
        .create_fn = settings_mqtt_create,
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT settings: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT settings screen registered");
    return ESP_OK;
}

