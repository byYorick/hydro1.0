/**
 * @file screen_init.c
 * @brief Централизованная инициализация всех экранов
 */

#include "screen_init.h"
#include "screen_manager.h"
#include "../screens/main_screen.h"
#include "../screens/sensor/sensor_detail_screen.h"
#include "../screens/sensor/sensor_settings_screen.h"
#include "../screens/system/system_menu_screen.h"
#include "../screens/system/system_screens.h"
#include "../screens/popup_screen.h"
#include "../screens/pumps/pumps_menu_screen.h"
#include "../screens/pumps/pumps_status_screen.h"
#include "../screens/pumps/pumps_manual_screen.h"
#include "../screens/pumps/pump_calibration_screen.h"
#include "../screens/pid/pid_main_screen.h"
#include "../screens/pid/pid_detail_screen.h"
#include "../screens/pid/pid_tuning_screen.h"
#include "../screens/pid/pid_advanced_screen.h"
#include "../screens/pid/pid_thresholds_screen.h"
#include "../screens/pid/pid_graph_screen.h"
#include "../widgets/notification_popup.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_INIT";

esp_err_t screen_system_init_all(void)
{
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "   Initializing Screen Manager System          ");
    ESP_LOGI(TAG, "========================================================");
    
    // 1. Инициализация Screen Manager
    ESP_LOGI(TAG, "[1/6] Initializing Screen Manager Core...");
    esp_err_t ret = screen_manager_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Screen Manager: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] Screen Manager Core initialized");
    
    // 2. Регистрация главного экрана
    ESP_LOGI(TAG, "[2/6] Registering main screen...");
    ret = main_screen_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register main screen: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] Main screen registered");
    
    // 3. Регистрация экранов датчиков (детализация)
    ESP_LOGI(TAG, "[3/6] Registering sensor detail screens...");
    ret = sensor_detail_screens_register_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register detail screens: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] 6 sensor detail screens registered");
    
    // 4. Регистрация экранов настроек датчиков
    ESP_LOGI(TAG, "[4/6] Registering sensor settings screens...");
    ret = sensor_settings_screens_register_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register settings screens: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] 6 sensor settings screens registered");
    
    // 5. Регистрация системных экранов
    ESP_LOGI(TAG, "[5/6] Registering system screens...");
    ret = system_menu_screen_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register system menu: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = system_screens_register_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register system screens: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] 7 system screens registered");
    
    // 6. Регистрация popup экрана (управляемый через Screen Manager)
    ESP_LOGI(TAG, "[6/7] Registering popup screen...");
    popup_screen_register();
    ESP_LOGI(TAG, "[OK] Popup screen registered");
    
    // 7. КРИТИЧНО: Инициализация системы попапов уведомлений
    ESP_LOGI(TAG, "[7/9] Initializing notification popup system...");
    widget_notification_popup_init();
    ESP_LOGI(TAG, "[OK] Notification popup system initialized");
    
    // 8. Регистрация экранов насосов
    ESP_LOGI(TAG, "[8/9] Registering pump screens...");
    
    // Главное меню насосов
    pumps_menu_screen_register();
    
    screen_config_t pumps_status_cfg = {
        .id = "pumps_status",
        .title = "Pumps Status",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "pumps_menu",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pumps_status_screen_create,
        .on_show = pumps_status_screen_on_show,
        .on_hide = pumps_status_screen_on_hide,
    };
    screen_register(&pumps_status_cfg);
    
    screen_config_t pumps_manual_cfg = {
        .id = "pumps_manual",
        .title = "Manual Control",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "pumps_menu",
        .can_go_back = true,
        .lazy_load = false,  // Часто используется - создаем сразу
        .destroy_on_hide = false,  // Кешируем
        .create_fn = pumps_manual_screen_create,
    };
    screen_register(&pumps_manual_cfg);
    
    screen_config_t pump_calib_cfg = {
        .id = "pump_calibration",
        .title = "Pump Calibration",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "pumps_menu",
        .can_go_back = true,
        .lazy_load = false,  // Часто используется - создаем сразу  
        .destroy_on_hide = false,  // Кешируем
        .create_fn = pump_calibration_screen_create,
    };
    screen_register(&pump_calib_cfg);
    
    ESP_LOGI(TAG, "[OK] 4 pump screens registered");
    
    // 9. Регистрация экранов PID
    ESP_LOGI(TAG, "[9/9] Registering PID screens...");
    
    screen_config_t pid_main_cfg = {
        .id = "pid_main",
        .title = "PID Controllers",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "pumps_menu",
        .can_go_back = true,
        .lazy_load = false,  // Часто используется - создаем сразу
        .cache_on_hide = true,
        .destroy_on_hide = false,
        .create_fn = pid_main_screen_create,
        .on_show = pid_main_screen_on_show,
        .on_hide = pid_main_screen_on_hide,
    };
    screen_register(&pid_main_cfg);
    
    screen_config_t pid_detail_cfg = {
        .id = "pid_detail",
        .title = "PID Detail",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "pid_main",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pid_detail_screen_create,
    };
    screen_register(&pid_detail_cfg);
    
    screen_config_t pid_tuning_cfg = {
        .id = "pid_tuning",
        .title = "PID Tuning",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "pid_detail",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pid_tuning_screen_create,
    };
    screen_register(&pid_tuning_cfg);
    
    screen_config_t pid_advanced_cfg = {
        .id = "pid_advanced",
        .title = "PID Advanced",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "pid_detail",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pid_advanced_screen_create,
    };
    screen_register(&pid_advanced_cfg);
    
    screen_config_t pid_thresh_cfg = {
        .id = "pid_thresholds",
        .title = "PID Thresholds",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "pid_advanced",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pid_thresholds_screen_create,
    };
    screen_register(&pid_thresh_cfg);
    
    screen_config_t pid_graph_cfg = {
        .id = "pid_graph",
        .title = "PID Graph",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "pid_detail",
        .can_go_back = true,
        .lazy_load = true,
        .destroy_on_hide = true,
        .create_fn = pid_graph_screen_create,
    };
    screen_register(&pid_graph_cfg);
    
    ESP_LOGI(TAG, "[OK] 6 PID screens registered");
    
    // Итоговая статистика
    uint8_t total = screen_get_registered_count();
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "   Screen System Initialization Complete!      ");
    ESP_LOGI(TAG, "   Total screens registered: %-2d                 ", total);
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Screens registered:");
    ESP_LOGI(TAG, "  - Main screen: 1");
    ESP_LOGI(TAG, "  - Sensor details: 6");
    ESP_LOGI(TAG, "  - Sensor settings: 6");
    ESP_LOGI(TAG, "  - System menu: 1");
    ESP_LOGI(TAG, "  - System settings: 6");
    ESP_LOGI(TAG, "  - Pump screens: 4");
    ESP_LOGI(TAG, "  - PID screens: 6");
    ESP_LOGI(TAG, "");
    
    // 6. Показываем главный экран
    ESP_LOGI(TAG, "Showing main screen...");
    ret = screen_show("main", NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to show main screen: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "[OK] Main screen shown");
    
    ESP_LOGI(TAG, "Screen Manager System ready!");
    
    return ESP_OK;
}

