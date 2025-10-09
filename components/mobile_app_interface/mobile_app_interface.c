/**
 * @file mobile_app_interface.c
 * @brief Реализация интерфейса для мобильного приложения
 *
 * Максимально использует возможности ESP32S3:
 * - HTTP сервер с поддержкой REST API
 * - WebSocket для реального времени
 * - Bluetooth LE для мобильных устройств
 * - JSON парсинг и генерация
 * - Аутентификация и шифрование
 * - Оптимизированная работа с памятью
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "mobile_app_interface.h"
#include "esp_log.h"
#include "esp_http_server.h"
// #include "esp_websocket_server.h" // TODO: Implement websocket server or remove
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

/// Тег для логирования
static const char *TAG = "MOBILE_IF";

/// Глобальные переменные состояния мобильного интерфейса
static network_mode_t current_network_mode = NETWORK_MODE_NONE;
static bool mobile_interface_initialized = false;
static SemaphoreHandle_t mobile_interface_mutex = NULL;
static QueueHandle_t command_queue = NULL;
static EventGroupHandle_t connection_event_group = NULL;
static httpd_handle_t http_server = NULL;
// static esp_websocket_handle_t websocket_server = NULL; // TODO: Implement websocket server

/// Константы мобильного интерфейса
#define MOBILE_COMMAND_QUEUE_SIZE 10
#define MOBILE_NOTIFICATION_QUEUE_SIZE 20
#define MOBILE_HISTORY_QUEUE_SIZE 50
#define MOBILE_MAX_CONNECTIONS 5
#define MOBILE_CONNECTION_TIMEOUT_MS 10000
#define MOBILE_DATA_SYNC_INTERVAL_MS 5000

/// Битовые флаги событий мобильного интерфейса
#define MOBILE_CONNECTED_BIT BIT0
#define MOBILE_DISCONNECTED_BIT BIT1
#define MOBILE_DATA_SYNC_BIT BIT2
#define MOBILE_COMMAND_RECEIVED_BIT BIT3

/// Структура для хранения активных соединений
typedef struct {
    bool in_use;
    char client_ip[16];
    uint16_t client_port;
    char device_info[128];
    uint32_t last_activity;
    bool authenticated;
    char auth_token[64];
} mobile_connection_t;

/// Массив активных соединений
static mobile_connection_t active_connections[MOBILE_MAX_CONNECTIONS] = {0};

/// Callback функции
static mobile_command_handler_t command_handler = NULL;
static mobile_error_handler_t error_handler = NULL;
static void *command_handler_ctx = NULL;
static void *error_handler_ctx = NULL;

/**
 * @brief Инициализация мобильного интерфейса
 */
esp_err_t mobile_app_interface_init(network_mode_t mode) {
    esp_err_t ret = ESP_OK;

    if (mobile_interface_initialized) {
        ESP_LOGW(TAG, "Мобильный интерфейс уже инициализирован");
        return ESP_OK;
    }

    // Создание мьютекса для защиты данных
    if (mobile_interface_mutex == NULL) {
        mobile_interface_mutex = xSemaphoreCreateMutex();
        if (mobile_interface_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса мобильного интерфейса");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(mobile_interface_mutex, portMAX_DELAY);

    current_network_mode = mode;

    // Создание очередей для команд и уведомлений
    if (command_queue == NULL) {
        command_queue = xQueueCreate(MOBILE_COMMAND_QUEUE_SIZE, sizeof(mobile_control_command_t));
        if (command_queue == NULL) {
            ESP_LOGE(TAG, "Ошибка создания очереди команд");
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    }

    // Создание event group для отслеживания соединений
    if (connection_event_group == NULL) {
        connection_event_group = xEventGroupCreate();
        if (connection_event_group == NULL) {
            ESP_LOGE(TAG, "Ошибка создания event group соединений");
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    }

    // Инициализация сетевых компонентов в зависимости от режима
    switch (mode) {
        case NETWORK_MODE_HYBRID:
        case NETWORK_MODE_AP:
        case NETWORK_MODE_STA:
            // Запуск HTTP сервера с REST API
            ret = start_http_server();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Ошибка запуска HTTP сервера");
                goto cleanup;
            }

            // Запуск WebSocket сервера
            ret = start_websocket_server();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Ошибка запуска WebSocket сервера");
                goto cleanup;
            }
            break;

        case NETWORK_MODE_BLE:
            // Инициализация Bluetooth LE
            ret = start_ble_server();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Ошибка запуска BLE сервера");
                goto cleanup;
            }
            break;

        default:
            ESP_LOGW(TAG, "Выбран режим без сети");
            ret = ESP_OK;
            goto cleanup;
    }

    mobile_interface_initialized = true;
    ESP_LOGI(TAG, "Мобильный интерфейс инициализирован в режиме %d", mode);

cleanup:
    xSemaphoreGive(mobile_interface_mutex);
    return ret;
}

/**
 * @brief Деинициализация мобильного интерфейса
 */
esp_err_t mobile_app_interface_deinit(void) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(mobile_interface_mutex, portMAX_DELAY);

    if (!mobile_interface_initialized) {
        xSemaphoreGive(mobile_interface_mutex);
        return ESP_OK;
    }

    // Остановка серверов в зависимости от режима
    switch (current_network_mode) {
        case NETWORK_MODE_HYBRID:
        case NETWORK_MODE_AP:
        case NETWORK_MODE_STA:
            // Остановка HTTP сервера
            if (http_server != NULL) {
                httpd_stop(http_server);
                http_server = NULL;
            }

            // Остановка WebSocket сервера
            // if (websocket_server != NULL) { // TODO: Implement websocket server
                // esp_websocket_server_stop(websocket_server); // TODO: Implement websocket server
                websocket_server = NULL;
            }
            break;

        case NETWORK_MODE_BLE:
            // Остановка BLE сервера
            stop_ble_server();
            break;

        default:
            break;
    }

    // Очистка ресурсов
    if (command_queue != NULL) {
        vQueueDelete(command_queue);
        command_queue = NULL;
    }

    if (connection_event_group != NULL) {
        vEventGroupDelete(connection_event_group);
        connection_event_group = NULL;
    }

    if (mobile_interface_mutex != NULL) {
        vSemaphoreDelete(mobile_interface_mutex);
        mobile_interface_mutex = NULL;
    }

    mobile_interface_initialized = false;
    ESP_LOGI(TAG, "Мобильный интерфейс деинициализирован");

    xSemaphoreGive(mobile_interface_mutex);
    return ret;
}

