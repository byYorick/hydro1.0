/**
 * @file wifi_settings_screen.c
 * @brief Реализация экрана настроек WiFi с полным функционалом
 */

#include "wifi_settings_screen.h"
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_base.h"
#include "widgets/back_button.h"
#include "widgets/event_helpers.h"
#include "lvgl_styles.h"
#include "notification_system.h"
#include "config_manager.h"
#include "network_manager.h"
// Прямая работа с ESP WiFi API
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
// ФИКС квадратов: Используем встроенный шрифт LVGL
#include "lvgl.h"

static const char *TAG = "WIFI_SCREEN";

#define MAX_NETWORKS 10  // Уменьшили с 16 до 10 для экономии DRAM
#define SCAN_PERIOD_MS 5000

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_status_label = NULL;
static lv_obj_t *g_ip_label = NULL;
static lv_obj_t *g_rssi_label = NULL;
static lv_obj_t *g_network_list = NULL;
static lv_obj_t *g_scan_btn = NULL;
static lv_obj_t *g_connect_btn = NULL;
static lv_obj_t *g_disconnect_btn = NULL;
static lv_obj_t *g_password_textarea = NULL;

// Данные (ОПТИМИЗАЦИЯ: динамическое выделение вместо статических массивов)
static char **g_scanned_networks = NULL;  // Будет выделено динамически
static int g_network_count = 0;
static int g_selected_network_idx = -1;
static bool g_is_scanning = false;
static lv_timer_t *g_status_timer = NULL;

/* =============================
 *  ПРОТОТИПЫ ФУНКЦИЙ
 * ============================= */

static void on_scan_click(lv_event_t *e);
static void on_network_select(lv_event_t *e);
static void on_connect_click(lv_event_t *e);
static void on_disconnect_click(lv_event_t *e);

/* =============================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================= */

/**
 * @brief Обновление статуса WiFi
 */
