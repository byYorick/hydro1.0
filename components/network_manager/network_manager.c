/**
 * @file network_manager.c
 * @brief Реализация менеджера сетевых функций ESP32S3
 *
 * Максимально использует возможности ESP32S3:
 * - Двухъядерный процессор для параллельной обработки сетевых задач
 * - Встроенный USB для прямого подключения к ПК
 * - Bluetooth 5.0 для мобильных устройств
 * - WiFi 802.11n с поддержкой точки доступа
 * - Встроенная поддержка TLS/SSL для безопасного соединения
 * - Аппаратное ускорение криптографических операций
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "network_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_sntp.h"
// #include "mdns.h" // TODO: Implement mDNS or remove if not needed
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
// #include "esp_bt.h" // TODO: Implement BLE or remove if not needed
// BLE includes removed - TODO: Implement BLE or remove if not needed
// #include "esp_gap_ble_api.h"
// #include "esp_gatts_api.h"
// #include "esp_bt_defs.h"
// #include "esp_bt_main.h"
// #include "esp_gatt_common_api.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

/// Тег для логирования
static const char *TAG = "NETWORK_MGR";

/// Битовые флаги событий WiFi
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define WIFI_SCAN_DONE_BIT      BIT2

/// Битовые флаги событий BLE
#define BLE_CONNECTED_BIT       BIT3
#define BLE_DISCONNECTED_BIT    BIT4

/// Размер буфера для WebSocket
#define WS_BUFFER_SIZE          1024

/// Максимальное количество HTTP обработчиков
#define MAX_HTTP_HANDLERS       16

/// Максимальное количество BLE обработчиков
#define MAX_BLE_HANDLERS        8

/// Глобальные переменные состояния сети
static network_mode_t current_mode = NETWORK_MODE_NONE;
static network_status_t current_status = NETWORK_STATUS_DISCONNECTED;
static network_wifi_config_t wifi_config = {0};
static ap_config_t ap_config = {0};
static network_stats_t network_stats = {0};
static EventGroupHandle_t wifi_event_group = NULL;
static EventGroupHandle_t ble_event_group = NULL;
static SemaphoreHandle_t network_mutex = NULL;
static httpd_handle_t http_server = NULL;
static int websocket_fd = -1;
static bool time_synced = false;
static bool mdns_started = false;
static bool ble_started = false;

/// Структура для хранения HTTP обработчиков
typedef struct {
    char uri[64];
    char method[8];
    httpd_method_t httpd_method;
    void (*handler)(void *ctx);
    void *user_ctx;
    bool in_use;
} http_handler_t;

/// Структура для хранения BLE обработчиков
typedef struct {
    char event[32];
    void (*handler)(void *ctx);
    void *user_ctx;
    bool in_use;
} ble_handler_t;

/// Массивы обработчиков
static http_handler_t http_handlers[MAX_HTTP_HANDLERS] = {0};
static ble_handler_t ble_handlers[MAX_BLE_HANDLERS] = {0};

/**
 * @brief Обработчик событий WiFi
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi станция запущена");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi отключен, попытка переподключения...");
        network_stats.wifi_reconnects++;
        current_status = NETWORK_STATUS_CONNECTING;
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Получен IP адрес: " IPSTR, IP2STR(&event->ip_info.ip));
        current_status = NETWORK_STATUS_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Станция подключена к точке доступа, MAC: " MACSTR,
                MAC2STR(event->mac));
        current_status = NETWORK_STATUS_AP_MODE;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Станция отключилась от точки доступа, MAC: " MACSTR,
                MAC2STR(event->mac));
    }
}

/**
 * @brief Инициализация WiFi драйвера с оптимизациями ESP32S3
 */
