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
#include "lcd_ili9341.h"
#include "config_manager.h"

static const char *TAG = "SYSTEM_SCREENS";

/* =============================
 *  ОБЩИЙ CALLBACK для всех системных экранов
 * ============================= */

/**
 * @brief Универсальный callback для добавления кнопки "Назад" в группу
 */
static esp_err_t system_screen_on_show(lv_obj_t *screen_obj, void *params)
{
    const char *screen_id = (const char*)params;
    if (!screen_id) {
        ESP_LOGW(TAG, "No screen ID in params");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "System screen '%s' shown - configuring encoder", screen_id);
    
    // Получаем instance для доступа к encoder_group
    screen_instance_t *inst = screen_get_by_id(screen_id);
    if (!inst || !inst->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available for %s", screen_id);
        return ESP_OK;
    }
    
    lv_group_t *group = inst->encoder_group;
    
    // Добавляем все интерактивные элементы
    lv_obj_t *child = lv_obj_get_child(screen_obj, 0);
    int added = 0;
    
    while (child != NULL) {
        if (lv_obj_check_type(child, &lv_button_class)) {
            lv_group_add_obj(group, child);
            added++;
        }
        else if (lv_obj_check_type(child, &lv_slider_class)) {
            lv_group_add_obj(group, child);
            added++;
        }
        else if (lv_obj_check_type(child, &lv_dropdown_class)) {
            lv_group_add_obj(group, child);
            added++;
        }
        else if (lv_obj_check_type(child, &lv_checkbox_class)) {
            lv_group_add_obj(group, child);
            added++;
        }
        
        // Вложенные элементы
        lv_obj_t *grandchild = lv_obj_get_child(child, 0);
        while (grandchild != NULL) {
            if (lv_obj_check_type(grandchild, &lv_button_class) ||
                lv_obj_check_type(grandchild, &lv_slider_class) ||
                lv_obj_check_type(grandchild, &lv_dropdown_class) ||
                lv_obj_check_type(grandchild, &lv_checkbox_class)) {
                lv_group_add_obj(group, grandchild);
                added++;
            }
            grandchild = lv_obj_get_child(child, lv_obj_get_index(grandchild) + 1);
        }
        
        child = lv_obj_get_child(screen_obj, lv_obj_get_index(child) + 1);
    }
    
    int obj_count = lv_group_get_obj_count(group);
    ESP_LOGI(TAG, "  Encoder group: %d objects (added %d)", obj_count, added);
    
    // Устанавливаем фокус
    if (obj_count > 0) {
        lv_group_focus_next(group);
        ESP_LOGI(TAG, "  Focus set");
    }
    
    return ESP_OK;
}

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

// Callback для изменения яркости
static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
    
    int32_t value = lv_slider_get_value(slider);
    
    // Обновляем текст
    lv_label_set_text_fmt(label, "%d%%", (int)value);
    
    // Применяем яркость
    lcd_ili9341_set_brightness((uint8_t)value);
    
    // Сохраняем в конфигурацию
    system_config_t config;
    if (config_load(&config) == ESP_OK) {
        config.display_brightness = (uint8_t)value;
        esp_err_t ret = config_save(&config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save brightness: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Brightness saved: %d%%", (int)value);
        }
    }
}

static lv_obj_t* display_settings_create(void *params)
{
    ESP_LOGI(TAG, "Creating display settings screen");
    
    screen_base_config_t cfg = {
        .title = "Display",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    // Получаем текущую яркость
    uint8_t current_brightness = lcd_ili9341_get_brightness();
    
    // Контейнер для настроек яркости
    lv_obj_t *container = lv_obj_create(base.content);
    lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1A2332), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 15, 0);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 10);
    
    // Заголовок "Яркость"
    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, "Brightness");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Метка со значением яркости
    lv_obj_t *value_label = lv_label_create(container);
    lv_label_set_text_fmt(value_label, "%d%%", current_brightness);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    // Слайдер яркости
    lv_obj_t *slider = lv_slider_create(container);
    lv_obj_set_width(slider, LV_PCT(100));
    lv_slider_set_range(slider, 10, 100);  // От 10% до 100%
    lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 40);
    
    // Стиль слайдера
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2D3E50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x00D4AA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x00D4AA), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB);
    
    // Добавляем событие
    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, value_label);
    
    // Подсказка
    lv_obj_t *hint_label = lv_label_create(container);
    lv_label_set_text(hint_label, "Rotate encoder to adjust");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_align(hint_label, LV_ALIGN_TOP_MID, 0, 70);
    
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
 *  SYSTEM STATUS SCREEN
 * ============================= */

static lv_obj_t* system_status_create(void *params)
{
    ESP_LOGI(TAG, "Creating system status screen");
    
    screen_base_config_t cfg = {
        .title = "System Status",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "System Status\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

esp_err_t system_screens_register_all(void)
{
    ESP_LOGI(TAG, "Registering all system screens");
    
    // System Status
    screen_config_t status_cfg = {
        .id = "system_status",
        .title = "System Status",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = system_status_create,
        .on_show = system_screen_on_show,
        .user_data = (void*)"system_status",
    };
    screen_register(&status_cfg);
    
    // Auto Control
    screen_config_t auto_cfg = {
        .id = "auto_control",
        .title = "Auto Control",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = auto_control_create,
        .on_show = system_screen_on_show,
        .user_data = (void*)"auto_control",
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
        .on_show = system_screen_on_show,
        .user_data = (void*)"wifi_settings",
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
        .on_show = system_screen_on_show,
        .user_data = (void*)"display_settings",
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
        .on_show = system_screen_on_show,
        .user_data = (void*)"data_logger",
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
        .on_show = system_screen_on_show,
        .user_data = (void*)"system_info",
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
        .on_show = system_screen_on_show,
        .user_data = (void*)"reset_confirm",
    };
    screen_register(&reset_cfg);
    
    ESP_LOGI(TAG, "All 7 system screens registered");
    return ESP_OK;
}