static void update_wifi_status(void)
{
    if (!g_status_label) return;
    
    wifi_info_t info;
    esp_err_t ret = network_manager_get_info(&info);
    
    if (ret == ESP_OK && info.is_connected) {
        // Подключены к WiFi
        if (info.rssi > -50) {
            lv_label_set_text(g_status_label, "Connected (excellent)");
            lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x4CAF50), 0);
        } else if (info.rssi > -70) {
            lv_label_set_text(g_status_label, "Connected (good)");
            lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xFF9800), 0);
        } else {
            lv_label_set_text(g_status_label, "Connected (weak)");
            lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xF44336), 0);
        }
        
        // Обновляем RSSI
        if (g_rssi_label) {
            char rssi_text[32];
            snprintf(rssi_text, sizeof(rssi_text), "Signal: %d dBm", info.rssi);
            lv_label_set_text(g_rssi_label, rssi_text);
        }
        
        // Обновляем IP
        if (g_ip_label) {
            char ip_text[32];
            snprintf(ip_text, sizeof(ip_text), "IP: %s", info.ip);
            lv_label_set_text(g_ip_label, ip_text);
        }
        
        // Показываем кнопку отключения
        if (g_disconnect_btn) {
            lv_obj_clear_flag(g_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_connect_btn) {
            lv_obj_add_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Не подключены
        lv_label_set_text(g_status_label, "Not connected");
        lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xaaaaaa), 0);
        
        if (g_rssi_label) {
            lv_label_set_text(g_rssi_label, "Signal: N/A");
        }
        if (g_ip_label) {
            lv_label_set_text(g_ip_label, "IP: None");
        }
        
        // Показываем кнопку подключения
        if (g_connect_btn) {
            lv_obj_clear_flag(g_connect_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_disconnect_btn) {
            lv_obj_add_flag(g_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief Таймер обновления статуса
 */
static void status_timer_cb(lv_timer_t *timer)
{
    update_wifi_status();
}

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Асинхронная задача WiFi сканирования (НЕ блокирует UI!)
 */
static void wifi_scan_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi scan task started");
    
    // Выполняем сканирование БЕЗ LVGL lock (не блокируем UI!)
    wifi_scan_result_t *scan_results = malloc(sizeof(wifi_scan_result_t) * MAX_NETWORKS);
    if (!scan_results) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        g_is_scanning = false;
        vTaskDelete(NULL);
        return;
    }
    
    uint16_t count = 0;
    esp_err_t ret = network_manager_scan(scan_results, MAX_NETWORKS, &count);
    
    // Освобождаем старые сети если были
    if (g_scanned_networks) {
        for (int i = 0; i < g_network_count; i++) {
            free(g_scanned_networks[i]);
        }
        free(g_scanned_networks);
        g_scanned_networks = NULL;
    }
    
    if (ret == ESP_OK && count > 0) {
        g_network_count = count;
        
        // Выделяем массив указателей
        g_scanned_networks = malloc(sizeof(char*) * count);
        if (g_scanned_networks) {
            for (int i = 0; i < count; i++) {
                g_scanned_networks[i] = malloc(32);
                if (g_scanned_networks[i]) {
                    strncpy(g_scanned_networks[i], scan_results[i].ssid, 31);
                    g_scanned_networks[i][31] = '\0';
                }
            }
        }
        ESP_LOGI(TAG, "Found %d networks", g_network_count);
    } else {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(ret));
        g_network_count = 0;
    }
    
    free(scan_results);
    
    // КРИТИЧНО: Заполняем список С LVGL LOCK и добавляем в encoder group!
    if (g_network_list && g_network_count > 0) {
        lv_lock();  // ОБЯЗАТЕЛЬНО для UI обновлений из задачи!
        
        // Получаем encoder group текущего экрана
        lv_group_t *group = lv_group_get_default();
        
        for (int i = 0; i < g_network_count; i++) {
            lv_obj_t *btn = lv_btn_create(g_network_list);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, 40);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a3a), 0);
            
            // КРИТИЧНО: Делаем кнопку фокусируемой энкодером!
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            if (group) {
                lv_group_add_obj(group, btn);  // Добавляем в encoder group!
            }
            
            lv_obj_t *label = lv_label_create(btn);
            
            // ФИКС квадратиков: ограничиваем SSID ASCII символами
            char safe_ssid[33];
            strncpy(safe_ssid, g_scanned_networks[i], 32);
            safe_ssid[32] = '\0';
            
            // Заменяем не-ASCII символы на '?'
            for (int j = 0; safe_ssid[j]; j++) {
                if ((unsigned char)safe_ssid[j] > 127) {
                    safe_ssid[j] = '?';
                }
            }
            
            lv_label_set_text(label, safe_ssid);
            lv_obj_center(label);
            
            // Сохраняем индекс сети
            lv_obj_set_user_data(btn, (void*)(intptr_t)i);
            
            // Добавляем обработчик клика
            widget_add_click_handler(btn, on_network_select, (void*)(intptr_t)i);
        }
        
        lv_unlock();
    }
    
    // Восстанавливаем текст кнопки
    lv_lock();
    if (g_scan_btn) {
        lv_obj_t *label = lv_obj_get_child(g_scan_btn, 0);
        if (label) {
            lv_label_set_text(label, "Scan Networks");
        }
    }
    lv_unlock();
    
    g_is_scanning = false;
    
    // notification_system(NOTIFICATION_INFO, "Scan complete", NOTIF_SOURCE_SYSTEM);
    
    ESP_LOGI(TAG, "WiFi scan task completed");
    vTaskDelete(NULL);  // Завершаем задачу
}

/**
 * @brief Callback кнопки сканирования (запускает async задачу)
 */
static void on_scan_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    if (g_is_scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return;
    }
    
    g_is_scanning = true;
    
    // Обновляем текст кнопки
    if (g_scan_btn) {
        lv_obj_t *label = lv_obj_get_child(g_scan_btn, 0);
        if (label) {
            lv_label_set_text(label, "Scanning...");
        }
    }
    
    // Очищаем список
    if (g_network_list) {
        lv_obj_clean(g_network_list);
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan in separate task...");
    
    // КРИТИЧНО: Запускаем сканирование в отдельной задаче чтобы не блокировать UI!
    xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 5, NULL);
}

/**
 * @brief Callback выбора сети
 */
static void on_network_select(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (idx >= 0 && idx < g_network_count) {
        g_selected_network_idx = idx;
        ESP_LOGI(TAG, "Selected network: %s", g_scanned_networks[idx]);
        
        // Подсвечиваем выбранную сеть
        lv_obj_t *list = g_network_list;
        if (list) {
            uint32_t child_cnt = lv_obj_get_child_count(list);
            for (uint32_t i = 0; i < child_cnt; i++) {
                lv_obj_t *child = lv_obj_get_child(list, i);
                if ((int)(intptr_t)lv_obj_get_user_data(child) == idx) {
                    lv_obj_set_style_bg_color(child, lv_color_hex(0x2196F3), 0);
                } else {
                    lv_obj_set_style_bg_color(child, lv_color_hex(0x3a3a3a), 0);
                }
            }
        }
        
        // Фокусируем поле пароля
        if (g_password_textarea) {
            lv_obj_clear_flag(g_password_textarea, LV_OBJ_FLAG_HIDDEN);
        }
        
        // char msg[64];
        // snprintf(msg, sizeof(msg), "Выбрана сеть: %s", g_scanned_networks[idx]);
        // notification_system(NOTIFICATION_INFO, msg, NOTIF_SOURCE_SYSTEM);
        ESP_LOGI(TAG, "Selected network: %s", g_scanned_networks[idx]);
    }
}