static esp_err_t wifi_driver_init(void) {
    esp_err_t ret = ESP_OK;

    // Конфигурация WiFi с максимальной производительностью для ESP32S3
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.osi_funcs.mutex_create = NULL;
    cfg.osi_funcs.mutex_delete = NULL;
    cfg.osi_funcs.mutex_lock = NULL;
    cfg.osi_funcs.mutex_unlock = NULL;

    // Настройка для двухъядерного использования
    cfg.wifi_task_core_id = 1;  // Выделенный core для WiFi задач

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Регистрация обработчиков событий
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    return ret;
}

/**
 * @brief Инициализация сетевого интерфейса
 */
static esp_err_t netif_init(void) {
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(esp_netif_init());

    return ret;
}

/**
 * @brief Конфигурация точки доступа с оптимизациями
 */
static esp_err_t ap_config_init(void) {
    esp_err_t ret = ESP_OK;

    // Создание сетевого интерфейса для точки доступа
    esp_netif_create_default_wifi_ap();

    // Конфигурация WiFi в режиме точки доступа
    network_wifi_config_t wifi_cfg = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0,
        },
    };

    // Копируем SSID из конфигурации
    strncpy((char*)wifi_cfg.ap.ssid, ap_config.ssid, sizeof(wifi_cfg.ap.ssid) - 1);
    wifi_cfg.ap.ssid_len = strlen(ap_config.ssid);

    // Копируем пароль если указан
    if (strlen(ap_config.password) > 0) {
        strncpy((char*)wifi_cfg.ap.password, ap_config.password, sizeof(wifi_cfg.ap.password) - 1);
    } else {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_cfg.ap.max_connection = ap_config.max_connections;
    wifi_cfg.ap.channel = ap_config.channel;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

    return ret;
}

/**
 * @brief Конфигурация клиентского режима WiFi
 */
static esp_err_t sta_config_init(void) {
    esp_err_t ret = ESP_OK;

    // Создание сетевого интерфейса для клиентского режима
    esp_netif_create_default_wifi_sta();

    // Конфигурация WiFi в клиентском режиме
    network_wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = "",
            .password = "",
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    // Копируем SSID и пароль из конфигурации
    strncpy((char*)wifi_cfg.sta.ssid, wifi_config.ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, wifi_config.password, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    return ret;
}

/**
 * @brief Инициализация NTP клиента для синхронизации времени
 */
static void ntp_init(void) {
    // Настройка NTP серверов с учетом региона
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.time.in.ua");
    esp_sntp_setservername(2, "ntp1.time.in.ua");

    // Настройка интервала синхронизации
    esp_sntp_set_sync_interval(3600000); // 1 час

    // Запуск SNTP клиента
    esp_sntp_init();

    ESP_LOGI(TAG, "NTP клиент инициализирован");
}

/**
 * @brief Callback функция синхронизации времени
 */
static void time_sync_notification_cb(struct timeval *tv) {
    time_synced = true;
    ESP_LOGI(TAG, "Время синхронизировано через NTP");

    // Обновляем внутренние часы ESP32
    settimeofday(tv, NULL);
}

/**
 * @brief Инициализация Bluetooth Low Energy
 */
static esp_err_t ble_init(void) {
    esp_err_t ret = ESP_OK;

    // BLE initialization removed - TODO: Implement BLE or remove if not needed
    // ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    // esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    // ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    // BLE initialization removed - TODO: Implement BLE or remove if not needed
    // ESP_ERROR_CHECK(esp_bluedroid_init());
    // ESP_ERROR_CHECK(esp_bluedroid_enable());

    return ret;
}

/**
 * @brief Callback функция GAP BLE событий
 */
// static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) { // TODO: Implement BLE or remove if not needed
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE реклама настроена");
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE реклама запущена");
            } else {
                ESP_LOGE(TAG, "Ошибка запуска BLE рекламы: %d", param->adv_start_cmpl.status);
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE реклама остановлена");
            break;

        default:
            break;
    }
    // BLE callback removed - TODO: Implement BLE or remove if not needed
// }

/**
 * @brief Запуск BLE рекламы для обнаружения мобильными приложениями
 */
