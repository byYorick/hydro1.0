# IoT Hydroponics System - Руководство

## Обзор системы

Полноценная IoT-система мониторинга и управления гидропоникой на базе ESP32-S3 с:
- ✅ MQTT клиент для локального брокера
- ✅ Telegram Bot для push-уведомлений
- ✅ SD-карта для локального хранения данных
- ✅ ESP-NOW mesh-сеть для распределенных датчиков
- ✅ Network Manager с WiFi поддержкой
- ⏸ TensorFlow Lite для AI-коррекции (запланировано)
- ⏸ Расширенный планировщик задач (запланировано)

## Архитектура

```
┌─────────────────────────────────────────────────┐
│            ESP32-S3 Gateway                      │
├─────────────────────────────────────────────────┤
│ ┌─────────────┐  ┌──────────────┐  ┌─────────┐ │
│ │   Sensors   │  │  SD Storage  │  │  LVGL   │ │
│ │  (pH, EC,   │  │  (CSV/JSON)  │  │   UI    │ │
│ │   Temp...)  │  └──────────────┘  └─────────┘ │
│ └─────────────┘                                 │
│                                                  │
│ ┌─────────────────────────────────────────────┐ │
│ │         IoT Integration Layer               │ │
│ ├──────────┬───────────┬────────────┬─────────┤ │
│ │  MQTT    │ Telegram  │  Mesh Net  │ Network │ │
│ │ Client   │    Bot    │  (ESP-NOW) │ Manager │ │
│ └──────────┴───────────┴────────────┴─────────┘ │
└──────────┬──────────────┬────────────┬──────────┘
           │              │            │
           v              v            v
    MQTT Broker    Telegram API   Slave Nodes
   (Mosquitto)                    (ESP32-S3)
```

## Созданные компоненты

### 1. MQTT Client (`components/mqtt_client/`)
Обеспечивает связь с локальным MQTT брокером.

**Топики:**
```
hydro/{device_id}/sensors/ph
hydro/{device_id}/sensors/ec
hydro/{device_id}/sensors/temp
hydro/{device_id}/sensors/humidity
hydro/{device_id}/sensors/lux
hydro/{device_id}/sensors/co2
hydro/{device_id}/commands
hydro/{device_id}/status
hydro/{device_id}/alarms
hydro/{device_id}/telemetry
```

**Основные функции:**
- `mqtt_client_init()` - инициализация
- `mqtt_publish_sensor_data()` - публикация всех датчиков
- `mqtt_publish_alarm()` - отправка аларма
- `mqtt_subscribe_commands()` - подписка на команды

### 2. Telegram Bot (`components/telegram_bot/`)
Push-уведомления и управление через Telegram.

**Функции:**
- `telegram_send_message()` - отправка текста
- `telegram_send_alarm()` - отправка аларма с emoji
- `telegram_send_status()` - статус системы
- `telegram_send_daily_report()` - ежедневный отчет

### 3. SD Storage (`components/sd_storage/`)
Локальное хранение данных на SD-карте.

**Структура каталогов:**
```
/sdcard/
  /data/
    /sensors/
      all_20251009.csv
      all_20251010.csv
    /events/
      alarms_20251009.log
    /config/
      settings.json
      calibration.json
```

**Функции:**
- `sd_write_sensor_log()` - запись данных датчиков
- `sd_write_event_log()` - запись события
- `sd_save_config()` / `sd_load_config()` - конфигурация
- `sd_sync_to_cloud()` - синхронизация с облаком

### 4. Mesh Network (`components/mesh_network/`)
ESP-NOW сеть для распределенных датчиков.

**Роли:**
- **Gateway** - главный узел с WiFi и MQTT
- **Slave** - подчиненные узлы с датчиками (без WiFi)

**Функции:**
- `mesh_send_sensor_data()` - отправка данных на gateway
- `mesh_broadcast_command()` - рассылка команды
- `mesh_register_peer()` - регистрация узла

### 5. IoT Integration (`main/iot_integration.c`)
Единый интерфейс для всех IoT компонентов.

**Функции:**
- `iot_system_init()` - инициализация всей системы
- `iot_system_start()` - запуск сервисов
- `iot_publish_sensor_data()` - публикация во все каналы
- `iot_publish_alarm()` - аларм во все каналы

## Конфигурация

Основной файл конфигурации: `main/iot_config.h`

