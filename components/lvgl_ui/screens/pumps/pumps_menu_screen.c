/**
 * @file pumps_menu_screen.c
 * @brief Главное меню управления насосами
 */

#include "pumps_menu_screen.h"
#include "../../screen_manager/screen_manager.h"
#include "../base/screen_template.h"
#include "../../widgets/menu_list.h"
#include "esp_log.h"

static const char *TAG = "PUMPS_MENU";

/* =============================
 *  MENU CALLBACKS
 * ============================= */

static void on_pumps_status_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "Pumps Status clicked (event: %d)", code);
        screen_show("pumps_status", NULL);
    }
}

static void on_pumps_manual_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "Manual Control clicked (event: %d)", code);
        screen_show("pumps_manual", NULL);
    }
}

static void on_pump_calibration_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "Pump Calibration clicked (event: %d)", code);
        screen_show("pump_calibration", NULL);
    }
}

static void on_pid_main_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "PID Settings clicked (event: %d)", code);
        screen_show("pid_intelligent_dashboard", NULL);  // Новый интеллектуальный PID
    }
}

/* =============================
 *  SCREEN IMPLEMENTATION
 * ============================= */

static lv_obj_t* pumps_menu_screen_create(void *params)
{
    (void)params;
    ESP_LOGI(TAG, "Creating pumps menu screen");
    
    // Определяем пункты меню
    menu_item_config_t menu_items[] = {
        {
            .text = "Статус насосов",
            .icon = LV_SYMBOL_LIST,  // Используем LIST вместо EYE
            .callback = on_pumps_status_click,
            .user_data = NULL,
        },
        {
            .text = "Ручное управление",
            .icon = LV_SYMBOL_PLAY,
            .callback = on_pumps_manual_click,
            .user_data = NULL,
        },
        {
            .text = "Калибровка",
            .icon = LV_SYMBOL_SETTINGS,
            .callback = on_pump_calibration_click,
            .user_data = NULL,
        },
        {
            .text = "PID настройки",
            .icon = LV_SYMBOL_EDIT,
            .callback = on_pid_main_click,
            .user_data = NULL,
        },
    };
    
    // Создаем экран меню используя шаблон
    template_menu_config_t menu_cfg = {
        .title = "Насосы",
        .items = menu_items,
        .item_count = 4,
        .has_back_button = true,
        .back_callback = NULL,
    };
    
    lv_obj_t *screen = template_create_menu_screen(&menu_cfg, NULL);
    
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create pumps menu screen");
        return NULL;
    }
    
    ESP_LOGD(TAG, "Pumps menu screen created successfully");
    return screen;
}

static esp_err_t pumps_menu_screen_on_show(lv_obj_t *screen, void *params)
{
    (void)screen;
    (void)params;
    ESP_LOGI(TAG, "Pumps menu shown");
    return ESP_OK;
}

static esp_err_t pumps_menu_screen_on_hide(lv_obj_t *screen)
{
    (void)screen;
    ESP_LOGI(TAG, "Pumps menu hidden");
    return ESP_OK;
}

/* =============================
 *  REGISTRATION
 * ============================= */

void pumps_menu_screen_register(void)
{
    ESP_LOGI(TAG, "Initializing pumps menu screen");
    
    screen_config_t config = {
        .id = "pumps_menu",
        .title = "Pumps",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = false,  // Кешируем экран
        .create_fn = pumps_menu_screen_create,
        .on_show = pumps_menu_screen_on_show,
        .on_hide = pumps_menu_screen_on_hide,
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register pumps menu screen: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Pumps menu registered successfully");
}

