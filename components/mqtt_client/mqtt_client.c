/**
 * @file mqtt_client.c
 * @brief Реализация MQTT клиента для IoT системы
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "mqtt_client.h"
#include "esp_log.h"
#include "mqtt_client.h" // ESP-IDF MQTT
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

/// Тег для логирования
static const char *TAG = "MQTT_CLIENT";

/// Глобальные переменные
static esp_mqtt_client_handle_t mqtt_client = NULL;
static SemaphoreHandle_t mqtt_mutex = NULL;
static bool mqtt_connected = false;
static char device_id[32] = "hydro_gateway_001";

/// Callbacks
static mqtt_command_callback_t command_callback = NULL;
static void *command_callback_ctx = NULL;
static mqtt_connection_callback_t connection_callback = NULL;
static void *connection_callback_ctx = NULL;

/// MQTT топики
#define MQTT_TOPIC_SENSORS_PH       "hydro/%s/sensors/ph"
#define MQTT_TOPIC_SENSORS_EC       "hydro/%s/sensors/ec"
#define MQTT_TOPIC_SENSORS_TEMP     "hydro/%s/sensors/temp"
#define MQTT_TOPIC_SENSORS_HUMIDITY "hydro/%s/sensors/humidity"
#define MQTT_TOPIC_SENSORS_LUX      "hydro/%s/sensors/lux"
#define MQTT_TOPIC_SENSORS_CO2      "hydro/%s/sensors/co2"
#define MQTT_TOPIC_COMMANDS         "hydro/%s/commands"
#define MQTT_TOPIC_STATUS           "hydro/%s/status"
#define MQTT_TOPIC_ALARMS           "hydro/%s/alarms"
#define MQTT_TOPIC_TELEMETRY        "hydro/%s/telemetry"

/**
 * @brief Парсинг команды из JSON
 */
static mqtt_command_type_t parse_command_type(const char *command_str) {
    if (strcmp(command_str, "set_ph_target") == 0) return MQTT_CMD_SET_PH_TARGET;
    if (strcmp(command_str, "set_ec_target") == 0) return MQTT_CMD_SET_EC_TARGET;
    if (strcmp(command_str, "start_pump") == 0) return MQTT_CMD_START_PUMP;
    if (strcmp(command_str, "stop_pump") == 0) return MQTT_CMD_STOP_PUMP;
    if (strcmp(command_str, "calibrate") == 0) return MQTT_CMD_CALIBRATE;
    if (strcmp(command_str, "reset") == 0) return MQTT_CMD_RESET;
    if (strcmp(command_str, "enable_auto") == 0) return MQTT_CMD_ENABLE_AUTO;
    if (strcmp(command_str, "disable_auto") == 0) return MQTT_CMD_DISABLE_AUTO;
    return MQTT_CMD_UNKNOWN;
}

/**
 * @brief Обработчик событий MQTT
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Подключено к MQTT брокеру");
            mqtt_connected = true;
            
            if (connection_callback) {
                connection_callback(true, connection_callback_ctx);
            }
            
            // Подписываемся на топик команд
            char topic[128];
            snprintf(topic, sizeof(topic), MQTT_TOPIC_COMMANDS, device_id);
            esp_mqtt_client_subscribe(mqtt_client, topic, 1);
            ESP_LOGI(TAG, "Подписка на топик: %s", topic);
            
            // Публикуем статус "online"
            mqtt_publish_status("online");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Отключено от MQTT брокера");
            mqtt_connected = false;
            
            if (connection_callback) {
                connection_callback(false, connection_callback_ctx);
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Подписка подтверждена, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Получено MQTT сообщение:");
            ESP_LOGI(TAG, "  Топик: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "  Данные: %.*s", event->data_len, event->data);
            
            // Обрабатываем команду
            if (command_callback) {
                char *data_str = malloc(event->data_len + 1);
                if (data_str) {
                    memcpy(data_str, event->data, event->data_len);
                    data_str[event->data_len] = '\0';
                    
                    cJSON *root = cJSON_Parse(data_str);
                    if (root) {
                        cJSON *command_json = cJSON_GetObjectItem(root, "command");
                        cJSON *payload_json = cJSON_GetObjectItem(root, "payload");
                        
                        if (command_json && cJSON_IsString(command_json)) {
                            mqtt_command_t command = {0};
                            command.type = parse_command_type(command_json->valuestring);
                            command.timestamp = esp_timer_get_time() / 1000;
                            
                            if (payload_json) {
                                char *payload_str = cJSON_PrintUnformatted(payload_json);
                                if (payload_str) {
                                    strncpy(command.payload, payload_str, sizeof(command.payload) - 1);
                                    free(payload_str);
                                }
                            }
                            
                            command_callback(&command, command_callback_ctx);
                        }
                        
                        cJSON_Delete(root);
                    }
                    
                    free(data_str);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Ошибка MQTT");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  TCP transport error");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "  Connection refused");
            }
            break;

        default:
            ESP_LOGD(TAG, "Событие MQTT id=%d", event->event_id);
            break;
    }
}

/**
 * @brief Инициализация MQTT клиента
 */