### Настройки MQTT
```c
#define MQTT_BROKER_URI "mqtt://192.168.1.100:1883"
#define MQTT_CLIENT_ID "hydro_gateway_001"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
```

### Настройки Telegram
```c
#define TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID "YOUR_CHAT_ID_HERE"
```

### Настройки SD-карты
```c
#define SD_MODE SD_MODE_SPI
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18
#define SD_CS_PIN 5
```

### Включение/Отключение компонентов
```c
#define IOT_MQTT_ENABLED true
#define IOT_TELEGRAM_ENABLED true
#define IOT_SD_ENABLED true
#define IOT_MESH_ENABLED false
```

## Использование в app_main.c

```c
#include "iot_integration.h"

void app_main(void) {
    // Базовая инициализация ESP-IDF
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Инициализация IoT системы
    ESP_ERROR_CHECK(iot_system_init());
    ESP_ERROR_CHECK(iot_system_start());
    
    // Основной цикл
    while (1) {
        // Чтение датчиков (пример)
        float ph = 6.8;
        float ec = 1.5;
        float temp = 24.5;
        float humidity = 65.0;
        float lux = 500;
        uint16_t co2 = 450;
        
        // Публикация данных во все каналы (MQTT, SD, Telegram)
        iot_publish_sensor_data(ph, ec, temp, humidity, lux, co2);
        
        // Проверка алармов
        if (ph < 5.5 || ph > 7.5) {
            iot_publish_alarm("ph_critical", "pH вышел за пределы!", "critical");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## Последовательность инициализации

1. **NVS** - Non-Volatile Storage
2. **Network Stack** - esp_netif, event loop
3. **Network Manager** - WiFi подключение
4. **SD Card** - монтирование файловой системы
5. **MQTT Client** - подключение к брокеру
6. **Telegram Bot** - инициализация API
7. **Mesh Network** - инициализация ESP-NOW (если включено)
8. **Запуск сервисов** - старт всех задач

## MQTT Dashboard

Для визуализации данных можно использовать:
- **MQTT Explorer** - для отладки
- **Node-RED** - для создания dashboard
- **Grafana + InfluxDB** - для графиков
- **Home Assistant** - для умного дома

Пример подписки в MQTT Explorer:
```
hydro/hydro_gateway_001/#
```

## Telegram Bot - Создание

1. Найдите @BotFather в Telegram
2. Отправьте `/newbot`
3. Следуйте инструкциям
4. Скопируйте токен в `TELEGRAM_BOT_TOKEN`
5. Отправьте сообщение вашему боту
6. Получите chat_id через `https://api.telegram.org/bot<TOKEN>/getUpdates`
7. Скопируйте chat_id в `TELEGRAM_CHAT_ID`

## Mesh Network - Настройка

### Gateway узел:
```c
#define MESH_ROLE MESH_ROLE_GATEWAY
#define MESH_DEVICE_ID 1
#define IOT_MESH_ENABLED false  // Gateway не использует mesh
```

### Slave узел:
```c
#define MESH_ROLE MESH_ROLE_SLAVE
#define MESH_DEVICE_ID 2  // Уникальный для каждого slave
#define IOT_MESH_ENABLED true
#define IOT_MQTT_ENABLED false  // Slave не подключается к MQTT напрямую
```

## Структура данных на SD-карте

### Датчики (CSV):
```csv
timestamp,ph,ec,temperature,humidity,lux,co2
1696867200,6.8,1.5,24.5,65.0,500,450
```

### События (LOG):
```
[2025-10-09 14:30:25] [CRITICAL] ph_critical: pH вышел за пределы!
[2025-10-09 14:35:10] [WARNING] pump_timeout: Насос не отвечает
```

## Следующие шаги

1. **TensorFlow Lite Integration** - AI-коррекция pH/EC
2. **Enhanced Task Scheduler** - cron-like расписания
3. **Web Dashboard** - веб-интерфейс на ESP32
4. **OTA Updates** - обновление прошивки по воздуху
5. **Multi-language Support** - поддержка языков

## Требования

- ESP-IDF v5.0+
- ESP32-S3 с PSRAM
- SD-карта (опционально)
- MQTT брокер (Mosquitto)
- Telegram Bot Token
- WiFi сеть

## Автор

Hydroponics Monitor Team, 2025

## Лицензия

MIT License