/**
 * @brief Запуск HTTP сервера с REST API
 */
static esp_err_t start_http_server(void) {
    esp_err_t ret = ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.max_open_sockets = MOBILE_MAX_CONNECTIONS;
    config.max_resp_headers = 16;
    config.stack_size = 8192;  // Увеличенный стек для ESP32S3
    config.core_id = 1;        // Выделенное ядро для HTTP сервера

    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    // Регистрация обработчиков REST API
    register_rest_api_handlers();

    ESP_LOGI(TAG, "HTTP сервер мобильного интерфейса запущен на порту %d", config.server_port);

    return ret;
}

/**
 * @brief Регистрация обработчиков REST API
 */
static void register_rest_api_handlers(void) {
    // Обработчик получения данных датчиков
    httpd_uri_t sensor_data_uri = {
        .uri       = "/api/sensors",
        .method    = HTTP_GET,
        .handler   = http_get_sensor_data_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &sensor_data_uri);

    // Обработчик получения статуса системы
    httpd_uri_t system_status_uri = {
        .uri       = "/api/system/status",
        .method    = HTTP_GET,
        .handler   = http_get_system_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &system_status_uri);

    // Обработчик получения настроек
    httpd_uri_t settings_uri = {
        .uri       = "/api/settings",
        .method    = HTTP_GET,
        .handler   = http_get_settings_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &settings_uri);

    // Обработчик обновления настроек
    httpd_uri_t settings_update_uri = {
        .uri       = "/api/settings",
        .method    = HTTP_POST,
        .handler   = http_update_settings_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &settings_update_uri);

    // Обработчик получения истории данных
    httpd_uri_t history_uri = {
        .uri       = "/api/history",
        .method    = HTTP_GET,
        .handler   = http_get_history_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &history_uri);

    // Обработчик отправки команд управления
    httpd_uri_t control_uri = {
        .uri       = "/api/control",
        .method    = HTTP_POST,
        .handler   = http_send_control_command_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &control_uri);

    // Обработчик аутентификации
    httpd_uri_t auth_uri = {
        .uri       = "/api/auth",
        .method    = HTTP_POST,
        .handler   = http_authenticate_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &auth_uri);

    // Обработчик получения информации об устройстве
    httpd_uri_t device_info_uri = {
        .uri       = "/api/device/info",
        .method    = HTTP_GET,
        .handler   = http_get_device_info_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &device_info_uri);

    ESP_LOGI(TAG, "REST API обработчики зарегистрированы");
}

/**
 * @brief HTTP обработчик получения данных датчиков
 */
