/**
 * @file mesh_network.c
 * @brief Реализация mesh-сети через ESP-NOW
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "mesh_network.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_crc.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>

/// Тег для логирования
static const char *TAG = "MESH_NETWORK";

/// Глобальные переменные
static mesh_role_t current_role = MESH_ROLE_GATEWAY;
static uint8_t current_device_id = 0;
static SemaphoreHandle_t mesh_mutex = NULL;
static QueueHandle_t mesh_rx_queue = NULL;
static bool mesh_initialized = false;
static bool mesh_running = false;

/// Callbacks
static mesh_sensor_callback_t sensor_callback = NULL;
static void *sensor_callback_ctx = NULL;
static mesh_command_callback_t command_callback = NULL;
static void *command_callback_ctx = NULL;
static mesh_heartbeat_callback_t heartbeat_callback = NULL;
static void *heartbeat_callback_ctx = NULL;

/// Peer management
#define MAX_PEERS 10
typedef struct {
    uint8_t mac[6];
    uint8_t device_id;
    bool in_use;
    uint32_t last_seen;
} peer_info_t;

static peer_info_t peer_list[MAX_PEERS] = {0};
static uint8_t peer_count = 0;

/**
 * @brief Callback для отправки ESP-NOW
 */
static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    (void)tx_info; // Не используется
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGV(TAG, "ESP-NOW отправка успешна");
    } else {
        ESP_LOGW(TAG, "ESP-NOW отправка не удалась");
    }
}

/**
 * @brief Callback для приема ESP-NOW
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(mesh_message_t)) {
        mesh_message_t *msg = (mesh_message_t *)malloc(sizeof(mesh_message_t));
        if (msg) {
            memcpy(msg, data, sizeof(mesh_message_t));
            
            // Отправляем в очередь для обработки
            if (xQueueSend(mesh_rx_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Очередь переполнена, сообщение потеряно");
                free(msg);
            }
        }
    }
}

/**
 * @brief Задача обработки входящих сообщений
 */
static void mesh_rx_task(void *pvParameters) {
    mesh_message_t *msg;
    
    while (1) {
        if (xQueueReceive(mesh_rx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "Получено сообщение от устройства %d, тип %d", 
                     msg->device_id, msg->msg_type);
            
            switch (msg->msg_type) {
                case MESH_MSG_SENSOR_DATA:
                    if (sensor_callback) {
                        mesh_sensor_data_t *data = (mesh_sensor_data_t *)msg->payload;
                        sensor_callback(msg->device_id, data, sensor_callback_ctx);
                    }
                    break;
                    
                case MESH_MSG_COMMAND:
                    if (command_callback) {
                        mesh_command_t *command = (mesh_command_t *)msg->payload;
                        command_callback(command, command_callback_ctx);
                    }
                    break;
                    
                case MESH_MSG_HEARTBEAT:
                    if (heartbeat_callback) {
                        mesh_heartbeat_t *heartbeat = (mesh_heartbeat_t *)msg->payload;
                        heartbeat_callback(msg->device_id, heartbeat, heartbeat_callback_ctx);
                    }
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Неизвестный тип сообщения: %d", msg->msg_type);
                    break;
            }
            
            free(msg);
        }
    }
}

/**
 * @brief Инициализация mesh-сети
 */
