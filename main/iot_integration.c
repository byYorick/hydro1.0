/**
 * @file iot_integration.c
 * @brief Реализация интеграции IoT компонентов
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#include "iot_integration.h"
#include "iot_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Подключение IoT компонентов
#if IOT_MQTT_ENABLED
#include "mqtt_client.h"
#endif

#if IOT_TELEGRAM_ENABLED
#include "telegram_bot.h"
#endif

#if IOT_SD_ENABLED
#include "sd_storage.h"
#endif

#if IOT_MESH_ENABLED
#include "mesh_network.h"
#endif

#include "network_manager.h"

static const char *TAG = "IOT_INTEGRATION";

// Флаги состояния
static bool iot_initialized = false;
static bool iot_running = false;

/**
 * @brief Инициализация IoT системы
 */
esp_err_t iot_system_init(void) {
    esp_err_t ret = ESP_OK;
    
    if (iot_initialized) {
        ESP_LOGW(TAG, "IoT система уже инициализирована");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "=== Инициализация IoT системы ===");
    ESP_LOGI(TAG, "Устройство: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "Версия: %s", FIRMWARE_VERSION);
    
    // ========================================================================
    // 1. Network Manager
    // ========================================================================
    ESP_LOGI(TAG, "1. Инициализация Network Manager...");
    
    ret = network_manager_init(NETWORK_MODE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка инициализации Network Manager");
        return ret;
    }
    
    // Настройка WiFi
    network_wifi_config_t wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .auto_reconnect = WIFI_AUTO_RECONNECT,
        .use_static_ip = false,
    };
    
    if (NETWORK_MODE == NETWORK_MODE_STA || NETWORK_MODE == NETWORK_MODE_HYBRID) {
        ret = network_manager_connect_wifi(&wifi_config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Не удалось подключиться к WiFi");
        }
    }
    
    // ========================================================================
    // 2. SD Card Storage
    // ========================================================================
#if IOT_SD_ENABLED
    ESP_LOGI(TAG, "2. Инициализация SD Card...");
    
    sd_storage_config_t sd_config = {
        .mode = SD_MODE,
        .mosi_pin = SD_MOSI_PIN,
        .miso_pin = SD_MISO_PIN,
        .sck_pin = SD_SCK_PIN,
        .cs_pin = SD_CS_PIN,
        .max_frequency = SD_MAX_FREQUENCY,
        .format_if_mount_failed = SD_FORMAT_IF_FAILED,
    };
    
    ret = sd_storage_init(&sd_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD-карта недоступна, продолжаем без неё");
    } else {
        ESP_LOGI(TAG, "SD-карта успешно инициализирована");
    }
#else
    ESP_LOGI(TAG, "2. SD Card отключен");
#endif
    
    // ========================================================================
    // 3. MQTT Client
    // ========================================================================
#if IOT_MQTT_ENABLED
    ESP_LOGI(TAG, "3. Инициализация MQTT Client...");
    
    mqtt_client_config_t mqtt_config = {
        .broker_uri = MQTT_BROKER_URI,
        .client_id = MQTT_CLIENT_ID,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .keepalive = MQTT_KEEPALIVE,
        .auto_reconnect = MQTT_AUTO_RECONNECT,
    };
    
    ret = mqtt_client_init(&mqtt_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Ошибка инициализации MQTT");
    } else {
        ESP_LOGI(TAG, "MQTT Client инициализирован");
    }
#else
    ESP_LOGI(TAG, "3. MQTT отключен");
#endif
    
    // ========================================================================
    // 4. Telegram Bot
    // ========================================================================
#if IOT_TELEGRAM_ENABLED
    ESP_LOGI(TAG, "4. Инициализация Telegram Bot...");
    
    telegram_config_t telegram_config = {
        .bot_token = TELEGRAM_BOT_TOKEN,
        .chat_id = TELEGRAM_CHAT_ID,
        .poll_interval = TELEGRAM_POLL_INTERVAL,
        .enable_commands = TELEGRAM_ENABLE_COMMANDS,
    };
    
    ret = telegram_bot_init(&telegram_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Ошибка инициализации Telegram");
    } else {
        ESP_LOGI(TAG, "Telegram Bot инициализирован");
    }
#else
    ESP_LOGI(TAG, "4. Telegram Bot отключен");
#endif
    
    // ========================================================================
    // 5. Mesh Network
    // ========================================================================
#if IOT_MESH_ENABLED
    ESP_LOGI(TAG, "5. Инициализация Mesh Network...");
    
    ret = mesh_network_init(MESH_ROLE, MESH_DEVICE_ID);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Ошибка инициализации Mesh");
    } else {
        ESP_LOGI(TAG, "Mesh Network инициализирован");
    }