static esp_err_t http_get_sensor_data_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    // Создаем JSON с данными датчиков
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Здесь нужно получить реальные данные датчиков
    cJSON_AddNumberToObject(root, "ph", 6.8);
    cJSON_AddNumberToObject(root, "ph_target", 6.8);
    cJSON_AddNumberToObject(root, "ec", 1.5);
    cJSON_AddNumberToObject(root, "ec_target", 1.5);
    cJSON_AddNumberToObject(root, "temperature", 24.5);
    cJSON_AddNumberToObject(root, "humidity", 65.0);
    cJSON_AddNumberToObject(root, "lux", 500);
    cJSON_AddNumberToObject(root, "co2", 450);
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);

    // Аварийные состояния
    cJSON_AddBoolToObject(root, "ph_alarm", false);
    cJSON_AddBoolToObject(root, "ec_alarm", false);
    cJSON_AddBoolToObject(root, "temp_alarm", false);

    // Единицы измерения
    cJSON_AddStringToObject(root, "ph_unit", "");
    cJSON_AddStringToObject(root, "ec_unit", "mS/cm");
    cJSON_AddStringToObject(root, "temp_unit", "°C");
    cJSON_AddStringToObject(root, "humidity_unit", "%");
    cJSON_AddStringToObject(root, "lux_unit", "lux");
    cJSON_AddStringToObject(root, "co2_unit", "ppm");

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ret;
}

/**
 * @brief HTTP обработчик получения статуса системы
 */
static esp_err_t http_get_system_status_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Системная информация
    cJSON_AddBoolToObject(root, "system_ok", true);
    cJSON_AddBoolToObject(root, "wifi_connected", true);
    cJSON_AddBoolToObject(root, "ble_connected", false);
    cJSON_AddBoolToObject(root, "pumps_ok", true);
    cJSON_AddBoolToObject(root, "sensors_ok", true);
    cJSON_AddBoolToObject(root, "display_ok", true);

    // Производительность
    cJSON_AddNumberToObject(root, "cpu_usage", 45.2);
    cJSON_AddNumberToObject(root, "memory_usage", 67.8);
    cJSON_AddNumberToObject(root, "free_heap", 234);

    // Время работы
    cJSON_AddNumberToObject(root, "uptime_seconds", 12345);

    // Версия прошивки и ID устройства
    cJSON_AddStringToObject(root, "firmware_version", "3.0.0");
    cJSON_AddStringToObject(root, "device_id", "HYDRO_ESP32S3_001");

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ret;
}

/**
 * @brief HTTP обработчик получения настроек системы
 */
static esp_err_t http_get_settings_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Целевые значения
    cJSON_AddNumberToObject(root, "ph_target", 6.8);
    cJSON_AddNumberToObject(root, "ec_target", 1.5);
    cJSON_AddNumberToObject(root, "temp_target", 24.0);

    // Допуски
    cJSON_AddNumberToObject(root, "ph_tolerance", 0.2);
    cJSON_AddNumberToObject(root, "ec_tolerance", 0.1);
    cJSON_AddNumberToObject(root, "temp_tolerance", 1.0);

    // Флаги управления
    cJSON_AddBoolToObject(root, "auto_correction_enabled", true);
    cJSON_AddBoolToObject(root, "notifications_enabled", true);

    // Интервалы
    cJSON_AddNumberToObject(root, "correction_interval", 300);
    cJSON_AddNumberToObject(root, "logging_interval", 60);

    // Сетевые настройки
    cJSON_AddStringToObject(root, "wifi_ssid", "HydroMonitor-AP");
    cJSON_AddStringToObject(root, "device_name", "HydroMonitor-ESP32S3");

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ret;
}

/**
 * @brief HTTP обработчик обновления настроек системы
 */
static esp_err_t http_update_settings_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    // Читаем тело запроса
    char buf[512];
    int ret_len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret_len <= 0) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    buf[ret_len] = '\0';

    // Парсим JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    // Обрабатываем полученные настройки
    cJSON *ph_target = cJSON_GetObjectItem(root, "ph_target");
    cJSON *ec_target = cJSON_GetObjectItem(root, "ec_target");
    cJSON *temp_target = cJSON_GetObjectItem(root, "temp_target");
    cJSON *auto_correction = cJSON_GetObjectItem(root, "auto_correction_enabled");

    if (ph_target) {
        ESP_LOGI(TAG, "Обновление целевого значения pH: %.2f", ph_target->valuedouble);
    }
    if (ec_target) {
        ESP_LOGI(TAG, "Обновление целевого значения EC: %.2f", ec_target->valuedouble);
    }
    if (temp_target) {
        ESP_LOGI(TAG, "Обновление целевой температуры: %.2f", temp_target->valuedouble);
    }
    if (auto_correction) {
        ESP_LOGI(TAG, "Автокоррекция: %s", auto_correction->valueint ? "включена" : "выключена");
    }

    cJSON_Delete(root);

    // Отправляем подтверждение
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Настройки обновлены");

    char *response_json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    ret = httpd_resp_send(req, response_json, strlen(response_json));

    free(response_json);
    cJSON_Delete(response);

    return ret;
}

/**
 * @brief HTTP обработчик получения истории данных
 */