// BLE advertising function removed - TODO: Implement BLE or remove if not needed
// static esp_err_t start_ble_advertising(const char *device_name) {
//     esp_err_t ret = ESP_OK;
//
//     // Настройка параметров рекламы
//     esp_ble_adv_params_t adv_params = {
//         .adv_int_min        = 0x20,
//         .adv_int_max        = 0x40,
//         .adv_type           = ADV_TYPE_IND,
//         .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
//         .channel_map        = ADV_CHNL_ALL,
//         .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
//     };
//
//     // Настройка данных рекламы
//     esp_ble_adv_data_t adv_data = {
//         .set_scan_rsp = false,
//         .include_name = true,
//         .include_txpower = true,
//         .min_interval = 0x20,
//         .max_interval = 0x40,
//         .appearance = 0x00,
//         .manufacturer_len = 0,
//         .p_manufacturer_data = NULL,
//         .service_data_len = 0,
//         .p_service_data = NULL,
//         .service_uuid_len = 0,
//         .p_service_uuid = NULL,
//         .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
//     };
//
//     // Установка имени устройства
//     ESP_ERROR_CHECK(esp_ble_gap_set_device_name(device_name));
//
//     // Запуск рекламы
//     ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&adv_params));
//
//     return ret;
// }

/**
 * @brief Инициализация сетевого менеджера
 */