esp_err_t mesh_network_init(mesh_role_t role, uint8_t device_id) {
    esp_err_t ret = ESP_OK;
    
    if (mesh_initialized) {
        ESP_LOGW(TAG, "Mesh-сеть уже инициализирована");
        return ESP_OK;
    }
    
    if (device_id == 0 || device_id == 0xFF) {
        ESP_LOGE(TAG, "Некорректный ID устройства");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Создание мьютекса
    if (mesh_mutex == NULL) {
        mesh_mutex = xSemaphoreCreateMutex();
        if (mesh_mutex == NULL) {
            ESP_LOGE(TAG, "Ошибка создания мьютекса");
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Создание очереди приема
    if (mesh_rx_queue == NULL) {
        mesh_rx_queue = xQueueCreate(10, sizeof(mesh_message_t *));
        if (mesh_rx_queue == NULL) {
            ESP_LOGE(TAG, "Ошибка создания очереди");
            return ESP_ERR_NO_MEM;
        }
    }
    
    xSemaphoreTake(mesh_mutex, portMAX_DELAY);
    
    current_role = role;
    current_device_id = device_id;
    
    // Инициализация ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка инициализации ESP-NOW: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    // Регистрация callbacks
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);
    
    mesh_initialized = true;
    
    ESP_LOGI(TAG, "Mesh-сеть инициализирована: роль=%s, ID=%d",
             role == MESH_ROLE_GATEWAY ? "Gateway" : "Slave", device_id);
    
cleanup:
    xSemaphoreGive(mesh_mutex);
    return ret;
}

/**
 * @brief Деинициализация mesh-сети
 */
esp_err_t mesh_network_deinit(void) {
    if (!mesh_initialized) {
        return ESP_OK;
    }
    
    mesh_network_stop();
    
    xSemaphoreTake(mesh_mutex, portMAX_DELAY);
    
    esp_now_deinit();
    
    mesh_initialized = false;
    peer_count = 0;
    memset(peer_list, 0, sizeof(peer_list));
    
    ESP_LOGI(TAG, "Mesh-сеть деинициализирована");
    
    xSemaphoreGive(mesh_mutex);
    
    if (mesh_rx_queue != NULL) {
        vQueueDelete(mesh_rx_queue);
        mesh_rx_queue = NULL;
    }
    
    if (mesh_mutex != NULL) {
        vSemaphoreDelete(mesh_mutex);
        mesh_mutex = NULL;
    }
    
    return ESP_OK;
}

/**
 * @brief Запуск mesh-сети
 */
esp_err_t mesh_network_start(void) {
    if (!mesh_initialized) {
        ESP_LOGE(TAG, "Mesh-сеть не инициализирована");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (mesh_running) {
        return ESP_OK;
    }
    
    // Создание задачи обработки
    BaseType_t ret = xTaskCreate(
        mesh_rx_task,
        "mesh_rx",
        4096,
        NULL,
        5,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Ошибка создания задачи приема");
        return ESP_FAIL;
    }
    
    mesh_running = true;
    ESP_LOGI(TAG, "Mesh-сеть запущена");
    
    return ESP_OK;
}

/**
 * @brief Остановка mesh-сети
 */
esp_err_t mesh_network_stop(void) {
    mesh_running = false;
    ESP_LOGI(TAG, "Mesh-сеть остановлена");
    return ESP_OK;
}

/**
 * @brief Регистрация peer
 */
esp_err_t mesh_register_peer(const uint8_t *peer_mac, uint8_t device_id) {
    if (!mesh_initialized || peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(mesh_mutex, portMAX_DELAY);
    
    // Поиск свободного слота
    int free_slot = -1;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (!peer_list[i].in_use) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) {
        ESP_LOGE(TAG, "Нет свободных слотов для peer");
        xSemaphoreGive(mesh_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Добавление peer в ESP-NOW
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, peer_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    
    esp_err_t ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка добавления peer: %s", esp_err_to_name(ret));
        xSemaphoreGive(mesh_mutex);
        return ret;
    }
    
    // Сохраняем информацию о peer
    memcpy(peer_list[free_slot].mac, peer_mac, 6);
    peer_list[free_slot].device_id = device_id;
    peer_list[free_slot].in_use = true;
    peer_list[free_slot].last_seen = esp_timer_get_time() / 1000;
    peer_count++;
    
    ESP_LOGI(TAG, "Peer registered: ID=%d, MAC=%02x:%02x:%02x:%02x:%02x:%02x", 
             device_id, peer_mac[0], peer_mac[1], peer_mac[2],
             peer_mac[3], peer_mac[4], peer_mac[5]);
    
    xSemaphoreGive(mesh_mutex);
    return ESP_OK;
}

/**
 * @brief Отправка данных датчиков
 */
esp_err_t mesh_send_sensor_data(const mesh_sensor_data_t *data) {
    if (!mesh_initialized || !mesh_running || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    mesh_message_t msg = {0};
    msg.device_id = current_device_id;
    msg.msg_type = MESH_MSG_SENSOR_DATA;
    msg.timestamp = esp_timer_get_time() / 1000;
    memcpy(msg.payload, data, sizeof(mesh_sensor_data_t));
    
    // Broadcast или отправка на gateway
    uint8_t *dest_mac = NULL; // NULL = broadcast
    
    esp_err_t ret = esp_now_send(dest_mac, (uint8_t *)&msg, sizeof(msg));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка отправки данных: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Broadcast команды
 */
esp_err_t mesh_broadcast_command(const mesh_command_t *command) {
    if (!mesh_initialized || !mesh_running || command == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    mesh_message_t msg = {0};
    msg.device_id = current_device_id;
    msg.msg_type = MESH_MSG_COMMAND;
    msg.timestamp = esp_timer_get_time() / 1000;
    memcpy(msg.payload, command, sizeof(mesh_command_t));
    
    esp_err_t ret = esp_now_send(NULL, (uint8_t *)&msg, sizeof(msg));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Команда отправлена broadcast");
    }
    
    return ret;
}

/**
 * @brief Отправка команды устройству
 */
esp_err_t mesh_send_command(uint8_t device_id, const mesh_command_t *command) {
    // TODO: Найти MAC по device_id и отправить напрямую
    return mesh_broadcast_command(command);
}

/**
 * @brief Отправка heartbeat
 */
esp_err_t mesh_send_heartbeat(const mesh_heartbeat_t *heartbeat) {
    if (!mesh_initialized || !mesh_running || heartbeat == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    mesh_message_t msg = {0};
    msg.device_id = current_device_id;
    msg.msg_type = MESH_MSG_HEARTBEAT;
    msg.timestamp = esp_timer_get_time() / 1000;
    memcpy(msg.payload, heartbeat, sizeof(mesh_heartbeat_t));
    
    return esp_now_send(NULL, (uint8_t *)&msg, sizeof(msg));
}

/// Регистрация callbacks
esp_err_t mesh_register_sensor_callback(mesh_sensor_callback_t callback, void *user_ctx) {
    sensor_callback = callback;
    sensor_callback_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t mesh_register_command_callback(mesh_command_callback_t callback, void *user_ctx) {
    command_callback = callback;
    command_callback_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t mesh_register_heartbeat_callback(mesh_heartbeat_callback_t callback, void *user_ctx) {
    heartbeat_callback = callback;
    heartbeat_callback_ctx = user_ctx;
    return ESP_OK;
}

/// Getters
uint8_t mesh_get_peer_count(void) {
    return peer_count;
}

mesh_role_t mesh_get_role(void) {
    return current_role;
}

uint8_t mesh_get_device_id(void) {
    return current_device_id;
}

bool mesh_is_connected_to_gateway(void) {
    return (peer_count > 0);
}

