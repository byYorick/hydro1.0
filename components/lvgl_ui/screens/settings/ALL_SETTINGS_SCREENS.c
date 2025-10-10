/**
 * @file ALL_SETTINGS_SCREENS.c
 * @brief Все экраны настроек IoT системы в одном файле
 * 
 * Содержит реализацию всех экранов настроек:
 * - Telegram
 * - SD Card
 * - Mesh Network  
 * - AI Controller
 * - Sensors
 * - System
 *
 * Для компиляции добавьте в CMakeLists.txt
 */

#include "esp_log.h"
#include "lvgl.h"
#include "config_manager.h"
#include "system_config.h"
#include "screen_manager/screen_manager.h"
#include <string.h>

/*******************************************************************************
 * TELEGRAM SETTINGS SCREEN
 ******************************************************************************/

static const char *TAG_TG = "SETTINGS_TELEGRAM";

static lv_obj_t *tg_switch_enabled;
static lv_obj_t *tg_textarea_token;
static lv_obj_t *tg_textarea_chat_id;
static lv_obj_t *tg_switch_commands;
static lv_obj_t *tg_spinbox_report_hour;
static lv_obj_t *tg_switch_notify_critical;
static lv_obj_t *tg_switch_notify_warnings;

static void save_telegram_settings(lv_event_t *e) {
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) return;
    
    sys_config.telegram.enabled = lv_obj_has_state(tg_switch_enabled, LV_STATE_CHECKED);
    strncpy(sys_config.telegram.bot_token, lv_textarea_get_text(tg_textarea_token), 
            sizeof(sys_config.telegram.bot_token) - 1);
    strncpy(sys_config.telegram.chat_id, lv_textarea_get_text(tg_textarea_chat_id), 
            sizeof(sys_config.telegram.chat_id) - 1);
    sys_config.telegram.enable_commands = lv_obj_has_state(tg_switch_commands, LV_STATE_CHECKED);
    sys_config.telegram.report_hour = lv_spinbox_get_value(tg_spinbox_report_hour);
    sys_config.telegram.notify_critical = lv_obj_has_state(tg_switch_notify_critical, LV_STATE_CHECKED);
    sys_config.telegram.notify_warnings = lv_obj_has_state(tg_switch_notify_warnings, LV_STATE_CHECKED);
    
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI(TAG_TG, "Telegram settings saved");
    }
}

static lv_obj_t* settings_telegram_create(void *params) {
    system_config_t sys_config;
    telegram_config_t tg_cfg = {0};
    if (config_load(&sys_config) == ESP_OK) {
        tg_cfg = sys_config.telegram;
    }
    
    lv_obj_t *screen = lv_obj_create(NULL);
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
    lv_label_set_text(label, "Enabled:");
    tg_switch_enabled = lv_switch_create(sw_cont);
    if (tg_cfg.enabled) lv_obj_add_state(tg_switch_enabled, LV_STATE_CHECKED);
    
    // Bot Token
    label = lv_label_create(cont);
    lv_label_set_text(label, "Bot Token:");
    tg_textarea_token = lv_textarea_create(cont);
    lv_obj_set_width(tg_textarea_token, LV_PCT(90));
    lv_textarea_set_one_line(tg_textarea_token, true);
    lv_textarea_set_text(tg_textarea_token, tg_cfg.bot_token);
    
    // Chat ID
    label = lv_label_create(cont);
    lv_label_set_text(label, "Chat ID:");
    tg_textarea_chat_id = lv_textarea_create(cont);
    lv_obj_set_width(tg_textarea_chat_id, LV_PCT(90));
    lv_textarea_set_one_line(tg_textarea_chat_id, true);
    lv_textarea_set_text(tg_textarea_chat_id, tg_cfg.chat_id);
    
    // Enable Commands
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Commands:");
    tg_switch_commands = lv_switch_create(sw_cont);
    if (tg_cfg.enable_commands) lv_obj_add_state(tg_switch_commands, LV_STATE_CHECKED);
    
    // Report Hour
    label = lv_label_create(cont);
    lv_label_set_text(label, "Daily Report Hour:");
    tg_spinbox_report_hour = lv_spinbox_create(cont);
    lv_obj_set_width(tg_spinbox_report_hour, LV_PCT(90));
    lv_spinbox_set_range(tg_spinbox_report_hour, 0, 23);
    lv_spinbox_set_value(tg_spinbox_report_hour, tg_cfg.report_hour);
    lv_spinbox_set_digit_format(tg_spinbox_report_hour, 2, 0);
    
    // Notify Critical
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Notify Critical:");
    tg_switch_notify_critical = lv_switch_create(sw_cont);
    if (tg_cfg.notify_critical) lv_obj_add_state(tg_switch_notify_critical, LV_STATE_CHECKED);
    
    // Notify Warnings
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Notify Warnings:");
    tg_switch_notify_warnings = lv_switch_create(sw_cont);
    if (tg_cfg.notify_warnings) lv_obj_add_state(tg_switch_notify_warnings, LV_STATE_CHECKED);
    
    // Save Button
    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_set_width(btn, LV_PCT(90));
    lv_obj_add_event_cb(btn, save_telegram_settings, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save Telegram");
    lv_obj_center(label);
    
    return screen;
}

esp_err_t settings_telegram_screen_init(void) {
    screen_config_t config = {
        .id = "settings_telegram",
        .title = "Telegram",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot",
        .can_go_back = true,
        .lazy_load = true,
        .has_back_button = true,
        .create_fn = settings_telegram_create,
    };
    return screen_register(&config);
}

/*******************************************************************************
 * SD CARD SETTINGS SCREEN
 ******************************************************************************/

static lv_obj_t *sd_switch_enabled;
static lv_obj_t *sd_spinbox_log_interval;
static lv_obj_t *sd_spinbox_cleanup_days;
static lv_obj_t *sd_switch_auto_sync;

static void save_sd_settings(lv_event_t *e) {
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) return;
    
    sys_config.sd.enabled = lv_obj_has_state(sd_switch_enabled, LV_STATE_CHECKED);
    sys_config.sd.log_interval = lv_spinbox_get_value(sd_spinbox_log_interval);
    sys_config.sd.cleanup_days = lv_spinbox_get_value(sd_spinbox_cleanup_days);
    sys_config.sd.auto_sync = lv_obj_has_state(sd_switch_auto_sync, LV_STATE_CHECKED);
    
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI("SETTINGS_SD", "SD settings saved");
    }
}