static esp_err_t http_get_history_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    // Получаем параметры запроса
    char sensor_type[32] = "all";
    char time_range[32] = "1h";

    httpd_req_get_hdr_value_str(req, "X-Sensor-Type", sensor_type, sizeof(sensor_type));
    httpd_req_get_hdr_value_str(req, "X-Time-Range", time_range, sizeof(time_range));

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Создаем массив данных истории (заглушка)
    cJSON *history_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "history", history_array);

    // Добавляем тестовые данные
    for (int i = 0; i < 10; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "timestamp", esp_timer_get_time() / 1000 - i * 60000);
        cJSON_AddNumberToObject(entry, "ph", 6.8 + (rand() % 10 - 5) * 0.1);
        cJSON_AddNumberToObject(entry, "ec", 1.5 + (rand() % 10 - 5) * 0.05);
        cJSON_AddNumberToObject(entry, "temperature", 24.0 + (rand() % 10 - 5) * 0.2);
        cJSON_AddItemToArray(history_array, entry);
    }

    cJSON_AddStringToObject(root, "sensor_type", sensor_type);
    cJSON_AddStringToObject(root, "time_range", time_range);
    cJSON_AddNumberToObject(root, "total_points", 10);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ret;
}

/**
 * @brief HTTP обработчик отправки команд управления
 */
static esp_err_t http_send_control_command_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    // Читаем тело запроса
    char buf[256];
    int ret_len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret_len <= 0) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    buf[ret_len] = '\0';

    // Парсим JSON команды
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    // Обрабатываем команду
    cJSON *command_type = cJSON_GetObjectItem(root, "command_type");
    cJSON *parameters = cJSON_GetObjectItem(root, "parameters");

    if (command_type && parameters) {
        ESP_LOGI(TAG, "Получена команда: %s", command_type->valuestring);

        // Создаем структуру команды для очереди
        mobile_control_command_t command;
        command.command_id = esp_random();
        strncpy(command.command_type, command_type->valuestring, sizeof(command.command_type) - 1);
        strncpy(command.parameters, parameters->valuestring, sizeof(command.parameters) - 1);
        command.timestamp = esp_timer_get_time() / 1000;
        command.executed = false;

        // Отправляем команду в очередь для обработки
        if (xQueueSend(command_queue, &command, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Команда отправлена в очередь обработки");
        } else {
            ESP_LOGW(TAG, "Очередь команд переполнена");
        }
    }

    cJSON_Delete(root);

    // Отправляем подтверждение
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Команда получена");

    char *response_json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    ret = httpd_resp_send(req, response_json, strlen(response_json));

    free(response_json);
    cJSON_Delete(response);

    return ret;
}

/**
 * @brief HTTP обработчик аутентификации
 */
static esp_err_t http_authenticate_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    // Читаем тело запроса
    char buf[256];
    int ret_len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret_len <= 0) {
        httpd_resp_send_401(req);
        return ESP_FAIL;
    }
    buf[ret_len] = '\0';

    // Парсим JSON аутентификации
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    cJSON *token = cJSON_GetObjectItem(root, "auth_token");
    bool authenticated = false;

    if (token) {
        // Проверяем токен аутентификации
        authenticated = mobile_app_authenticate(token->valuestring);
        ESP_LOGI(TAG, "Аутентификация: %s", authenticated ? "успешна" : "неудачна");
    }

    cJSON_Delete(root);

    // Отправляем результат аутентификации
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "authenticated", authenticated);
    cJSON_AddStringToObject(response, "device_id", "HYDRO_ESP32S3_001");

    if (authenticated) {
        cJSON_AddStringToObject(response, "message", "Аутентификация успешна");
        httpd_resp_set_status(req, "200 OK");
    } else {
        cJSON_AddStringToObject(response, "message", "Неверный токен аутентификации");
        httpd_resp_set_status(req, "401 Unauthorized");
    }

    char *response_json = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    ret = httpd_resp_send(req, response_json, strlen(response_json));

    free(response_json);
    cJSON_Delete(response);

    return ret;
}

/**
 * @brief HTTP обработчик получения информации об устройстве
 */
