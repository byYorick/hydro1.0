/**
 * @file iot_config.h
 * @brief Конфигурация IoT гидропонной системы
 *
 * @author Hydroponics Monitor Team
 * @date 2025
 */

#ifndef IOT_CONFIG_H
#define IOT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MQTT Configuration
// ============================================================================

#define MQTT_BROKER_URI         "mqtt://192.168.1.100:1883"
#define MQTT_CLIENT_ID          "hydro_gateway_001"
#define MQTT_USERNAME           ""  // Оставить пустым если не требуется
#define MQTT_PASSWORD           ""  // Оставить пустым если не требуется
#define MQTT_KEEPALIVE          120 // секунды
#define MQTT_AUTO_RECONNECT     true

// ============================================================================
// Telegram Bot Configuration
// ============================================================================

#define TELEGRAM_BOT_TOKEN      "YOUR_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID        "YOUR_CHAT_ID_HERE"
#define TELEGRAM_POLL_INTERVAL  10000  // мс
#define TELEGRAM_ENABLE_COMMANDS true

// ============================================================================
// Mesh Network Configuration
// ============================================================================

#define MESH_ROLE               MESH_ROLE_GATEWAY  // MESH_ROLE_GATEWAY или MESH_ROLE_SLAVE
#define MESH_DEVICE_ID          1  // 1-254

// ============================================================================
// SD Card Configuration
// ============================================================================

#define SD_CARD_ENABLED         true
#define SD_MODE                 SD_MODE_SPI
#define SD_MOSI_PIN             23
#define SD_MISO_PIN             19
#define SD_SCK_PIN              18
#define SD_CS_PIN               5
#define SD_MAX_FREQUENCY        20000000  // 20 MHz
#define SD_FORMAT_IF_FAILED     false

// ============================================================================
// Network Configuration
// ============================================================================

#define WIFI_SSID               "YourWiFiSSID"
#define WIFI_PASSWORD           "YourWiFiPassword"
#define WIFI_AUTO_RECONNECT     true

#define AP_SSID                 "HydroMonitor-AP"
#define AP_PASSWORD             "12345678"
#define AP_CHANNEL              1
#define AP_MAX_CONNECTIONS      4

#define NETWORK_MODE            NETWORK_MODE_STA  // или NETWORK_MODE_AP, NETWORK_MODE_HYBRID

// ============================================================================
// IoT Features Enable/Disable
// ============================================================================

#define IOT_MQTT_ENABLED        true
#define IOT_TELEGRAM_ENABLED    true
#define IOT_SD_ENABLED          true
#define IOT_MESH_ENABLED        false  // Включить если это slave узел
#define IOT_AI_ENABLED          false  // TensorFlow Lite (пока не реализовано)

// ============================================================================
// Data Logging Configuration
// ============================================================================

#define LOG_SENSOR_INTERVAL     60000  // Интервал логирования датчиков (мс)
#define MQTT_PUBLISH_INTERVAL   5000   // Интервал публикации в MQTT (мс)
#define TELEGRAM_REPORT_HOUR    20     // Час отправки ежедневного отчета (0-23)
#define SD_CLEANUP_DAYS         30     // Хранить данные на SD N дней

// ============================================================================
// Device Information
// ============================================================================

#define DEVICE_NAME             "HydroMonitor ESP32-S3"
#define FIRMWARE_VERSION        "3.0.0-IoT"
#define HARDWARE_VERSION        "v3.0"

#ifdef __cplusplus
}
#endif

#endif // IOT_CONFIG_H