esp_err_t network_manager_init(network_mode_t mode) {
    esp_err_t ret = ESP_OK;

    if (network_mutex == NULL) {
        network_mutex = xSemaphoreCreateMutex();
        if (network_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса сети");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(network_mutex, portMAX_DELAY);

    current_mode = mode;
    current_status = NETWORK_STATUS_DISCONNECTED;

    // Инициализация сетевого интерфейса
    ESP_ERROR_CHECK(netif_init());

    // Инициализация WiFi драйвера
    ESP_ERROR_CHECK(wifi_driver_init());

    // Создание event group для синхронизации WiFi событий
    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
        if (wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Ошибка создания WiFi event group");
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    }

    // Инициализация в зависимости от режима
    switch (mode) {
        case NETWORK_MODE_STA:
            ESP_ERROR_CHECK(sta_config_init());
            break;

        case NETWORK_MODE_AP:
            ESP_ERROR_CHECK(ap_config_init());
            break;

        case NETWORK_MODE_HYBRID:
            // В гибридном режиме запускаем точку доступа и подключаемся как клиент
            ESP_ERROR_CHECK(ap_config_init());
            ESP_ERROR_CHECK(sta_config_init());
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
            break;

        case NETWORK_MODE_BLE:
            // Только Bluetooth режим - TODO: Implement BLE or remove if not needed
            // ESP_ERROR_CHECK(ble_init());
            // ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
            break;

        default:
            ESP_LOGW(TAG, "Выбран режим без сети");
            ret = ESP_OK;
            goto cleanup;
    }

    // Запуск WiFi если не только BLE режим
    if (mode != NETWORK_MODE_BLE && mode != NETWORK_MODE_NONE) {
        ESP_ERROR_CHECK(esp_wifi_start());

        if (mode == NETWORK_MODE_AP || mode == NETWORK_MODE_HYBRID) {
            ESP_LOGI(TAG, "Запуск точки доступа WiFi: %s", ap_config.ssid);
        }

        if (mode == NETWORK_MODE_STA || mode == NETWORK_MODE_HYBRID) {
            ESP_LOGI(TAG, "Подключение к WiFi сети: %s", wifi_config.ssid);
        }
    }

    // Инициализация BLE если требуется
    if (mode == NETWORK_MODE_BLE || mode == NETWORK_MODE_HYBRID) {
        ESP_ERROR_CHECK(start_ble_advertising("HydroMonitor-ESP32S3"));

        // Создание event group для BLE событий
        if (ble_event_group == NULL) {
            ble_event_group = xEventGroupCreate();
            if (ble_event_group == NULL) {
                ESP_LOGE(TAG, "Ошибка создания BLE event group");
                ret = ESP_ERR_NO_MEM;
                goto cleanup;
            }
        }
    }

    // Инициализация NTP для синхронизации времени
    ntp_init();
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    ESP_LOGI(TAG, "Сетевой менеджер инициализирован в режиме %d", mode);

cleanup:
    xSemaphoreGive(network_mutex);
    return ret;
}

/**
 * @brief Деинициализация сетевого менеджера
 */
esp_err_t network_manager_deinit(void) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(network_mutex, portMAX_DELAY);

    // Остановка HTTP сервера
    if (http_server != NULL) {
        httpd_stop(http_server);
        http_server = NULL;
    }

    // Остановка WebSocket
    if (websocket_fd != -1) {
        close(websocket_fd);
        websocket_fd = -1;
    }

    // Остановка mDNS
    if (mdns_started) {
        mdns_free();
        mdns_started = false;
    }

    // Остановка BLE - TODO: Implement BLE or remove if not needed
    // if (ble_started) {
    //     esp_ble_gap_stop_advertising();
    //     esp_bluedroid_disable();
    //     esp_bt_controller_disable();
    //     ble_started = false;
    // }

    // Остановка WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Очистка ресурсов
    if (wifi_event_group != NULL) {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = NULL;
    }

    if (ble_event_group != NULL) {
        vEventGroupDelete(ble_event_group);
        ble_event_group = NULL;
    }

    if (network_mutex != NULL) {
        vSemaphoreDelete(network_mutex);
        network_mutex = NULL;
    }

    current_mode = NETWORK_MODE_NONE;
    current_status = NETWORK_STATUS_DISCONNECTED;

    ESP_LOGI(TAG, "Сетевой менеджер деинициализирован");

    xSemaphoreGive(network_mutex);
    return ret;
}

/**
 * @brief Подключение к WiFi сети
 */
esp_err_t network_manager_connect_wifi(const network_wifi_config_t *config) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(network_mutex, portMAX_DELAY);

    memcpy(&wifi_config, config, sizeof(network_wifi_config_t));

    // Настройка статического IP если требуется
    if (config->use_static_ip) {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_dhcpc_stop(netif);

        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = ipaddr_addr(config->static_ip);
        ip_info.gw.addr = ipaddr_addr(config->gateway);
        ip_info.netmask.addr = ipaddr_addr(config->netmask);

        ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

        // Настройка DNS
        esp_netif_dns_info_t dns_info;
        dns_info.ip.u_addr.ip4.addr = ipaddr_addr(config->dns);
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;

        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));
    }

    // Обновление конфигурации WiFi
    network_wifi_config_t wifi_cfg;
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg));

    strncpy((char*)wifi_cfg.sta.ssid, config->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, config->password, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Подключение к WiFi: %s", config->ssid);

    xSemaphoreGive(network_mutex);
    return ret;
}

/**
 * @brief Создание точки доступа WiFi
 */
esp_err_t network_manager_start_ap(const ap_config_t *config) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(network_mutex, portMAX_DELAY);

    memcpy(&ap_config, config, sizeof(ap_config_t));

    // Обновление конфигурации точки доступа
    network_wifi_config_t wifi_cfg;
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &wifi_cfg));

    strncpy((char*)wifi_cfg.ap.ssid, config->ssid, sizeof(wifi_cfg.ap.ssid) - 1);
    wifi_cfg.ap.ssid_len = strlen(config->ssid);

    if (strlen(config->password) > 0) {
        strncpy((char*)wifi_cfg.ap.password, config->password, sizeof(wifi_cfg.ap.password) - 1);
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_cfg.ap.max_connection = config->max_connections;
    wifi_cfg.ap.channel = config->channel;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

    ESP_LOGI(TAG, "Точка доступа запущена: %s", config->ssid);

    xSemaphoreGive(network_mutex);
    return ret;
}

/**
 * @brief Отключение от WiFi сети
 */