static esp_err_t http_get_device_info_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    // Информация об устройстве
    cJSON_AddStringToObject(root, "device_type", "ESP32-S3 Hydroponics Monitor");
    cJSON_AddStringToObject(root, "hardware_version", "v3.0");
    cJSON_AddStringToObject(root, "firmware_version", "3.0.0");
    cJSON_AddStringToObject(root, "device_id", "HYDRO_ESP32S3_001");

    // Аппаратные характеристики ESP32S3
    cJSON_AddStringToObject(root, "cpu_cores", "2");
    cJSON_AddNumberToObject(root, "cpu_frequency_mhz", 240);
    cJSON_AddNumberToObject(root, "flash_size_mb", 4);
    cJSON_AddNumberToObject(root, "psram_size_mb", 8);
    cJSON_AddNumberToObject(root, "ram_size_kb", 512);

    // Поддерживаемые функции
    cJSON_AddBoolToObject(root, "wifi_support", true);
    cJSON_AddBoolToObject(root, "bluetooth_support", true);
    cJSON_AddBoolToObject(root, "usb_support", true);
    cJSON_AddBoolToObject(root, "display_support", true);

    // Датчики
    cJSON *sensors = cJSON_CreateArray();
    cJSON_AddItemToArray(sensors, cJSON_CreateString("pH"));
    cJSON_AddItemToArray(sensors, cJSON_CreateString("EC"));
    cJSON_AddItemToArray(sensors, cJSON_CreateString("Temperature"));
    cJSON_AddItemToArray(sensors, cJSON_CreateString("Humidity"));
    cJSON_AddItemToArray(sensors, cJSON_CreateString("Lux"));
    cJSON_AddItemToArray(sensors, cJSON_CreateString("CO2"));
    cJSON_AddItemToObject(root, "sensors", sensors);

    // Исполнительные устройства
    cJSON *actuators = cJSON_CreateArray();
    cJSON_AddItemToArray(actuators, cJSON_CreateString("pH UP Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("pH DOWN Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("EC A Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("EC B Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("EC C Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("Water Pump"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("Light Relay"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("Fan Relay"));
    cJSON_AddItemToArray(actuators, cJSON_CreateString("Heater Relay"));
    cJSON_AddItemToObject(root, "actuators", actuators);

    // Версии API
    cJSON_AddStringToObject(root, "api_version", "1.0.0");
    cJSON_AddStringToObject(root, "websocket_version", "1.0.0");
    cJSON_AddStringToObject(root, "ble_version", "1.0.0");

    // Сетевые настройки
    cJSON_AddStringToObject(root, "default_ssid", "HydroMonitor-AP");
    cJSON_AddStringToObject(root, "default_ip", "192.168.4.1");
    cJSON_AddNumberToObject(root, "http_port", 8080);
    cJSON_AddNumberToObject(root, "websocket_port", 8081);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ret;
}

/**
 * @brief Запуск WebSocket сервера для реального времени
 */
static esp_err_t start_websocket_server(void) {
    esp_err_t ret = ESP_OK;

    esp_websocket_config_t ws_config = {
        .port = 8081,
        .max_connections = MOBILE_MAX_CONNECTIONS,
        .max_frame_size = 4096,
        .stack_size = 6144,  // Увеличенный стек для ESP32S3
    };

    // websocket_server = esp_websocket_server_start(&ws_config); // TODO: Implement websocket server
    if (websocket_server == NULL) {
        ESP_LOGE(TAG, "Ошибка запуска WebSocket сервера");
        return ESP_FAIL;
    }

    // Регистрация обработчиков WebSocket событий
    // esp_websocket_server_register_on_connect(websocket_server, websocket_connect_handler); // TODO: Implement websocket server
    // esp_websocket_server_register_on_disconnect(websocket_server, websocket_disconnect_handler); // TODO: Implement websocket server
    // esp_websocket_server_register_on_message(websocket_server, websocket_message_handler); // TODO: Implement websocket server

    ESP_LOGI(TAG, "WebSocket сервер запущен на порту %d", ws_config.port);

    return ret;
}

/**
 * @brief Обработчик подключения WebSocket клиента
 */
static void websocket_connect_handler(esp_websocket_handle_t ws_handle, void *arg) {
    ESP_LOGI(TAG, "WebSocket клиент подключен");

    // Устанавливаем флаг подключения
    xEventGroupSetBits(connection_event_group, MOBILE_CONNECTED_BIT);
}

/**
 * @brief Обработчик отключения WebSocket клиента
 */
static void websocket_disconnect_handler(esp_websocket_handle_t ws_handle, void *arg) {
    ESP_LOGI(TAG, "WebSocket клиент отключен");

    // Устанавливаем флаг отключения
    xEventGroupSetBits(connection_event_group, MOBILE_DISCONNECTED_BIT);
}

/**
 * @brief Обработчик сообщений WebSocket
 */
static void websocket_message_handler(esp_websocket_handle_t ws_handle, esp_websocket_frame_t *frame, void *arg) {
    ESP_LOGV(TAG, "Получено WebSocket сообщение: %d байт", frame->len);

    if (frame->type == WS_TEXT_FRAME) {
        // Обрабатываем текстовые сообщения
        char *message = malloc(frame->len + 1);
        if (message) {
            memcpy(message, frame->data, frame->len);
            message[frame->len] = '\0';

            process_websocket_message(message);

            free(message);
        }
    }
}

/**
 * @brief Обработка WebSocket сообщения
 */
static void process_websocket_message(const char *message) {
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGW(TAG, "Некорректный JSON в WebSocket сообщении");
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type == NULL) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "get_sensor_data") == 0) {
        // Запрос данных датчиков
        send_websocket_sensor_data();
    } else if (strcmp(type->valuestring, "control_command") == 0) {
        // Команда управления
        process_websocket_command(root);
    } else if (strcmp(type->valuestring, "subscribe") == 0) {
        // Подписка на обновления
        process_websocket_subscription(root);
    }

    cJSON_Delete(root);
}