/**
 * @brief Callback подключения
 */
static void on_connect_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    if (g_selected_network_idx < 0 || g_selected_network_idx >= g_network_count) {
        // notification_system(NOTIFICATION_WARNING, "Выберите сеть из списка!", NOTIF_SOURCE_SYSTEM);
        ESP_LOGW(TAG, "No network selected");
        return;
    }
    
    if (!g_password_textarea) {
        // notification_system(NOTIFICATION_ERROR, "Ошибка: поле пароля не найдено", NOTIF_SOURCE_SYSTEM);
        ESP_LOGE(TAG, "Password field not found");
        return;
    }
    
    const char *password = lv_textarea_get_text(g_password_textarea);
    
    ESP_LOGI(TAG, "Connecting to %s...", g_scanned_networks[g_selected_network_idx]);
    
    // Подключаемся через network_manager
    esp_err_t ret = network_manager_connect(g_scanned_networks[g_selected_network_idx], password);
    
    if (ret == ESP_OK) {
        // notification_system(NOTIFICATION_INFO, "Подключение к WiFi...", NOTIF_SOURCE_SYSTEM);
        ESP_LOGI(TAG, "Connecting to WiFi...");
        
        // Сохраняем конфигурацию в NVS
        network_manager_save_credentials();
        
        // Обновляем статус через 3 секунды
        vTaskDelay(pdMS_TO_TICKS(3000));
        update_wifi_status();
    } else {
        // notification_system(NOTIFICATION_ERROR, "Ошибка подключения!", NOTIF_SOURCE_SYSTEM);
        ESP_LOGE(TAG, "Connection failed");
    }
}

/**
 * @brief Callback отключения
 */
static void on_disconnect_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    ESP_LOGI(TAG, "Disconnecting WiFi...");
    
    esp_err_t ret = network_manager_disconnect();
    
    if (ret == ESP_OK) {
        // notification_system(NOTIFICATION_INFO, "WiFi отключен", NOTIF_SOURCE_SYSTEM);
        ESP_LOGI(TAG, "WiFi disconnected");
        vTaskDelay(pdMS_TO_TICKS(500));
        update_wifi_status();
    } else {
        // notification_system(NOTIFICATION_ERROR, "Ошибка отключения!", NOTIF_SOURCE_SYSTEM);
        ESP_LOGE(TAG, "Disconnect failed");
    }
}

/* =============================
 *  СОЗДАНИЕ ЭКРАНА
 * ============================= */