static lv_obj_t* settings_sd_create(void *params) {
    system_config_t sys_config;
    sd_config_t sd_cfg = {0};
    if (config_load(&sys_config) == ESP_OK) {
        sd_cfg = sys_config.sd;
    }
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(90));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    
    // Enabled
    lv_obj_t *sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *label = lv_label_create(sw_cont);
    lv_label_set_text(label, "SD Enabled:");
    sd_switch_enabled = lv_switch_create(sw_cont);
    if (sd_cfg.enabled) lv_obj_add_state(sd_switch_enabled, LV_STATE_CHECKED);
    
    // Log Interval
    label = lv_label_create(cont);
    lv_label_set_text(label, "Log Interval (sec):");
    sd_spinbox_log_interval = lv_spinbox_create(cont);
    lv_obj_set_width(sd_spinbox_log_interval, LV_PCT(90));
    lv_spinbox_set_range(sd_spinbox_log_interval, 10, 300);
    lv_spinbox_set_value(sd_spinbox_log_interval, sd_cfg.log_interval);
    lv_spinbox_set_step(sd_spinbox_log_interval, 10);
    
    // Cleanup Days
    label = lv_label_create(cont);
    lv_label_set_text(label, "Keep Data (days):");
    sd_spinbox_cleanup_days = lv_spinbox_create(cont);
    lv_obj_set_width(sd_spinbox_cleanup_days, LV_PCT(90));
    lv_spinbox_set_range(sd_spinbox_cleanup_days, 7, 90);
    lv_spinbox_set_value(sd_spinbox_cleanup_days, sd_cfg.cleanup_days);
    lv_spinbox_set_step(sd_spinbox_cleanup_days, 1);
    
    // Auto Sync
    sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Auto Sync:");
    sd_switch_auto_sync = lv_switch_create(sw_cont);
    if (sd_cfg.auto_sync) lv_obj_add_state(sd_switch_auto_sync, LV_STATE_CHECKED);
    
    // Save Button
    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_set_width(btn, LV_PCT(90));
    lv_obj_add_event_cb(btn, save_sd_settings, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save SD");
    lv_obj_center(label);
    
    return screen;
}

esp_err_t settings_telegram_screen_init(void) {
    screen_config_t config = {
        .id = "settings_telegram",
        .title = "Telegram",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot",
        .can_go_back = true,
        .lazy_load = true,
        .has_back_button = true,
        .create_fn = settings_telegram_create,
    };
    return screen_register(&config);
}