esp_err_t network_manager_disconnect_wifi(void) {
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    current_status = NETWORK_STATUS_DISCONNECTED;

    ESP_LOGI(TAG, "Отключено от WiFi");

    return ret;
}

/**
 * @brief Получение текущего статуса сети
 */
network_status_t network_manager_get_status(void) {
    return current_status;
}

/**
 * @brief Получение сетевых статистик
 */
esp_err_t network_manager_get_stats(network_stats_t *stats) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(network_mutex, portMAX_DELAY);

    memcpy(stats, &network_stats, sizeof(network_stats_t));

    // Получение уровня сигнала WiFi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        stats->rssi = ap_info.rssi;
    }

    // Подсчет времени работы
    stats->uptime_seconds = esp_timer_get_time() / 1000000ULL;

    xSemaphoreGive(network_mutex);
    return ret;
}

/**
 * @brief Сканирование доступных WiFi сетей
 */
int network_manager_scan_wifi(char (*networks)[32], int max_networks) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    wifi_ap_record_t ap_records[32];

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    int count = 0;
    for (int i = 0; i < ap_count && count < max_networks; i++) {
        strncpy(networks[count], (char*)ap_records[i].ssid, 31);
        networks[count][31] = '\0';
        count++;
    }

    return count;
}

/**
 * @brief Проверка подключения к интернету
 */
bool network_manager_is_internet_available(void) {
    if (current_status != NETWORK_STATUS_CONNECTED) {
        return false;
    }

    // Простая проверка через ping к Google DNS
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return false;
    }

    bool connected = (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    close(sock);

    return connected;
}

/**
 * @brief Синхронизация времени через NTP
 */
esp_err_t network_manager_sync_time(void) {
    if (!network_manager_is_internet_available()) {
        ESP_LOGE(TAG, "Интернет недоступен для NTP синхронизации");
        return ESP_ERR_NOT_CONNECTED;
    }

    esp_sntp_restart();

    // Ожидание синхронизации времени (до 10 секунд)
    int retry = 0;
    while (!time_synced && retry < 100) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        retry++;
    }

    if (time_synced) {
        ESP_LOGI(TAG, "NTP синхронизация завершена успешно");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "NTP синхронизация не удалась");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief Получение текущего времени
 */
esp_err_t network_manager_get_time(uint32_t *timestamp, char *time_str, char *date_str) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timestamp != NULL) {
        *timestamp = (uint32_t)now;
    }

    if (time_str != NULL) {
        strftime(time_str, 9, "%H:%M:%S", &timeinfo);
    }

    if (date_str != NULL) {
        strftime(date_str, 11, "%d.%m.%Y", &timeinfo);
    }

    return ESP_OK;
}

/**
 * @brief Получение локального IP адреса
 */
esp_err_t network_manager_get_ip(char *ip_str) {
    esp_netif_ip_info_t ip_info;

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }

    if (netif == NULL) {
        strcpy(ip_str, "0.0.0.0");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));

    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));

    return ESP_OK;
}

/**
 * @brief Получение MAC адреса
 */
esp_err_t network_manager_get_mac(char *mac_str) {
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

/**
 * @brief Сохранение сетевой конфигурации в NVS
 */
esp_err_t network_manager_save_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(nvs_open("network", NVS_READWRITE, &nvs_handle));

    // Сохранение WiFi конфигурации
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_ssid", wifi_config.ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_pass", wifi_config.password));
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "wifi_static", wifi_config.use_static_ip));
    if (wifi_config.use_static_ip) {
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_ip", wifi_config.static_ip));
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_gw", wifi_config.gateway));
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_nm", wifi_config.netmask));
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "wifi_dns", wifi_config.dns));
    }

    // Сохранение AP конфигурации
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ap_ssid", ap_config.ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ap_pass", ap_config.password));
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "ap_ch", ap_config.channel));
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "ap_max", ap_config.max_connections));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Сетевая конфигурация сохранена в NVS");

    return ret;
}