lv_obj_t* wifi_settings_screen_create(void *params)
{
    ESP_LOGI(TAG, "Creating WiFi settings screen");
    
    // Создаем базовый экран
    screen_base_config_t cfg = {
        .title = "WiFi",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,
        .back_user_data = NULL
    };
    
    screen_base_t base = screen_base_create(&cfg);
    if (!base.screen) {
        ESP_LOGE(TAG, "Failed to create base screen");
        return NULL;
    }
    
    g_screen = base.screen;
    
    // ФИКС квадратов: Используем встроенный шрифт LVGL для всего экрана WiFi
    lv_obj_set_style_text_font(base.screen, &lv_font_montserrat_14, 0);
    
    // Контейнер с прокруткой
    lv_obj_t *scroll = lv_obj_create(base.content);
    lv_obj_set_size(scroll, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scroll, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 8, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(scroll, 8, 0);
    
    // Статус подключения
    lv_obj_t *status_cont = lv_obj_create(scroll);
    lv_obj_set_size(status_cont, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(status_cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_radius(status_cont, 8, 0);
    lv_obj_set_style_border_width(status_cont, 0, 0);
    lv_obj_set_style_pad_all(status_cont, 10, 0);
    
    lv_obj_t *status_title = lv_label_create(status_cont);
    lv_label_set_text(status_title, "Status:");
    lv_obj_set_style_text_color(status_title, lv_color_hex(0xaaaaaa), 0);
    
    g_status_label = lv_label_create(status_cont);
    lv_label_set_text(g_status_label, "Checking...");
    lv_obj_align(g_status_label, LV_ALIGN_TOP_LEFT, 0, 20);
    
    g_rssi_label = lv_label_create(status_cont);
    lv_label_set_text(g_rssi_label, "Signal: N/A");
    lv_obj_set_style_text_color(g_rssi_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(g_rssi_label, LV_ALIGN_TOP_LEFT, 0, 40);
    
    g_ip_label = lv_label_create(status_cont);
    lv_label_set_text(g_ip_label, "IP: None");
    lv_obj_set_style_text_color(g_ip_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(g_ip_label, LV_ALIGN_TOP_LEFT, 0, 60);
    
    // Кнопка сканирования
    g_scan_btn = lv_btn_create(scroll);
    lv_obj_set_size(g_scan_btn, LV_PCT(95), 40);
    lv_obj_set_style_bg_color(g_scan_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_scan_btn, 6, 0);
    widget_add_click_handler(g_scan_btn, on_scan_click, NULL);
    
    lv_obj_t *scan_label = lv_label_create(g_scan_btn);
    lv_label_set_text(scan_label, "Scan Networks");
    lv_obj_center(scan_label);
    
    // Список сетей
    g_network_list = lv_obj_create(scroll);
    lv_obj_set_size(g_network_list, LV_PCT(95), 150);
    lv_obj_set_style_bg_color(g_network_list, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_radius(g_network_list, 8, 0);
    lv_obj_set_style_border_width(g_network_list, 1, 0);
    lv_obj_set_style_border_color(g_network_list, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_all(g_network_list, 4, 0);
    lv_obj_set_flex_flow(g_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_network_list, 4, 0);
    
    // Поле пароля
    g_password_textarea = lv_textarea_create(scroll);
    lv_obj_set_size(g_password_textarea, LV_PCT(95), 40);
    lv_textarea_set_placeholder_text(g_password_textarea, "WiFi Password");
    lv_textarea_set_password_mode(g_password_textarea, true);
    lv_textarea_set_one_line(g_password_textarea, true);
    lv_obj_set_style_bg_color(g_password_textarea, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_text_color(g_password_textarea, lv_color_white(), 0);
    lv_obj_add_flag(g_password_textarea, LV_OBJ_FLAG_HIDDEN);
    
    // Кнопка подключения
    g_connect_btn = lv_btn_create(scroll);
    lv_obj_set_size(g_connect_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(g_connect_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(g_connect_btn, 6, 0);
    widget_add_click_handler(g_connect_btn, on_connect_click, NULL);
    
    lv_obj_t *connect_label = lv_label_create(g_connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);
    
    // Кнопка отключения
    g_disconnect_btn = lv_btn_create(scroll);
    lv_obj_set_size(g_disconnect_btn, LV_PCT(45), 40);
    lv_obj_set_style_bg_color(g_disconnect_btn, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_radius(g_disconnect_btn, 6, 0);
    widget_add_click_handler(g_disconnect_btn, on_disconnect_click, NULL);
    lv_obj_add_flag(g_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *disconnect_label = lv_label_create(g_disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_center(disconnect_label);
    
    ESP_LOGI(TAG, "WiFi settings screen created");
    
    return base.screen;
}

esp_err_t wifi_settings_screen_on_show(lv_obj_t *screen_obj, void *params)
{
    ESP_LOGI(TAG, "WiFi settings screen shown");
    
    // Обновляем статус
    update_wifi_status();
    
    // Запускаем таймер обновления статуса
    if (!g_status_timer) {
        g_status_timer = lv_timer_create(status_timer_cb, 2000, NULL);
    }
    
    return ESP_OK;
}

esp_err_t wifi_settings_screen_on_hide(lv_obj_t *screen_obj, void *params)
{
    ESP_LOGI(TAG, "WiFi settings screen hidden");
    
    // Останавливаем таймер
    if (g_status_timer) {
        lv_timer_del(g_status_timer);
        g_status_timer = NULL;
    }
    
    // Освобождаем память от сетей
    if (g_scanned_networks) {
        for (int i = 0; i < g_network_count; i++) {
            if (g_scanned_networks[i]) {
                free(g_scanned_networks[i]);
            }
        }
        free(g_scanned_networks);
        g_scanned_networks = NULL;
    }
    g_network_count = 0;
    g_selected_network_idx = -1;
    
    // Очищаем ссылки
    g_screen = NULL;
    g_status_label = NULL;
    g_ip_label = NULL;
    g_rssi_label = NULL;
    g_network_list = NULL;
    g_scan_btn = NULL;
    g_connect_btn = NULL;
    g_disconnect_btn = NULL;
    g_password_textarea = NULL;
    
    return ESP_OK;
}