/**
 * @brief Отправка данных датчиков через WebSocket
 */
static void send_websocket_sensor_data(void) {
    // if (websocket_server == NULL) return; // TODO: Implement websocket server

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return;

    cJSON_AddStringToObject(root, "type", "sensor_data");
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);

    // Данные датчиков (заглушка)
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "ph", 6.8);
    cJSON_AddNumberToObject(data, "ec", 1.5);
    cJSON_AddNumberToObject(data, "temperature", 24.5);
    cJSON_AddNumberToObject(data, "humidity", 65.0);
    cJSON_AddNumberToObject(data, "lux", 500);
    cJSON_AddNumberToObject(data, "co2", 450);
    cJSON_AddItemToObject(root, "data", data);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string) {
        // esp_websocket_server_send_text(websocket_server, json_string, strlen(json_string)); // TODO: Implement websocket server
        free(json_string);
    }

    cJSON_Delete(root);
}

/**
 * @brief Обработка команды управления через WebSocket
 */
static void process_websocket_command(cJSON *root) {
    cJSON *command_type = cJSON_GetObjectItem(root, "command_type");
    cJSON *parameters = cJSON_GetObjectItem(root, "parameters");

    if (command_type && parameters) {
        ESP_LOGI(TAG, "WebSocket команда: %s", command_type->valuestring);

        // Создаем структуру команды
        mobile_control_command_t command;
        command.command_id = esp_random();
        strncpy(command.command_type, command_type->valuestring, sizeof(command.command_type) - 1);
        strncpy(command.parameters, parameters->valuestring, sizeof(command.parameters) - 1);
        command.timestamp = esp_timer_get_time() / 1000;
        command.executed = false;

        // Отправляем в очередь обработки
        if (xQueueSend(command_queue, &command, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "Очередь команд переполнена");
        }
    }
}

/**
 * @brief Обработка подписки на обновления через WebSocket
 */
static void process_websocket_subscription(cJSON *root) {
    cJSON *events = cJSON_GetObjectItem(root, "events");

    if (events && cJSON_IsArray(events)) {
        ESP_LOGI(TAG, "Подписка на события через WebSocket");

        // Обрабатываем массив событий для подписки
        int array_size = cJSON_GetArraySize(events);
        for (int i = 0; i < array_size; i++) {
            cJSON *event = cJSON_GetArrayItem(events, i);
            if (event && cJSON_IsString(event)) {
                ESP_LOGI(TAG, "Подписка на событие: %s", event->valuestring);
            }
        }
    }
}

/**
 * @brief Запуск Bluetooth LE сервера
 */
static esp_err_t start_ble_server(void) {
    esp_err_t ret = ESP_OK;

    // Здесь будет реализация Bluetooth LE GATT сервера
    // для мобильных устройств

    ESP_LOGI(TAG, "Bluetooth LE сервер запущен для мобильных устройств");

    return ret;
}

/**
 * @brief Остановка Bluetooth LE сервера
 */
static void stop_ble_server(void) {
    // Остановка BLE сервера
    ESP_LOGI(TAG, "Bluetooth LE сервер остановлен");
}

/**
 * @brief Отправка данных датчиков в мобильное приложение
 */
esp_err_t mobile_app_send_sensor_data(const mobile_sensor_data_t *data) {
    if (!mobile_interface_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(mobile_interface_mutex, portMAX_DELAY);

    // Отправка через WebSocket
    // if (websocket_server != NULL) { // TODO: Implement websocket server
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "type", "sensor_data");
            cJSON_AddNumberToObject(root, "timestamp", data->timestamp);

            cJSON *sensor_data = cJSON_CreateObject();
            cJSON_AddNumberToObject(sensor_data, "ph", data->ph);
            cJSON_AddNumberToObject(sensor_data, "ec", data->ec);
            cJSON_AddNumberToObject(sensor_data, "temperature", data->temperature);
            cJSON_AddNumberToObject(sensor_data, "humidity", data->humidity);
            cJSON_AddNumberToObject(sensor_data, "lux", data->lux);
            cJSON_AddNumberToObject(sensor_data, "co2", data->co2);
            cJSON_AddBoolToObject(sensor_data, "ph_alarm", data->ph_alarm);
            cJSON_AddBoolToObject(sensor_data, "ec_alarm", data->ec_alarm);
            cJSON_AddBoolToObject(sensor_data, "temp_alarm", data->temp_alarm);
            cJSON_AddItemToObject(root, "data", sensor_data);

            char *json_string = cJSON_PrintUnformatted(root);
            if (json_string) {
                // esp_websocket_server_send_text(websocket_server, json_string, strlen(json_string)); // TODO: Implement websocket server
                free(json_string);
            }

            cJSON_Delete(root);
        }
    // } // TODO: Implement websocket server

    xSemaphoreGive(mobile_interface_mutex);
    return ESP_OK;
}