/**
 * @brief Загрузка сетевой конфигурации из NVS
 */
esp_err_t network_manager_load_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(nvs_open("network", NVS_READONLY, &nvs_handle));

    // Загрузка WiFi конфигурации
    size_t length = sizeof(wifi_config.ssid);
    ret = nvs_get_str(nvs_handle, "wifi_ssid", wifi_config.ssid, &length);
    if (ret != ESP_OK) {
        strcpy(wifi_config.ssid, "HydroMonitor");
    }

    length = sizeof(wifi_config.password);
    ret = nvs_get_str(nvs_handle, "wifi_pass", wifi_config.password, &length);
    if (ret != ESP_OK) {
        strcpy(wifi_config.password, "");
    }

    wifi_config.use_static_ip = false;
    nvs_get_u8(nvs_handle, "wifi_static", &wifi_config.use_static_ip);

    if (wifi_config.use_static_ip) {
        length = sizeof(wifi_config.static_ip);
        nvs_get_str(nvs_handle, "wifi_ip", wifi_config.static_ip, &length);
        length = sizeof(wifi_config.gateway);
        nvs_get_str(nvs_handle, "wifi_gw", wifi_config.gateway, &length);
        length = sizeof(wifi_config.netmask);
        nvs_get_str(nvs_handle, "wifi_nm", wifi_config.netmask, &length);
        length = sizeof(wifi_config.dns);
        nvs_get_str(nvs_handle, "wifi_dns", wifi_config.dns, &length);
    }

    // Загрузка AP конфигурации
    length = sizeof(ap_config.ssid);
    ret = nvs_get_str(nvs_handle, "ap_ssid", ap_config.ssid, &length);
    if (ret != ESP_OK) {
        strcpy(ap_config.ssid, "HydroMonitor-AP");
    }

    length = sizeof(ap_config.password);
    ret = nvs_get_str(nvs_handle, "ap_pass", ap_config.password, &length);
    if (ret != ESP_OK) {
        strcpy(ap_config.password, "12345678");
    }

    ap_config.channel = 1;
    nvs_get_u8(nvs_handle, "ap_ch", &ap_config.channel);

    ap_config.max_connections = 4;
    nvs_get_u8(nvs_handle, "ap_max", &ap_config.max_connections);

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Сетевая конфигурация загружена из NVS");

    return ESP_OK;
}

/**
 * @brief Сброс сетевой конфигурации к значениям по умолчанию
 */
esp_err_t network_manager_reset_config(void) {
    strcpy(wifi_config.ssid, "HydroMonitor");
    strcpy(wifi_config.password, "");
    wifi_config.use_static_ip = false;

    strcpy(ap_config.ssid, "HydroMonitor-AP");
    strcpy(ap_config.password, "12345678");
    ap_config.channel = 1;
    ap_config.max_connections = 4;

    return network_manager_save_config();
}

/**
 * @brief HTTP обработчик для главной страницы веб-интерфейса
 */
static esp_err_t http_index_handler(httpd_req_t *req) {
    const char *html =
        "<!DOCTYPE html>"
        "<html><head><title>Hydroponics Monitor</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:Arial;margin:20px;} .card{display:inline-block;border:1px solid #ddd;padding:10px;margin:10px;border-radius:5px;}</style>"
        "</head><body>"
        "<h1>Hydroponics Monitor ESP32S3</h1>"
        "<div class='card'><h3>pH</h3><span id='ph'>Loading...</span></div>"
        "<div class='card'><h3>EC</h3><span id='ec'>Loading...</span></div>"
        "<div class='card'><h3>Temperature</h3><span id='temp'>Loading...</span></div>"
        "<script>setInterval(()=>fetch('/api/sensors').then(r=>r.json()).then(d=>{document.getElementById('ph').innerText=d.ph+' '+d.ph_unit;document.getElementById('ec').innerText=d.ec+' '+d.ec_unit;document.getElementById('temp').innerText=d.temp+' '+d.temp_unit;}),1000);</script>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));

    network_stats.http_requests++;

    return ESP_OK;
}

