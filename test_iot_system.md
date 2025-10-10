# Тестирование IoT системы гидропоники

## Подготовка к тестированию

### 1. Настройка конфигурации

Отредактируйте `main/iot_config.h`:

```c
// MQTT Broker (локальный Mosquitto)
#define MQTT_BROKER_URI "mqtt://192.168.1.100:1883"
#define MQTT_CLIENT_ID "hydro_gateway_001"

// Telegram Bot
#define TELEGRAM_BOT_TOKEN "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
#define TELEGRAM_CHAT_ID "123456789"

// WiFi
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"

// Включение компонентов
#define IOT_MQTT_ENABLED true
#define IOT_TELEGRAM_ENABLED true
#define IOT_SD_ENABLED true  // Если есть SD-карта
#define IOT_MESH_ENABLED false  // Только для slave узлов
```

### 2. Установка MQTT брокера (Mosquitto)

На Raspberry Pi или домашнем сервере:

```bash
# Ubuntu/Debian/Raspberry Pi OS
sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients

# Запуск
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# Проверка
mosquitto_sub -h localhost -t "hydro/#" -v
```

### 3. Создание Telegram Bot

1. Найдите @BotFather в Telegram
2. Отправьте `/newbot`
3. Придумайте имя: `HydroMonitor Bot`
4. Придумайте username: `hydro_monitor_bot`
5. Скопируйте токен и вставьте в `TELEGRAM_BOT_TOKEN`

Получение Chat ID:
```bash
# Отправьте сообщение вашему боту в Telegram
# Затем выполните:
curl https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getUpdates
# Найдите "chat":{"id":123456789} и скопируйте ID
```

## Модульные тесты

### Тест 1: MQTT подключение

```bash
# Терминал 1: Запустите subscriber
mosquitto_sub -h 192.168.1.100 -t "hydro/hydro_gateway_001/#" -v

# Терминал 2: Прошейте ESP32
idf.py build flash monitor

# Ожидаемый результат:
# hydro/hydro_gateway_001/status {"timestamp":1234567890,"status":"online","device_id":"hydro_gateway_001"}
# hydro/hydro_gateway_001/sensors/ph {"timestamp":1234567890,"value":6.8,"unit":"pH","status":"ok"}
```

### Тест 2: Telegram уведомления

После запуска ESP32 вы должны получить в Telegram:
```
🚀 Система запущена

Гидропонная система готова к работе
```

В 20:00 ежедневно:
```
📊 Дневной отчет

Система работает: 5 часов
Все датчики в норме
Автоматика активна
```

### Тест 3: SD-карта логирование

Подключите SD-карту к ESP32:
- MOSI → GPIO 23
- MISO → GPIO 19
- SCK → GPIO 18
- CS → GPIO 5

После запуска проверьте структуру на SD:
```
/sdcard/
  /data/
    /sensors/
      all_20251009.csv
    /events/
      alarms_20251009.log
    /config/
```

### Тест 4: AI коррекция

В логах должны быть сообщения каждые 5 минут:
```
I (305000) SYS_TASKS: AI коррекция: pH UP 5.2 мл
I (305005) SYS_TASKS: AI коррекция: EC A=3.1 B=3.1 C=1.5 мл
```

### Тест 5: Mesh-сеть (при наличии slave узла)

Настройте второй ESP32 как slave:
```c
#define MESH_ROLE MESH_ROLE_SLAVE
#define MESH_DEVICE_ID 2
#define IOT_MESH_ENABLED true
#define IOT_MQTT_ENABLED false
```

В логах gateway должно появиться:
```
I (30000) MESH_NETWORK: Получено сообщение от устройства 2, тип 0
```

## Интеграционные тесты

### Полный цикл: Датчик → AI → Коррекция → MQTT → Telegram

1. **Изменение pH**:
   - Измените pH раствора (добавьте кислоту/щелочь)
   - Наблюдайте изменение в логах

2. **AI реакция**:
   - Через 5 минут AI должен определить необходимость коррекции
   - В логах: `AI коррекция: pH DOWN 8.5 мл`