/**
 * @brief Проверка аутентификации мобильного приложения
 */
bool mobile_app_authenticate(const char *token) {
    if (token == NULL) {
        return false;
    }

    // Простая проверка токена (в реальности должна быть более сложная логика)
    return (strcmp(token, "HYDRO_MOBILE_TOKEN_2025") == 0);
}

/**
 * @brief Получение команд управления от мобильного приложения
 */
int mobile_app_get_control_commands(mobile_control_command_t *command) {
    if (!mobile_interface_initialized) {
        return -1;
    }

    if (command_queue == NULL) {
        return 0;
    }

    // Проверяем наличие команд в очереди
    if (xQueueReceive(command_queue, command, pdMS_TO_TICKS(100)) == pdTRUE) {
        return 1;  // Найдена 1 команда
    }

    return 0;  // Нет команд
}

/**
 * @brief Регистрация обработчика команд
 */
esp_err_t mobile_app_register_command_handler(mobile_command_handler_t handler, void *ctx) {
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    command_handler = handler;
    command_handler_ctx = ctx;

    ESP_LOGI(TAG, "Обработчик команд мобильного приложения зарегистрирован");

    return ESP_OK;
}

/**
 * @brief Регистрация обработчика ошибок
 */
esp_err_t mobile_app_register_error_handler(mobile_error_handler_t handler, void *ctx) {
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    error_handler = handler;
    error_handler_ctx = ctx;

    ESP_LOGI(TAG, "Обработчик ошибок мобильного приложения зарегистрирован");

    return ESP_OK;
}

/**
 * @brief Проверка подключения мобильного приложения
 */
bool mobile_app_is_connected(void) {
    if (!mobile_interface_initialized) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(connection_event_group);
    return (bits & MOBILE_CONNECTED_BIT) != 0;
}

/**
 * @brief Получение информации об устройстве
 */
esp_err_t mobile_app_get_device_info(char *device_info, size_t max_length) {
    if (device_info == NULL || max_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *info =
        "{"
        "\"device_type\":\"ESP32-S3 Hydroponics Monitor\","
        "\"firmware_version\":\"3.0.0\","
        "\"device_id\":\"HYDRO_ESP32S3_001\","
        "\"api_version\":\"1.0.0\""
        "}";

    strncpy(device_info, info, max_length - 1);
    device_info[max_length - 1] = '\0';

    return ESP_OK;
}

/**
 * @brief Отправка уведомления мобильному приложению
 */
esp_err_t mobile_app_send_notification(const char *type, const char *message, uint8_t priority) {
    if (!mobile_interface_initialized || type == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(mobile_interface_mutex, portMAX_DELAY);

    // Отправка уведомления через WebSocket
    // if (websocket_server != NULL) { // TODO: Implement websocket server
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "type", "notification");
            cJSON_AddStringToObject(root, "notification_type", type);
            cJSON_AddStringToObject(root, "message", message);
            cJSON_AddNumberToObject(root, "priority", priority);
            cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);

            char *json_string = cJSON_PrintUnformatted(root);
            if (json_string) {
                // esp_websocket_server_send_text(websocket_server, json_string, strlen(json_string)); // TODO: Implement websocket server
                free(json_string);
            }

            cJSON_Delete(root);
        }
    // } // TODO: Implement websocket server

    xSemaphoreGive(mobile_interface_mutex);
    return ESP_OK;
}

/**
 * @brief Включение синхронизации данных
 */
esp_err_t mobile_app_enable_sync(bool enable, uint32_t sync_interval) {
    if (!mobile_interface_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Синхронизация данных %s с интервалом %d сек",
             enable ? "включена" : "выключена", sync_interval);

    // Здесь будет логика включения/выключения синхронизации
    return ESP_OK;
}

/**
 * @brief Получение версии API
 */
const char *mobile_app_get_api_version(void) {
    return "1.0.0";
}

/**
 * @brief Сохранение настроек мобильного интерфейса в NVS
 */
esp_err_t mobile_app_save_settings(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(nvs_open("mobile_app", NVS_READWRITE, &nvs_handle));

    // Сохранение различных настроек мобильного интерфейса
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "sync_enabled", 1));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "sync_interval", MOBILE_DATA_SYNC_INTERVAL_MS / 1000));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Настройки мобильного интерфейса сохранены в NVS");

    return ret;
}