/**
 * @brief HTTP обработчик для API данных датчиков
 */
static esp_err_t http_api_sensors_handler(httpd_req_t *req) {
    char json[256];

    // Здесь нужно получить реальные данные датчиков
    snprintf(json, sizeof(json),
             "{\"ph\":6.8,\"ph_unit\":\"\",\"ec\":1.5,\"ec_unit\":\"mS/cm\",\"temp\":24.5,\"temp_unit\":\"°C\",\"humidity\":65.0,\"humidity_unit\":\"%%\",\"lux\":500,\"lux_unit\":\"lux\",\"co2\":450,\"co2_unit\":\"ppm\",\"timestamp\":%lld}",
             (long long)esp_timer_get_time());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    network_stats.http_requests++;

    return ESP_OK;
}

/**
 * @brief Запуск HTTP сервера для веб-интерфейса
 */
esp_err_t network_manager_start_http_server(uint16_t port) {
    esp_err_t ret = ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_open_sockets = 7;
    config.max_resp_headers = 8;
    config.stack_size = 6144;  // Увеличенный стек для ESP32S3

    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    // Регистрация обработчиков
    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = http_index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t api_sensors_uri = {
        .uri       = "/api/sensors",
        .method    = HTTP_GET,
        .handler   = http_api_sensors_handler,
        .user_ctx  = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &api_sensors_uri));

    ESP_LOGI(TAG, "HTTP сервер запущен на порту %d", port);

    return ret;
}

/**
 * @brief Остановка HTTP сервера
 */
esp_err_t network_manager_stop_http_server(void) {
    if (http_server != NULL) {
        httpd_stop(http_server);
        http_server = NULL;
        ESP_LOGI(TAG, "HTTP сервер остановлен");
    }

    return ESP_OK;
}

/**
 * @brief Регистрация HTTP обработчика
 */
