/**
 * @file settings_wifi_screen.c
 * @brief Экран настроек WiFi с редактированием через энкодер
 */

#include "settings_wifi_screen.h"
#include "screen_manager/screen_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "config_manager.h"
#include "system_config.h"
#include <string.h>

static const char *TAG = "SETTINGS_WIFI";

// UI элементы
static lv_obj_t *textarea_ssid;
static lv_obj_t *textarea_password;
static lv_obj_t *switch_static_ip;
static lv_obj_t *textarea_ip;
static lv_obj_t *textarea_gateway;
static lv_obj_t *textarea_netmask;
static lv_obj_t *textarea_dns;
static lv_obj_t *dropdown_mode;
static lv_obj_t *switch_auto_reconnect;
static lv_obj_t *btn_save;

// Локальная копия конфигурации
static wifi_config_t local_wifi_config;

/**
 * @brief Сохранение настроек
 */
static void save_wifi_settings(lv_event_t *e) {
    ESP_LOGI(TAG, "Saving WiFi settings...");
    
    // Загружаем текущую конфигурацию системы
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load config");
        return;
    }
    
    // Получаем значения из UI
    const char *ssid = lv_textarea_get_text(textarea_ssid);
    const char *password = lv_textarea_get_text(textarea_password);
    bool static_ip = lv_obj_has_state(switch_static_ip, LV_STATE_CHECKED);
    const char *ip = lv_textarea_get_text(textarea_ip);
    const char *gw = lv_textarea_get_text(textarea_gateway);
    const char *nm = lv_textarea_get_text(textarea_netmask);
    const char *dns = lv_textarea_get_text(textarea_dns);
    uint16_t mode_idx = lv_dropdown_get_selected(dropdown_mode);
    bool auto_reconnect = lv_obj_has_state(switch_auto_reconnect, LV_STATE_CHECKED);
    
    // Обновляем конфигурацию
    strncpy(sys_config.wifi.ssid, ssid, sizeof(sys_config.wifi.ssid) - 1);
    strncpy(sys_config.wifi.password, password, sizeof(sys_config.wifi.password) - 1);
    sys_config.wifi.use_static_ip = static_ip;
    strncpy(sys_config.wifi.static_ip, ip, sizeof(sys_config.wifi.static_ip) - 1);
    strncpy(sys_config.wifi.gateway, gw, sizeof(sys_config.wifi.gateway) - 1);
    strncpy(sys_config.wifi.netmask, nm, sizeof(sys_config.wifi.netmask) - 1);
    strncpy(sys_config.wifi.dns, dns, sizeof(sys_config.wifi.dns) - 1);
    sys_config.wifi.network_mode = mode_idx;
    sys_config.wifi.auto_reconnect = auto_reconnect;
    
    // Сохраняем в NVS
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi settings saved successfully");
        
        // Показываем уведомление
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Saved!");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);
        lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
        
        // Удаляем через 2 секунды
        lv_obj_del_delayed(label, 2000);
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi settings");
    }
}

/**
 * @brief Обновление видимости полей static IP
 */