#else
    ESP_LOGI(TAG, "5. Mesh Network отключен");
#endif
    
    iot_initialized = true;
    ESP_LOGI(TAG, "=== IoT система инициализирована ===");
    
    return ESP_OK;
}

/**
 * @brief Запуск IoT системы
 */
esp_err_t iot_system_start(void) {
    if (!iot_initialized) {
        ESP_LOGE(TAG, "IoT система не инициализирована");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (iot_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "=== Запуск IoT сервисов ===");
    
#if IOT_MQTT_ENABLED
    ESP_LOGI(TAG, "Запуск MQTT...");
    mqtt_client_start();
#endif
    
#if IOT_TELEGRAM_ENABLED
    ESP_LOGI(TAG, "Запуск Telegram Bot...");
    telegram_bot_start();
    
    // Отправляем уведомление о запуске
    telegram_send_message("🚀 *Система запущена*\n\nГидропонная система готова к работе");
#endif
    
#if IOT_MESH_ENABLED
    ESP_LOGI(TAG, "Запуск Mesh Network...");
    mesh_network_start();
#endif
    
    iot_running = true;
    ESP_LOGI(TAG, "=== IoT сервисы запущены ===");
    
    return ESP_OK;
}

/**
 * @brief Остановка IoT системы
 */
esp_err_t iot_system_stop(void) {
    if (!iot_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Остановка IoT сервисов...");
    
#if IOT_MESH_ENABLED
    mesh_network_stop();
#endif
    
#if IOT_TELEGRAM_ENABLED
    telegram_send_message("⏸ Система остановлена");
    telegram_bot_stop();
#endif
    
#if IOT_MQTT_ENABLED
    mqtt_client_stop();
#endif
    
    iot_running = false;
    ESP_LOGI(TAG, "IoT сервисы остановлены");
    
    return ESP_OK;
}

/**
 * @brief Деинициализация IoT системы
 */
esp_err_t iot_system_deinit(void) {
    if (!iot_initialized) {
        return ESP_OK;
    }
    
    iot_system_stop();
    
#if IOT_MESH_ENABLED
    mesh_network_deinit();
#endif
    
#if IOT_TELEGRAM_ENABLED
    telegram_bot_deinit();
#endif
    
#if IOT_MQTT_ENABLED
    mqtt_client_deinit();
#endif
    
#if IOT_SD_ENABLED
    sd_storage_deinit();
#endif
    
    network_manager_deinit();
    
    iot_initialized = false;
    ESP_LOGI(TAG, "IoT система деинициализирована");
    
    return ESP_OK;
}

/**
 * @brief Публикация данных датчиков
 */
esp_err_t iot_publish_sensor_data(float ph, float ec, float temperature, 
                                   float humidity, float lux, uint16_t co2) {
    if (!iot_running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Публикация данных: pH=%.2f, EC=%.2f, Temp=%.1f°C", ph, ec, temperature);
    
    // ========================================================================
    // 1. Публикация в MQTT
    // ========================================================================
#if IOT_MQTT_ENABLED
    if (mqtt_client_is_connected()) {
        mqtt_sensor_data_t mqtt_data = {
            .ph = ph,
            .ec = ec,
            .temperature = temperature,
            .humidity = humidity,
            .lux = lux,
            .co2 = co2,
            .timestamp = esp_timer_get_time() / 1000,
            .ph_alarm = false,  // TODO: Получать из ph_ec_controller
            .ec_alarm = false,
            .temp_alarm = false,
        };
        
        mqtt_publish_sensor_data(&mqtt_data);
    }
#endif
    
    // ========================================================================
    // 2. Логирование на SD-карту
    // ========================================================================
#if IOT_SD_ENABLED
    if (sd_storage_is_mounted()) {
        sd_sensor_record_t sd_record = {
            .timestamp = time(NULL),
            .ph = ph,
            .ec = ec,
            .temperature = temperature,
            .humidity = humidity,
            .lux = lux,
            .co2 = co2,
        };
        
        sd_write_sensor_log(&sd_record);
    }
#endif
    
    // ========================================================================
    // 3. Отправка через Mesh (если slave)
    // ========================================================================
#if IOT_MESH_ENABLED
    if (mesh_get_role() == MESH_ROLE_SLAVE) {
        mesh_sensor_data_t mesh_data = {
            .device_id = mesh_get_device_id(),
            .ph = ph,
            .ec = ec,
            .temperature = temperature,
            .humidity = humidity,
            .lux = (uint16_t)lux,
            .co2 = co2,
            .timestamp = esp_timer_get_time() / 1000,
        };
        
        mesh_send_sensor_data(&mesh_data);
    }
#endif
    
    return ESP_OK;
}

/**
 * @brief Публикация аларма
 */
esp_err_t iot_publish_alarm(const char *type, const char *message, const char *severity) {
    if (!iot_running || type == NULL || message == NULL || severity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Аларм [%s]: %s (severity=%s)", type, message, severity);
    
    // ========================================================================
    // 1. MQTT
    // ========================================================================
#if IOT_MQTT_ENABLED
    if (mqtt_client_is_connected()) {
        mqtt_publish_alarm(type, message, severity);
    }
#endif
    
    // ========================================================================
    // 2. Telegram (только для critical/high)
    // ========================================================================
#if IOT_TELEGRAM_ENABLED
    telegram_severity_t tg_severity;
    
    if (strcmp(severity, "critical") == 0) {
        tg_severity = TELEGRAM_SEVERITY_CRITICAL;
        telegram_send_alarm(type, message, tg_severity);
    } else if (strcmp(severity, "high") == 0) {
        tg_severity = TELEGRAM_SEVERITY_ERROR;
        telegram_send_alarm(type, message, tg_severity);
    }
#endif
    
    // ========================================================================
    // 3. SD Log
    // ========================================================================
#if IOT_SD_ENABLED
    if (sd_storage_is_mounted()) {
        sd_event_record_t event = {
            .timestamp = time(NULL),
        };
        strncpy(event.type, type, sizeof(event.type) - 1);
        strncpy(event.message, message, sizeof(event.message) - 1);
        strncpy(event.severity, severity, sizeof(event.severity) - 1);
        
        sd_write_event_log(&event);
    }
#endif
    
    return ESP_OK;
}

/**
 * @brief Получение статистики IoT системы
 */
esp_err_t iot_get_system_stats(char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int offset = 0;
    
    offset += snprintf(buffer + offset, max_len - offset,
                      "IoT System Status:\n");
    
    offset += snprintf(buffer + offset, max_len - offset,
                      "- Initialized: %s\n", iot_initialized ? "Yes" : "No");
    
    offset += snprintf(buffer + offset, max_len - offset,
                      "- Running: %s\n", iot_running ? "Yes" : "No");
    
#if IOT_MQTT_ENABLED
    offset += snprintf(buffer + offset, max_len - offset,
                      "- MQTT: %s\n", mqtt_client_is_connected() ? "Connected" : "Disconnected");
#endif
    
#if IOT_SD_ENABLED
    offset += snprintf(buffer + offset, max_len - offset,
                      "- SD Card: %s\n", sd_storage_is_mounted() ? "Mounted" : "Not mounted");
#endif
    
#if IOT_MESH_ENABLED
    offset += snprintf(buffer + offset, max_len - offset,
                      "- Mesh Peers: %d\n", mesh_get_peer_count());
#endif
    
    return ESP_OK;
}

