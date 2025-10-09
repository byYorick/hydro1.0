/**
 * @file notification_popup.c
 * @brief Адаптер для показа уведомлений через Screen Manager
 */

#include "notification_popup.h"
#include "screens/popup_screen.h"
#include "esp_log.h"

static const char *TAG = "NOTIF_POPUP";

// Время автоскрытия попапа (мс)
#define POPUP_AUTO_HIDE_MS 5000

/**
 * @brief Callback от notification_system - показывает попап через Screen Manager
 */
static void notification_callback(const notification_t *notification)
{
    if (!notification) {
        return;
    }
    
    // Показываем только важные уведомления
    if (notification->priority >= NOTIF_PRIORITY_NORMAL) {
        // Определяем таймаут в зависимости от приоритета
        uint32_t timeout = POPUP_AUTO_HIDE_MS; // 5 секунд по умолчанию
        if (notification->priority == NOTIF_PRIORITY_URGENT) {
            timeout = 8000; // 8 секунд для срочных
        } else if (notification->priority == NOTIF_PRIORITY_LOW) {
            timeout = 3000; // 3 секунды для низких
        }
        
        ESP_LOGI(TAG, "Forwarding notification to popup_screen: [%d] %s", 
                 notification->type, notification->message);
        
        // Показываем через popup_screen (управляется Screen Manager)
        popup_show_notification(notification, timeout);
    }
}

void widget_notification_popup_init(void)
{
    // Регистрируем callback в notification_system
    esp_err_t ret = notification_register_callback(notification_callback);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Notification popup adapter initialized (via Screen Manager)");
    } else {
        ESP_LOGW(TAG, "Failed to register notification callback: %s", 
                 esp_err_to_name(ret));
    }
}