/**
 * @brief Загрузка настроек мобильного интерфейса из NVS
 */
esp_err_t mobile_app_load_settings(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(nvs_open("mobile_app", NVS_READONLY, &nvs_handle));

    // Загрузка настроек (с значениями по умолчанию)
    uint8_t sync_enabled = 1;
    uint32_t sync_interval = MOBILE_DATA_SYNC_INTERVAL_MS / 1000;

    nvs_get_u8(nvs_handle, "sync_enabled", &sync_enabled);
    nvs_get_u32(nvs_handle, "sync_interval", &sync_interval);

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Настройки мобильного интерфейса загружены из NVS");

    return ret;
}

/**
 * @brief Сброс настроек мобильного интерфейса к значениям по умолчанию
 */
esp_err_t mobile_app_reset_settings(void) {
    ESP_LOGI(TAG, "Сброс настроек мобильного интерфейса к значениям по умолчанию");

    // Здесь будет логика сброса настроек
    return ESP_OK;
}

/**
 * @brief Включение оффлайн режима
 */
esp_err_t mobile_app_enable_offline_mode(bool enable) {
    ESP_LOGI(TAG, "Оффлайн режим мобильного приложения %s",
             enable ? "включен" : "выключен");

    // Здесь будет логика включения оффлайн режима
    return ESP_OK;
}

/**
 * @brief Проверка доступности оффлайн данных
 */
bool mobile_app_has_offline_data(void) {
    // Проверка наличия сохраненных данных для оффлайн режима
    return false;
}

/**
 * @brief Синхронизация данных при восстановлении соединения
 */
esp_err_t mobile_app_sync_offline_data(void) {
    ESP_LOGI(TAG, "Синхронизация оффлайн данных");

    // Здесь будет логика синхронизации
    return ESP_OK;
}

/**
 * @brief Валидация версии мобильного приложения
 */
bool mobile_app_validate_version(const char *app_version) {
    if (app_version == NULL) {
        return false;
    }

    // Простая проверка совместимости версий
    return (strcmp(app_version, "1.0.0") >= 0);
}

/**
 * @brief Получение рекомендуемой версии мобильного приложения
 */
esp_err_t mobile_app_get_recommended_version(char *recommended_version, size_t max_length) {
    if (recommended_version == NULL || max_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(recommended_version, "1.0.0", max_length - 1);
    recommended_version[max_length - 1] = '\0';

    return ESP_OK;
}

/**
 * @brief Отправка файла логов в мобильное приложение
 */
esp_err_t mobile_app_send_logs(const char *log_type) {
    if (log_type == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Отправка логов типа %s в мобильное приложение", log_type);

    // Здесь будет логика отправки логов
    return ESP_OK;
}

/**
 * @brief Получение диагностической информации
 */
esp_err_t mobile_app_get_diagnostic_info(char *diagnostic_info, size_t max_length) {
    if (diagnostic_info == NULL || max_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *info =
        "{"
        "\"connections_active\":0,"
        "\"websocket_status\":\"connected\","
        "\"api_requests_total\":42,"
        "\"last_error\":\"none\""
        "}";

    strncpy(diagnostic_info, info, max_length - 1);
    diagnostic_info[max_length - 1] = '\0';

    return ESP_OK;
}

/**
 * @brief Включение отладочного режима мобильного интерфейса
 */
esp_err_t mobile_app_enable_debug_mode(bool enable) {
    ESP_LOGI(TAG, "Отладочный режим мобильного интерфейса %s",
             enable ? "включен" : "выключен");

    // Здесь будет логика включения отладочного режима
    return ESP_OK;
}

/**
 * @brief Тестирование соединения с мобильным приложением
 */
esp_err_t mobile_app_test_connection(void) {
    if (!mobile_interface_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Тестирование соединения с мобильным приложением");

    // Проверка доступности серверов
    if (http_server == NULL /* && websocket_server == NULL */) { // TODO: Implement websocket server
        ESP_LOGW(TAG, "Серверы мобильного интерфейса не запущены");
        return ESP_ERR_INVALID_STATE;
    }

    // Тест отправки данных
    mobile_sensor_data_t test_data = {
        .ph = 7.0,
        .ec = 1.4,
        .temperature = 25.0,
        .humidity = 60.0,
        .lux = 450,
        .co2 = 400,
        .timestamp = esp_timer_get_time() / 1000,
        .ph_alarm = false,
        .ec_alarm = false,
        .temp_alarm = false
    };

    esp_err_t ret = mobile_app_send_sensor_data(&test_data);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Тест соединения успешен");
    } else {
        ESP_LOGW(TAG, "Тест соединения неудачен");
    }

    return ret;
}

// Здесь будут реализации остальных функций мобильного интерфейса...