esp_err_t mqtt_client_init(const mqtt_client_config_t *config) {
    esp_err_t ret = ESP_OK;
    
    if (mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT клиент уже инициализирован");
        return ESP_OK;
    }
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Конфигурация MQTT не указана");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Создание мьютекса
    if (mqtt_mutex == NULL) {
        mqtt_mutex = xSemaphoreCreateMutex();
        if (mqtt_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(mqtt_mutex, portMAX_DELAY);
    
    // Сохраняем ID клиента
    strncpy(device_id, config->client_id, sizeof(device_id) - 1);
    
    // Конфигурация MQTT клиента
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,
        .credentials.client_id = config->client_id,
        .session.keepalive = config->keepalive > 0 ? config->keepalive : 120,
        .network.reconnect_timeout_ms = 10000,
        .network.disable_auto_reconnect = !config->auto_reconnect,
    };
    
    // Добавляем учетные данные если указаны
    if (strlen(config->username) > 0) {
        mqtt_cfg.credentials.username = config->username;
        mqtt_cfg.credentials.authentication.password = config->password;
    }
    
    // Создание MQTT клиента
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Ошибка создания MQTT клиента");
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    // Регистрация обработчика событий
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    ESP_LOGI(TAG, "MQTT клиент инициализирован: %s @ %s", config->client_id, config->broker_uri);
    
cleanup:
    xSemaphoreGive(mqtt_mutex);
    return ret;
}

/**
 * @brief Деинициализация MQTT клиента
 */
esp_err_t mqtt_client_deinit(void) {
    if (mqtt_client == NULL) {
        return ESP_OK;
    }
    
    xSemaphoreTake(mqtt_mutex, portMAX_DELAY);
    
    // Публикуем статус "offline" перед отключением
    mqtt_publish_status("offline");
    
    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_connected = false;
    
    ESP_LOGI(TAG, "MQTT клиент деинициализирован");
    
    xSemaphoreGive(mqtt_mutex);
    
    if (mqtt_mutex != NULL) {
        vSemaphoreDelete(mqtt_mutex);
        mqtt_mutex = NULL;
    }
    
    return ESP_OK;
}

/**
 * @brief Запуск MQTT клиента
 */
esp_err_t mqtt_client_start(void) {
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT клиент не инициализирован");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT клиент запущен");
    } else {
        ESP_LOGE(TAG, "Ошибка запуска MQTT клиента");
    }
    
    return ret;
}

/**
 * @brief Остановка MQTT клиента
 */
esp_err_t mqtt_client_stop(void) {
    if (mqtt_client == NULL) {
        return ESP_OK;
    }
    
    mqtt_publish_status("offline");
    
    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);
    mqtt_connected = false;
    
    ESP_LOGI(TAG, "MQTT клиент остановлен");
    
    return ret;
}

/**
 * @brief Проверка подключения
 */
bool mqtt_client_is_connected(void) {
    return mqtt_connected;
}

/**
 * @brief Публикация данных датчика (общая функция)
 */
static esp_err_t publish_sensor(const char *topic_template, float value, const char *status, const char *unit) {
    if (!mqtt_connected || mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char topic[128];
    char payload[256];
    
    snprintf(topic, sizeof(topic), topic_template, device_id);
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddNumberToObject(root, "value", value);
    cJSON_AddStringToObject(root, "unit", unit);
    cJSON_AddStringToObject(root, "status", status);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_str, 0, 1, 0);
    free(json_str);
    
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Публикация pH
 */
esp_err_t mqtt_publish_ph(float value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_PH, value, status, "pH");
}

