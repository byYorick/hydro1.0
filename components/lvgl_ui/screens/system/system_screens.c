/**
 * @file system_screens.c
 * @brief Реализация всех системных экранов
 */

#include "system_screens.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_base.h"
#include "widgets/back_button.h"
#include "lvgl_styles.h"
#include "esp_log.h"

static const char *TAG = "SYSTEM_SCREENS";

/* =============================
 *  AUTO CONTROL SCREEN
 * ============================= */

static lv_obj_t* auto_control_create(void *params)
{
    ESP_LOGI(TAG, "Creating auto control screen");
    
    screen_base_config_t cfg = {
        .title = "Auto Control",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Auto Control Settings\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  WIFI SETTINGS SCREEN
 * ============================= */

static lv_obj_t* wifi_settings_create(void *params)
{
    ESP_LOGI(TAG, "Creating WiFi settings screen");
    
    screen_base_config_t cfg = {
        .title = "WiFi Settings",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "WiFi Configuration\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  DISPLAY SETTINGS SCREEN
 * ============================= */

static lv_obj_t* display_settings_create(void *params)
{
    ESP_LOGI(TAG, "Creating display settings screen");
    
    screen_base_config_t cfg = {
        .title = "Display Settings",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Display Configuration\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  DATA LOGGER SCREEN
 * ============================= */

static lv_obj_t* data_logger_create(void *params)
{
    ESP_LOGI(TAG, "Creating data logger screen");
    
    screen_base_config_t cfg = {
        .title = "Data Logger",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Data Logger Settings\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  SYSTEM INFO SCREEN
 * ============================= */

static lv_obj_t* system_info_create(void *params)
{
    ESP_LOGI(TAG, "Creating system info screen");
    
    screen_base_config_t cfg = {
        .title = "System Info",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "System Information\n\nESP32-S3\nVersion 3.0");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  RESET CONFIRM SCREEN
 * ============================= */

static lv_obj_t* reset_confirm_create(void *params)
{
    ESP_LOGI(TAG, "Creating reset confirm screen");
    
    screen_base_config_t cfg = {
        .title = "Reset Confirm",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Reset all settings?\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

esp_err_t system_screens_register_all(void)
{
    ESP_LOGI(TAG, "Registering all system screens");
    
    // Auto Control
    screen_config_t auto_cfg = {
        .id = "auto_control",
        .title = "Auto Control",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,  // Освобождать память
        .create_fn = auto_control_create,
    };
    screen_register(&auto_cfg);
    
    // WiFi Settings
    screen_config_t wifi_cfg = {
        .id = "wifi_settings",
        .title = "WiFi Settings",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = wifi_settings_create,
    };
    screen_register(&wifi_cfg);
    
    // Display Settings
    screen_config_t display_cfg = {
        .id = "display_settings",
        .title = "Display Settings",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = display_settings_create,
    };
    screen_register(&display_cfg);
    
    // Data Logger
    screen_config_t logger_cfg = {
        .id = "data_logger",
        .title = "Data Logger",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = data_logger_create,
    };
    screen_register(&logger_cfg);
    
    // System Info
    screen_config_t info_cfg = {
        .id = "system_info",
        .title = "System Info",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = system_info_create,
    };
    screen_register(&info_cfg);
    
    // Reset Confirm
    screen_config_t reset_cfg = {
        .id = "reset_confirm",
        .title = "Reset Confirm",
        .category = SCREEN_CATEGORY_DIALOG,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = reset_confirm_create,
    };
    screen_register(&reset_cfg);
    
    ESP_LOGI(TAG, "All 6 system screens registered");
    return ESP_OK;
}