3. **MQTT публикация**:
   - Проверьте MQTT Explorer: топик `hydro/hydro_gateway_001/sensors/ph`
   - Значение обновляется каждые 5 секунд

4. **Telegram аларм**:
   - Если pH выйдет за пределы (< 5.5 или > 7.5)
   - Вы получите сообщение: `🔴 КРИТИЧНО: ph_critical - pH вышел за пределы!`

5. **SD лог**:
   - Каждую минуту записывается строка в `all_20251009.csv`
   - Формат: `timestamp,ph,ec,temperature,humidity,lux,co2`

### Offline-режим тест

1. Отключите WiFi роутер
2. ESP32 должен продолжать работу
3. Данные сохраняются на SD-карту
4. При восстановлении WiFi - автоматическая синхронизация

## Проверка производительности

### Heap Memory

```
I (60000) app_main: System running. Free heap: 234567 / 512000 bytes
```

Нормальное потребление: ~200KB свободной памяти из 512KB

### CPU Usage

Используйте команду `tasks` в мониторе:
```
idf.py monitor
# В консоли нажмите Ctrl+] затем введите команду
```

Ожидаемая загрузка:
- sensor_task: ~5%
- mqtt_publish: ~2%
- ai_correct: ~3%
- Остальные задачи: <1%

## Отладка проблем

### MQTT не подключается

```
E (5000) MQTT_CLIENT: Ошибка подключения к брокеру
```

Решение:
- Проверьте IP брокера: `ping 192.168.1.100`
- Убедитесь что Mosquitto запущен: `sudo systemctl status mosquitto`
- Проверьте WiFi подключение ESP32

### Telegram не отправляет сообщения

```
E (10000) TELEGRAM_BOT: Ошибка отправки сообщения
```

Решение:
- Проверьте токен бота
- Убедитесь что вы отправили хотя бы одно сообщение боту
- Проверьте Chat ID

### SD-карта не монтируется

```
E (3000) SD_STORAGE: Ошибка монтирования SD-карты
```

Решение:
- Проверьте правильность подключения пинов
- Убедитесь что SD-карта отформатирована в FAT32
- Попробуйте установить `SD_FORMAT_IF_FAILED true`

### AI коррекция не работает

```
W (300000) AI_CONTROLLER: TensorFlow Lite модель не загружена, используется PID-алгоритм
```

Это нормально! TFLite модель пока не обучена, используется эвристический PID-алгоритм.

## Команды мониторинга

### MQTT Explorer

Подписка на все топики:
```
hydro/hydro_gateway_001/#
```

Отправка тестовой команды:
```
Topic: hydro/hydro_gateway_001/commands
Payload: {"command":"set_ph_target","payload":{"value":6.5}}
```

### Проверка SD-карты

```bash
# Извлеките SD-карту и вставьте в компьютер
# Откройте файлы:
cat /data/sensors/all_20251009.csv
cat /data/events/alarms_20251009.log
```

## Метрики успешности

✅ **Система работает корректно если:**

1. MQTT публикует данные каждые 5 секунд
2. SD-карта логирует каждую минуту
3. Telegram отправляет ежедневный отчет в 20:00
4. AI коррекция срабатывает при отклонениях pH/EC
5. Free heap > 150KB
6. Нет критических ошибок в логах

## Дополнительные возможности

### Node-RED Dashboard

Установите Node-RED на сервере с Mosquitto:
```bash
npm install -g --unsafe-perm node-red
node-red
# Откройте http://localhost:1880
```

Импортируйте flow для визуализации MQTT данных.

### Home Assistant интеграция

Добавьте в `configuration.yaml`:
```yaml
mqtt:
  sensor:
    - name: "Hydro pH"
      state_topic: "hydro/hydro_gateway_001/sensors/ph"
      value_template: "{{ value_json.value }}"
      unit_of_measurement: "pH"
```

## Следующие шаги

- [ ] Обучить TensorFlow Lite модель на реальных данных
- [ ] Добавить веб-dashboard на ESP32
- [ ] Реализовать OTA обновления
- [ ] Добавить multi-language поддержку
- [ ] Создать мобильное приложение (Flutter/React Native)