/**
 * @brief Публикация EC
 */
esp_err_t mqtt_publish_ec(float value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_EC, value, status, "mS/cm");
}

/**
 * @brief Публикация температуры
 */
esp_err_t mqtt_publish_temperature(float value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_TEMP, value, status, "°C");
}

/**
 * @brief Публикация влажности
 */
esp_err_t mqtt_publish_humidity(float value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_HUMIDITY, value, status, "%");
}

/**
 * @brief Публикация освещённости
 */
esp_err_t mqtt_publish_lux(float value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_LUX, value, status, "lux");
}

/**
 * @brief Публикация CO2
 */
esp_err_t mqtt_publish_co2(uint16_t value, const char *status) {
    return publish_sensor(MQTT_TOPIC_SENSORS_CO2, (float)value, status, "ppm");
}

/**
 * @brief Публикация всех данных датчиков
 */
esp_err_t mqtt_publish_sensor_data(const mqtt_sensor_data_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    // Публикуем каждый датчик отдельно
    ret |= mqtt_publish_ph(data->ph, data->ph_alarm ? "alarm" : "ok");
    ret |= mqtt_publish_ec(data->ec, data->ec_alarm ? "alarm" : "ok");
    ret |= mqtt_publish_temperature(data->temperature, data->temp_alarm ? "alarm" : "ok");
    ret |= mqtt_publish_humidity(data->humidity, "ok");
    ret |= mqtt_publish_lux(data->lux, "ok");
    ret |= mqtt_publish_co2(data->co2, "ok");
    
    return ret;
}

/**
 * @brief Публикация аларма
 */
esp_err_t mqtt_publish_alarm(const char *type, const char *message, const char *severity) {
    if (!mqtt_connected || mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (type == NULL || message == NULL || severity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char topic[128];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_ALARMS, device_id);
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddStringToObject(root, "severity", severity);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_str, 0, 1, 0);
    free(json_str);
    
    ESP_LOGI(TAG, "Опубликован аларм [%s]: %s (severity=%s)", type, message, severity);
    
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Публикация телеметрии
 */
esp_err_t mqtt_publish_telemetry(uint32_t uptime, uint32_t free_heap, float cpu_usage) {
    if (!mqtt_connected || mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char topic[128];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_TELEMETRY, device_id);
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddNumberToObject(root, "uptime", uptime);
    cJSON_AddNumberToObject(root, "free_heap", free_heap);
    cJSON_AddNumberToObject(root, "cpu_usage", cpu_usage);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_str, 0, 0, 0);
    free(json_str);
    
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Публикация статуса
 */
esp_err_t mqtt_publish_status(const char *status) {
    if (mqtt_client == NULL || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char topic[128];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_STATUS, device_id);
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "device_id", device_id);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_str, 0, 1, 1); // QoS=1, retain=1
    free(json_str);
    
    ESP_LOGI(TAG, "Статус опубликован: %s", status);
    
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Подписка на команды
 */
esp_err_t mqtt_subscribe_commands(mqtt_command_callback_t callback, void *user_ctx) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    command_callback = callback;
    command_callback_ctx = user_ctx;
    
    ESP_LOGI(TAG, "Зарегистрирован callback для команд");
    
    return ESP_OK;
}

/**
 * @brief Регистрация callback подключения
 */
esp_err_t mqtt_register_connection_callback(mqtt_connection_callback_t callback, void *user_ctx) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    connection_callback = callback;
    connection_callback_ctx = user_ctx;
    
    ESP_LOGI(TAG, "Зарегистрирован callback подключения");
    
    return ESP_OK;
}

/**
 * @brief Получение ID устройства
 */
esp_err_t mqtt_get_device_id(char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(buffer, device_id, max_len - 1);
    buffer[max_len - 1] = '\0';
    
    return ESP_OK;
}

/**
 * @brief Установка ID устройства
 */
esp_err_t mqtt_set_device_id(const char *new_device_id) {
    if (new_device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(device_id, new_device_id, sizeof(device_id) - 1);
    device_id[sizeof(device_id) - 1] = '\0';
    
    ESP_LOGI(TAG, "ID устройства установлен: %s", device_id);
    
    return ESP_OK;
}

