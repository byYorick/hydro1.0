# IoT Hydroponics - Быстрый старт

## 🚀 За 5 минут до запуска

### Шаг 1: Настройка конфигурации

Откройте `main/iot_config.h` и измените:

```c
// 1. WiFi настройки
#define WIFI_SSID "ВашаWiFiСеть"
#define WIFI_PASSWORD "ВашПароль"

// 2. MQTT брокер (IP вашего Raspberry Pi или сервера)
#define MQTT_BROKER_URI "mqtt://192.168.1.100:1883"

// 3. Telegram (получите токен у @BotFather)
#define TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID "YOUR_CHAT_ID_HERE"
```

### Шаг 2: Компиляция и прошивка

```bash
# В ESP-IDF терминале
idf.py build flash monitor
```

### Шаг 3: Проверка работы

В логах вы должны увидеть:

```
I (2345) IOT_INTEGRATION: === Инициализация IoT системы ===
I (2356) IOT_INTEGRATION: Устройство: HydroMonitor ESP32-S3
I (2367) IOT_INTEGRATION: Версия: 3.0.0-IoT
I (2378) IOT_INTEGRATION: 1. Инициализация Network Manager...
I (5234) IOT_INTEGRATION: 2. Инициализация SD Card...
I (5456) IOT_INTEGRATION: 3. Инициализация MQTT Client...
I (5678) MQTT_CLIENT: Подключено к MQTT брокеру
I (5789) IOT_INTEGRATION: 4. Инициализация Telegram Bot...
I (5890) TELEGRAM_BOT: Telegram Bot инициализирован
I (6001) IOT_INTEGRATION: === IoT система инициализирована ===
I (6012) IOT_INTEGRATION: === Запуск IoT сервисов ===
```

## ✅ Проверка функциональности

### MQTT

Откройте MQTT Explorer и подключитесь к `192.168.1.100:1883`

Вы должны видеть топики:
```
hydro/
  hydro_gateway_001/
    status        → {"status":"online",...}
    sensors/
      ph          → {"value":6.8,"unit":"pH",...}
      ec          → {"value":1.5,"unit":"mS/cm",...}
      temp        → {"value":24.5,"unit":"°C",...}
```

### Telegram

В Telegram бот должен отправить:
```
🚀 Система запущена

Гидропонная система готова к работе
```

### SD-карта

Если подключена SD-карта, на ней появится:
```
/sdcard/data/sensors/all_20251009.csv
```

Содержимое файла:
```csv
timestamp,ph,ec,temperature,humidity,lux,co2
1696867200,6.8,1.5,24.5,65.0,500,450
```

## 📊 Дашборд MQTT Dashboard

### Android/iOS приложение

1. Установите **MQTT Dashboard** (IoT OnOff) из Play Store/App Store
2. Добавьте подключение:
   - Broker: `192.168.1.100:1883`
   - Client ID: `mqtt_dashboard_mobile`

3. Создайте виджеты:

**pH индикатор:**
- Тип: Text
- Topic: `hydro/hydro_gateway_001/sensors/ph`
- JSONPath: `$.value`

**EC индикатор:**
- Тип: Text  
- Topic: `hydro/hydro_gateway_001/sensors/ec`
- JSONPath: `$.value`

**Температура:**
- Тип: Gauge
- Topic: `hydro/hydro_gateway_001/sensors/temp`
- JSONPath: `$.value`
- Min: 0, Max: 40

## 🎛️ Отправка команд

### Через MQTT

Отправьте команду на топик `hydro/hydro_gateway_001/commands`:

```json
{
  "command": "set_ph_target",
  "payload": {
    "value": 6.5
  }
}
```

Доступные команды:
- `set_ph_target` - установить целевой pH
- `set_ec_target` - установить целевой EC
- `start_pump` - запустить насос
- `stop_pump` - остановить насос
- `enable_auto` - включить автоматику
- `disable_auto` - выключить автоматику

### Через Telegram (если включено)

Отправьте боту:
```
/status
/set_ph 6.5
/calibrate
```

## 🔧 Отладка

### Включение подробных логов

В `sdkconfig` или через `idf.py menuconfig`:
```
Component config → Log output → Default log verbosity → Debug
```

### Мониторинг MQTT трафика

```bash
# Подписка на все топики
mosquitto_sub -h 192.168.1.100 -t "#" -v

# Только датчики
mosquitto_sub -h 192.168.1.100 -t "hydro/+/sensors/#" -v

# Только алармы
mosquitto_sub -h 192.168.1.100 -t "hydro/+/alarms" -v
```

### Проверка SD-карты

В мониторе ESP-IDF выполните:
```
I (60000) SD_STORAGE: Статистика SD:
  Total: 4096 MB
  Used: 15 MB
  Free: 4081 MB
  Sensor records: 1440
```

## 📈 Визуализация данных

### Grafana + InfluxDB

1. Установите InfluxDB и Telegraf на сервере
2. Настройте Telegraf для чтения MQTT:

```toml
[[inputs.mqtt_consumer]]
  servers = ["tcp://localhost:1883"]
  topics = ["hydro/+/sensors/#"]
  data_format = "json"
  json_string_fields = ["status"]
```

3. Создайте dashboard в Grafana с графиками pH, EC, температуры

## 🌐 Удаленный доступ

### Через VPN

1. Настройте WireGuard/OpenVPN на Raspberry Pi
2. Подключайтесь к домашней сети удаленно
3. Доступ к MQTT Dashboard и Node-RED

### Через CloudMQTT (облачный брокер)

Измените в `iot_config.h`:
```c
#define MQTT_BROKER_URI "mqtt://m12.cloudmqtt.com:12345"
#define MQTT_USERNAME "cloudmqtt_user"
#define MQTT_PASSWORD "cloudmqtt_password"
```

## 🎯 Что делает каждая задача

| Задача | Интервал | Функция |
|--------|----------|---------|
| `sensor_task` | 2 сек | Чтение всех датчиков |
| `mqtt_publish_task` | 5 сек | Публикация в MQTT |
| `sd_logging_task` | 1 мин | Запись на SD-карту |
| `telegram_task` | 1 мин | Проверка времени для отчетов |
| `ai_correction_task` | 5 мин | AI коррекция pH/EC |
| `mesh_heartbeat_task` | 30 сек | Heartbeat для mesh-сети |

## 💡 Полезные ссылки

- **MQTT Explorer**: https://mqtt-explorer.com/
- **MQTT Dashboard**: https://play.google.com/store/apps/details?id=com.thn.iotmqttdashboard
- **Node-RED**: https://nodered.org/
- **Home Assistant**: https://www.home-assistant.io/
- **Grafana**: https://grafana.com/

## 📞 Поддержка

При проблемах проверьте:
1. Логи ESP32: `idf.py monitor`
2. Логи Mosquitto: `sudo journalctl -u mosquitto -f`
3. Файл IOT_SYSTEM_README.md - полная документация
4. Файл test_iot_system.md - подробные тесты

---

**Готово! Система запущена и работает! 🎉**