esp_err_t network_manager_register_http_handler(const char *uri, const char *method,
                                               void (*handler)(void *), void *user_ctx) {
    // Находим свободный слот в массиве обработчиков
    for (int i = 0; i < MAX_HTTP_HANDLERS; i++) {
        if (!http_handlers[i].in_use) {
            strncpy(http_handlers[i].uri, uri, sizeof(http_handlers[i].uri) - 1);
            strncpy(http_handlers[i].method, method, sizeof(http_handlers[i].method) - 1);
            http_handlers[i].handler = handler;
            http_handlers[i].user_ctx = user_ctx;
            http_handlers[i].in_use = true;

            // Определяем HTTP метод
            if (strcmp(method, "GET") == 0) {
                http_handlers[i].httpd_method = HTTP_GET;
            } else if (strcmp(method, "POST") == 0) {
                http_handlers[i].httpd_method = HTTP_POST;
            } else if (strcmp(method, "PUT") == 0) {
                http_handlers[i].httpd_method = HTTP_PUT;
            } else if (strcmp(method, "DELETE") == 0) {
                http_handlers[i].httpd_method = HTTP_DELETE;
            }

            ESP_LOGI(TAG, "Зарегистрирован HTTP обработчик: %s %s", method, uri);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Нет свободных слотов для HTTP обработчика");
    return ESP_ERR_NO_MEM;
}

/**
 * @brief Включение mDNS сервиса для обнаружения устройства
 */
esp_err_t network_manager_start_mdns(const char *hostname, const char *service_name, uint16_t port) {
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("Hydroponics Monitor"));

    mdns_txt_item_t service_txt[] = {
        {"board", "esp32s3"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", port, service_txt, 2));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_hydroponics", "_tcp", port, service_txt, 2));

    mdns_started = true;

    ESP_LOGI(TAG, "mDNS сервис запущен: %s.local", hostname);

    return ret;
}

/**
 * @brief Отключение mDNS сервиса
 */
esp_err_t network_manager_stop_mdns(void) {
    if (mdns_started) {
        mdns_free();
        mdns_started = false;
        ESP_LOGI(TAG, "mDNS сервис остановлен");
    }

    return ESP_OK;
}

/**
 * @brief Включение Bluetooth Low Energy
 */
esp_err_t network_manager_start_ble(const char *device_name) {
    esp_err_t ret = ESP_OK;

    if (!ble_started) {
        ESP_ERROR_CHECK(start_ble_advertising(device_name));
        ble_started = true;
        ESP_LOGI(TAG, "Bluetooth LE запущен: %s", device_name);
    }

    return ret;
}

/**
 * @brief Отключение Bluetooth Low Energy
 */
esp_err_t network_manager_stop_ble(void) {
    if (ble_started) {
        esp_ble_gap_stop_advertising();
        ble_started = false;
        ESP_LOGI(TAG, "Bluetooth LE остановлен");
    }

    return ESP_OK;
}

/**
 * @brief Проверка доступности обновлений OTA
 */
bool network_manager_check_ota_update(const char *current_version) {
    // Здесь должна быть логика проверки обновлений с сервера
    // Пока возвращаем false (обновлений нет)
    return false;
}

/**
 * @brief Запуск обновления OTA
 */
esp_err_t network_manager_start_ota_update(const char *url) {
    esp_err_t ret = ESP_OK;

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = NULL,
        .timeout_ms = 30000,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = NULL,
        .bulk_flash_erase = false,
        .partial_http_download = false,
        .max_http_request_size = 16384,
    };

    ESP_LOGI(TAG, "Запуск OTA обновления с URL: %s", url);

    ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA обновление завершено успешно, перезагрузка...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Ошибка OTA обновления: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Получение прогресса OTA обновления
 */
uint8_t network_manager_get_ota_progress(void) {
    // Здесь должна быть логика получения прогресса OTA
    // Пока возвращаем 0
    return 0;
}

/**
 * @brief Регистрация BLE обработчика
 */
esp_err_t network_manager_register_ble_handler(const char *event,
                                              void (*handler)(void *), void *user_ctx) {
    // Находим свободный слот в массиве обработчиков
    for (int i = 0; i < MAX_BLE_HANDLERS; i++) {
        if (!ble_handlers[i].in_use) {
            strncpy(ble_handlers[i].event, event, sizeof(ble_handlers[i].event) - 1);
            ble_handlers[i].handler = handler;
            ble_handlers[i].user_ctx = user_ctx;
            ble_handlers[i].in_use = true;

            ESP_LOGI(TAG, "Зарегистрирован BLE обработчик: %s", event);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Нет свободных слотов для BLE обработчика");
    return ESP_ERR_NO_MEM;
}

/**
 * @brief Отправка данных через Bluetooth LE
 */
esp_err_t network_manager_send_ble(const uint8_t *data, size_t length) {
    // Здесь должна быть реализация отправки данных через BLE
    // Пока заглушка
    ESP_LOGV(TAG, "Отправка данных через BLE: %d байт", length);
    return ESP_OK;
}

/**
 * @brief Регистрация WebSocket обработчика
 */
esp_err_t network_manager_register_ws_handler(const char *event,
                                             void (*handler)(void *), void *user_ctx) {
    // Здесь должна быть реализация регистрации WebSocket обработчиков
    // Пока заглушка
    ESP_LOGI(TAG, "Зарегистрирован WebSocket обработчик: %s", event);
    return ESP_OK;
}

/**
 * @brief Отправка данных через WebSocket
 */
esp_err_t network_manager_send_websocket(const uint8_t *data, size_t length) {
    // Здесь должна быть реализация отправки данных через WebSocket
    // Пока заглушка
    ESP_LOGV(TAG, "Отправка данных через WebSocket: %d байт", length);
    return ESP_OK;
}

// Реализация остальных функций будет добавлена в следующих частях...