static void update_static_ip_visibility(lv_event_t *e) {
    bool checked = lv_obj_has_state(switch_static_ip, LV_STATE_CHECKED);
    
    if (checked) {
        lv_obj_clear_flag(textarea_ip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(textarea_gateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(textarea_netmask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(textarea_dns, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(textarea_ip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_gateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_netmask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_dns, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t* settings_wifi_create(void *params) {
    ESP_LOGI(TAG, "Creating WiFi settings screen");
    
    // Загружаем текущую конфигурацию
    system_config_t sys_config;
    if (config_load(&sys_config) == ESP_OK) {
        local_wifi_config = sys_config.wifi;
    }
    
    // Создаем контейнер с прокруткой
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(90));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    
    // SSID
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "SSID:");
    
    textarea_ssid = lv_textarea_create(cont);
    lv_obj_set_width(textarea_ssid, LV_PCT(90));
    lv_textarea_set_one_line(textarea_ssid, true);
    lv_textarea_set_text(textarea_ssid, local_wifi_config.ssid);
    lv_textarea_set_max_length(textarea_ssid, 31);
    
    // Password
    label = lv_label_create(cont);
    lv_label_set_text(label, "Password:");
    
    textarea_password = lv_textarea_create(cont);
    lv_obj_set_width(textarea_password, LV_PCT(90));
    lv_textarea_set_one_line(textarea_password, true);
    lv_textarea_set_text(textarea_password, local_wifi_config.password);
    lv_textarea_set_max_length(textarea_password, 63);
    lv_textarea_set_password_mode(textarea_password, false); // Показываем полностью
    
    // Network Mode
    label = lv_label_create(cont);
    lv_label_set_text(label, "Mode:");
    
    dropdown_mode = lv_dropdown_create(cont);
    lv_obj_set_width(dropdown_mode, LV_PCT(90));
    lv_dropdown_set_options(dropdown_mode, "Station (STA)\nAccess Point (AP)\nHybrid (STA+AP)");
    lv_dropdown_set_selected(dropdown_mode, local_wifi_config.network_mode);
    
    // Auto Reconnect
    lv_obj_t *sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Auto Reconnect:");
    
    switch_auto_reconnect = lv_switch_create(sw_cont);
    if (local_wifi_config.auto_reconnect) {
        lv_obj_add_state(switch_auto_reconnect, LV_STATE_CHECKED);
    }
    
    // Static IP
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Static IP:");
    
    switch_static_ip = lv_switch_create(sw_cont);
    if (local_wifi_config.use_static_ip) {
        lv_obj_add_state(switch_static_ip, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(switch_static_ip, update_static_ip_visibility, LV_EVENT_VALUE_CHANGED, NULL);
    
    // IP Address
    label = lv_label_create(cont);
    lv_label_set_text(label, "IP Address:");
    
    textarea_ip = lv_textarea_create(cont);
    lv_obj_set_width(textarea_ip, LV_PCT(90));
    lv_textarea_set_one_line(textarea_ip, true);
    lv_textarea_set_text(textarea_ip, local_wifi_config.static_ip);
    lv_textarea_set_max_length(textarea_ip, 15);
    
    // Gateway
    label = lv_label_create(cont);
    lv_label_set_text(label, "Gateway:");
    
    textarea_gateway = lv_textarea_create(cont);
    lv_obj_set_width(textarea_gateway, LV_PCT(90));
    lv_textarea_set_one_line(textarea_gateway, true);
    lv_textarea_set_text(textarea_gateway, local_wifi_config.gateway);
    lv_textarea_set_max_length(textarea_gateway, 15);
    
    // Netmask
    label = lv_label_create(cont);
    lv_label_set_text(label, "Netmask:");
    
    textarea_netmask = lv_textarea_create(cont);
    lv_obj_set_width(textarea_netmask, LV_PCT(90));
    lv_textarea_set_one_line(textarea_netmask, true);
    lv_textarea_set_text(textarea_netmask, local_wifi_config.netmask);
    lv_textarea_set_max_length(textarea_netmask, 15);
    
    // DNS
    label = lv_label_create(cont);
    lv_label_set_text(label, "DNS:");
    
    textarea_dns = lv_textarea_create(cont);
    lv_obj_set_width(textarea_dns, LV_PCT(90));
    lv_textarea_set_one_line(textarea_dns, true);
    lv_textarea_set_text(textarea_dns, local_wifi_config.dns);
    lv_textarea_set_max_length(textarea_dns, 15);
    
    // Начальная видимость static IP полей
    if (!local_wifi_config.use_static_ip) {
        lv_obj_add_flag(textarea_ip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_gateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_netmask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(textarea_dns, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Кнопка "Сохранить"
    btn_save = lv_btn_create(cont);
    lv_obj_set_width(btn_save, LV_PCT(90));
    lv_obj_add_event_cb(btn_save, save_wifi_settings, LV_EVENT_CLICKED, NULL);
    
    label = lv_label_create(btn_save);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save");
    lv_obj_center(label);
    
    return screen;
}

esp_err_t settings_wifi_screen_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi settings screen");
    
    screen_config_t config = {
        .id = "settings_wifi",
        .title = "WiFi Settings",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_main",
        .can_go_back = true,
        .lazy_load = true,
        .cache_on_hide = false,
        .destroy_on_hide = true,
        .has_status_bar = true,
        .has_back_button = true,
        .create_fn = settings_wifi_create,
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi settings: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi settings screen registered");
    return ESP_OK;
}