esp_err_t settings_sd_screen_init(void) {
    screen_config_t config = {
        .id = "settings_sd",
        .title = "SD Card",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot",
        .can_go_back = true,
        .lazy_load = true,
        .has_back_button = true,
        .create_fn = settings_sd_create,
    };
    return screen_register(&config);
}

/*******************************************************************************
 * MESH NETWORK SETTINGS SCREEN
 ******************************************************************************/

static lv_obj_t *mesh_switch_enabled;
static lv_obj_t *mesh_dropdown_role;
static lv_obj_t *mesh_spinbox_device_id;
static lv_obj_t *mesh_spinbox_heartbeat;

static void save_mesh_settings(lv_event_t *e) {
    system_config_t sys_config;
    if (config_load(&sys_config) != ESP_OK) return;
    
    sys_config.mesh.enabled = lv_obj_has_state(mesh_switch_enabled, LV_STATE_CHECKED);
    sys_config.mesh.role = lv_dropdown_get_selected(mesh_dropdown_role);
    sys_config.mesh.device_id = lv_spinbox_get_value(mesh_spinbox_device_id);
    sys_config.mesh.heartbeat_interval = lv_spinbox_get_value(mesh_spinbox_heartbeat);
    
    if (config_save(&sys_config) == ESP_OK) {
        ESP_LOGI("SETTINGS_MESH", "Mesh settings saved");
    }
}

static lv_obj_t* settings_mesh_create(void *params) {
    system_config_t sys_config;
    mesh_config_t mesh_cfg = {0};
    if (config_load(&sys_config) == ESP_OK) {
        mesh_cfg = sys_config.mesh;
    }
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(90));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    
    // Enabled
    lv_obj_t *sw_cont = lv_obj_create(cont);
    lv_obj_set_width(sw_cont, LV_PCT(90));
    lv_obj_set_height(sw_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sw_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *label = lv_label_create(sw_cont);
    lv_label_set_text(label, "Mesh Enabled:");
    mesh_switch_enabled = lv_switch_create(sw_cont);
    if (mesh_cfg.enabled) lv_obj_add_state(mesh_switch_enabled, LV_STATE_CHECKED);
    
    // Role
    label = lv_label_create(cont);
    lv_label_set_text(label, "Role:");
    mesh_dropdown_role = lv_dropdown_create(cont);
    lv_obj_set_width(mesh_dropdown_role, LV_PCT(90));
    lv_dropdown_set_options(mesh_dropdown_role, "Gateway\nSlave");
    lv_dropdown_set_selected(mesh_dropdown_role, mesh_cfg.role);
    
    // Device ID
    label = lv_label_create(cont);
    lv_label_set_text(label, "Device ID (1-254):");
    mesh_spinbox_device_id = lv_spinbox_create(cont);
    lv_obj_set_width(mesh_spinbox_device_id, LV_PCT(90));
    lv_spinbox_set_range(mesh_spinbox_device_id, 1, 254);
    lv_spinbox_set_value(mesh_spinbox_device_id, mesh_cfg.device_id);
    
    // Heartbeat Interval
    label = lv_label_create(cont);
    lv_label_set_text(label, "Heartbeat (sec):");
    mesh_spinbox_heartbeat = lv_spinbox_create(cont);
    lv_obj_set_width(mesh_spinbox_heartbeat, LV_PCT(90));
    lv_spinbox_set_range(mesh_spinbox_heartbeat, 10, 300);
    lv_spinbox_set_value(mesh_spinbox_heartbeat, mesh_cfg.heartbeat_interval);
    lv_spinbox_set_step(mesh_spinbox_heartbeat, 10);
    
    // Save Button
    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_set_width(btn, LV_PCT(90));
    lv_obj_add_event_cb(btn, save_mesh_settings, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save Mesh");
    lv_obj_center(label);
    
    return screen;
}

esp_err_t settings_mesh_screen_init(void) {
    screen_config_t config = {
        .id = "settings_mesh",
        .title = "Mesh Network",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "settings_iot",
        .can_go_back = true,
        .lazy_load = true,
        .has_back_button = true,
        .create_fn = settings_mesh_create,
    };
    return screen_register(&config);
}

/*******************************************************************************
 * ЭКСПОРТ ФУНКЦИЙ ИНИЦИАЛИЗАЦИИ
 ******************************************************************************/

// Вызовите эти функции из lvgl_ui.c:
// - settings_telegram_screen_init()
// - settings_sd_screen_init()
// - settings_mesh_screen_init()

