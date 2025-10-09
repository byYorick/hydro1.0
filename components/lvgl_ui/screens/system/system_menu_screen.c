/**
 * @file system_menu_screen.c
 * @brief Реализация экрана системного меню
 */

#include "system_menu_screen.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"
#include "widgets/menu_list.h"
#include "esp_log.h"

static const char *TAG = "SYSTEM_MENU";

/* =============================
 *  CALLBACKS
 * ============================= */

static void on_auto_control_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Auto Control clicked");
    screen_show("auto_control", NULL);
}

static void on_wifi_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "WiFi Settings clicked");
    screen_show("wifi_settings", NULL);
}

static void on_display_settings_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Display Settings clicked");
    screen_show("display_settings", NULL);
}

static void on_data_logger_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Data Logger clicked");
    screen_show("data_logger", NULL);
}

static void on_system_info_click(lv_event_t *e) {
    ESP_LOGI(TAG, "System Info clicked");
    screen_show("system_info", NULL);
}

static void on_reset_click(lv_event_t *e) {
    ESP_LOGI(TAG, "Reset clicked");
    screen_show("reset_confirm", NULL);
}

/**
 * @brief Callback при показе системного меню
 */
static esp_err_t system_menu_on_show(lv_obj_t *screen_obj, void *params)
{
    ESP_LOGI(TAG, "System menu shown - configuring encoder");
    
    // Получаем instance для доступа к encoder_group
    screen_instance_t *inst = screen_get_by_id("system_menu");
    if (!inst || !inst->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available");
        return ESP_OK;
    }
    
    lv_group_t *group = inst->encoder_group;
    
    // Добавляем все кнопки меню в группу
    lv_obj_t *child = lv_obj_get_child(screen_obj, 0);
    int added = 0;
    
    while (child != NULL) {
        if (lv_obj_check_type(child, &lv_button_class)) {
            lv_group_add_obj(group, child);
            added++;
            ESP_LOGD(TAG, "  Added button to encoder group");
        }
        
        // Проверяем вложенные элементы
        lv_obj_t *grandchild = lv_obj_get_child(child, 0);
        while (grandchild != NULL) {
            if (lv_obj_check_type(grandchild, &lv_button_class)) {
                lv_group_add_obj(group, grandchild);
                added++;
                ESP_LOGD(TAG, "  Added nested button");
            }
            grandchild = lv_obj_get_child(child, lv_obj_get_index(grandchild) + 1);
        }
        
        child = lv_obj_get_child(screen_obj, lv_obj_get_index(child) + 1);
    }
    
    int obj_count = lv_group_get_obj_count(group);
    ESP_LOGI(TAG, "  Encoder group has %d objects (added %d)", obj_count, added);
    
    // Устанавливаем фокус
    if (obj_count > 0) {
        lv_group_focus_next(group);
        ESP_LOGI(TAG, "  Initial focus set");
    }
    
    return ESP_OK;
}

/* =============================
 *  СОЗДАНИЕ ЭКРАНА
 * ============================= */

static lv_obj_t* system_menu_create(void *params)
{
    ESP_LOGI(TAG, "Creating system menu screen");
    
    // Пункты системного меню
    menu_item_config_t items[] = {
        {
            .text = "Auto Control",
            .icon = LV_SYMBOL_PLAY,
            .callback = on_auto_control_click,
            .user_data = NULL,
        },
        {
            .text = "WiFi Settings",
            .icon = LV_SYMBOL_WIFI,
            .callback = on_wifi_settings_click,
            .user_data = NULL,
        },
        {
            .text = "Display Settings",
            .icon = LV_SYMBOL_IMAGE,
            .callback = on_display_settings_click,
            .user_data = NULL,
        },
        {
            .text = "Data Logger",
            .icon = LV_SYMBOL_SD_CARD,
            .callback = on_data_logger_click,
            .user_data = NULL,
        },
        {
            .text = "System Info",
            .icon = LV_SYMBOL_LIST,
            .callback = on_system_info_click,
            .user_data = NULL,
        },
        {
            .text = "Reset Settings",
            .icon = LV_SYMBOL_REFRESH,
            .callback = on_reset_click,
            .user_data = NULL,
        },
    };
    
    // Используем шаблон меню (без группы - настроится в on_show)
    template_menu_config_t menu_cfg = {
        .title = "System Settings",
        .items = items,
        .item_count = 6,
        .has_back_button = true,
        .back_callback = NULL,  // Автоматически вернется к main
    };
    
    return template_create_menu_screen(&menu_cfg, NULL);
}

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

esp_err_t system_menu_screen_init(void)
{
    ESP_LOGI(TAG, "Initializing system menu screen");
    
    screen_config_t config = {
        .id = "system_menu",
        .title = "System Settings",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "main",            // Возврат на главный экран
        .can_go_back = true,
        .lazy_load = true,
        .cache_on_hide = true,          // Кэшировать для быстрого доступа
        .destroy_on_hide = false,
        .has_status_bar = true,
        .has_back_button = true,
        .create_fn = system_menu_create,
        .on_show = system_menu_on_show,  // Настройка encoder group
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register system menu: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "System menu registered successfully");
    return ESP_OK;
}
