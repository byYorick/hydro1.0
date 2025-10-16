/**
 * @file network_manager.c
 * @brief Реализация Network Manager - управление WiFi
 */

#include "network_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "NET_MGR";

// Event bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1

// Глобальное состояние
static bool g_initialized = false;
static bool g_connected = false;
static EventGroupHandle_t g_wifi_event_group = NULL;
static esp_netif_t *g_sta_netif = NULL;
static wifi_config_t g_current_config = {0};
static uint32_t g_reconnect_count = 0;
static int8_t g_last_rssi = 0;

/* =============================
 *  EVENT HANDLERS
 * ============================= */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected");
        g_connected = false;
        g_reconnect_count++;
        
        // Автопереподключение
        esp_wifi_connect();
        xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_connected = true;
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================= */

esp_err_t network_manager_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing network manager...");
    
    // Создаем event group
    g_wifi_event_group = xEventGroupCreate();
    if (!g_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Инициализация netif
    ESP_ERROR_CHECK(esp_netif_init());

    // Создаем event loop если еще не создан
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
    return ret;
}

    // Создаем default WiFi STA
    g_sta_netif = esp_netif_create_default_wifi_sta();
    
    // Инициализация WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Регистрируем обработчики событий
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Устанавливаем режим STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Запускаем WiFi
        ESP_ERROR_CHECK(esp_wifi_start());

    g_initialized = true;
    ESP_LOGI(TAG, "Network manager initialized successfully");
    
    return ESP_OK;
}

esp_err_t network_manager_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing network manager...");
    
    // Отключаемся
    network_manager_disconnect();
    
    // Останавливаем WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Удаляем event group
    if (g_wifi_event_group) {
        vEventGroupDelete(g_wifi_event_group);
        g_wifi_event_group = NULL;
    }
    
    g_initialized = false;
    ESP_LOGI(TAG, "Network manager deinitialized");
    
    return ESP_OK;
}

esp_err_t network_manager_connect(const char *ssid, const char *password)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ssid) {
        ESP_LOGE(TAG, "SSID is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    // Заполняем конфигурацию
    memset(&g_current_config, 0, sizeof(wifi_config_t));
    strncpy((char *)g_current_config.sta.ssid, ssid, sizeof(g_current_config.sta.ssid) - 1);
    
    if (password && strlen(password) > 0) {
        strncpy((char *)g_current_config.sta.password, password, sizeof(g_current_config.sta.password) - 1);
        g_current_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        g_current_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    
    g_current_config.sta.pmf_cfg.capable = true;
    g_current_config.sta.pmf_cfg.required = false;
    
    // Устанавливаем конфигурацию
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &g_current_config));
    
    // Подключаемся
    esp_err_t ret = esp_wifi_connect();
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Connect failed: %s", esp_err_to_name(ret));
    return ret;
}

    ESP_LOGI(TAG, "Connection initiated");
    return ESP_OK;
}

esp_err_t network_manager_disconnect(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting WiFi...");
    
    esp_err_t ret = esp_wifi_disconnect();
    g_connected = false;

    return ret;
}

esp_err_t network_manager_scan(wifi_scan_result_t *results, uint16_t max_results, uint16_t *actual_count)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!results || !actual_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    // Конфигурация сканирования
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };
    
    // Запускаем блокирующее сканирование
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(ret));
        *actual_count = 0;
        return ret;
    }
    
    // Получаем количество найденных сетей
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    ESP_LOGI(TAG, "Found %d networks", ap_count);
    
    if (ap_count == 0) {
        *actual_count = 0;
    return ESP_OK;
}

    // Ограничиваем количество результатов
    if (ap_count > max_results) {
        ap_count = max_results;
    }
    
    // Получаем записи
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        *actual_count = 0;
        return ESP_ERR_NO_MEM;
    }
    
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    if (ret == ESP_OK) {
        // Копируем результаты
        for (int i = 0; i < ap_count; i++) {
            strncpy(results[i].ssid, (char *)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
            results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
            results[i].rssi = ap_records[i].rssi;
            results[i].authmode = ap_records[i].authmode;
            results[i].channel = ap_records[i].primary;
        }
        *actual_count = ap_count;
    }
    
    free(ap_records);

    return ret;
}

esp_err_t network_manager_get_info(wifi_info_t *info)
{
    if (!g_initialized || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(info, 0, sizeof(wifi_info_t));
    
    // Проверяем подключение
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        // Подключены
        info->status = WIFI_STATUS_CONNECTED;
        info->is_connected = true;
        info->rssi = ap_info.rssi;
        strncpy(info->ssid, (char *)ap_info.ssid, sizeof(info->ssid) - 1);
        g_last_rssi = ap_info.rssi;
        
        // Получаем IP
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(g_sta_netif, &ip_info) == ESP_OK) {
            snprintf(info->ip, sizeof(info->ip), IPSTR, IP2STR(&ip_info.ip));
            snprintf(info->gateway, sizeof(info->gateway), IPSTR, IP2STR(&ip_info.gw));
            snprintf(info->netmask, sizeof(info->netmask), IPSTR, IP2STR(&ip_info.netmask));
        }
    } else {
        // Не подключены
        info->status = WIFI_STATUS_DISCONNECTED;
        info->is_connected = false;
        info->rssi = 0;
        strcpy(info->ssid, "N/A");
        strcpy(info->ip, "0.0.0.0");
    }
    
    info->reconnect_count = g_reconnect_count;

    return ESP_OK;
}

bool network_manager_is_connected(void)
{
    return g_connected;
}

esp_err_t network_manager_save_credentials(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Saving WiFi credentials to NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("network", NVS_READWRITE, &nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Сохраняем SSID и пароль
    nvs_set_str(nvs_handle, "wifi_ssid", (char *)g_current_config.sta.ssid);
    nvs_set_str(nvs_handle, "wifi_pass", (char *)g_current_config.sta.password);
    
    // Commit
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Credentials saved");
    }

    return ret;
}

esp_err_t network_manager_load_and_connect(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Loading WiFi credentials from NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("network", NVS_READONLY, &nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved credentials");
    return ret;
}

    char ssid[MAX_WIFI_SSID_LEN] = {0};
    char password[MAX_WIFI_PASSWORD_LEN] = {0};
    size_t len;
    
    // Читаем SSID
    len = sizeof(ssid);
    ret = nvs_get_str(nvs_handle, "wifi_ssid", ssid, &len);
    
    if (ret == ESP_OK) {
        // Читаем пароль
        len = sizeof(password);
        nvs_get_str(nvs_handle, "wifi_pass", password, &len);
        
        nvs_close(nvs_handle);
        
        // Подключаемся
        ESP_LOGI(TAG, "Auto-connecting to saved network: %s", ssid);
        return network_manager_connect(ssid, password);
    }
    
    nvs_close(nvs_handle);
    ESP_LOGW(TAG, "No SSID found in NVS");
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t network_manager_get_mac(char *mac_str, size_t len)
{
    if (!mac_str || len < 18) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);

    if (ret == ESP_OK) {
        snprintf(mac_str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return ret;
}
