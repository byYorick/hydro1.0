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
    ESP_LOGI(TAG, "[7/7] Initializing notification popup system...");
    widget_notification_popup_init();
    ESP_LOGI(TAG, "[OK] Notification popup system initialized");
    
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

