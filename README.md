# 🌱 Гидропонный монитор на базе ESP32-S3

![Version](https://img.shields.io/badge/version-1.0-green.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)
![License](https://img.shields.io/badge/license-MIT-orange.svg)

Профессиональная система мониторинга и автоматического управления гидропонными установками с графическим интерфейсом, поддержкой мобильного приложения и расширенными возможностями.

---

## 📋 Содержание

- [Обзор](#обзор)
- [Основные возможности](#основные-возможности)
- [Архитектура системы](#архитектура-системы)
- [Аппаратные требования](#аппаратные-требования)
- [Схемы подключения](#схемы-подключения)
- [Установка и настройка](#установка-и-настройка)
- [Компоненты системы](#компоненты-системы)
- [API и использование](#api-и-использование)
- [Конфигурация](#конфигурация)
- [Калибровка датчиков](#калибровка-датчиков)
- [Мобильное приложение](#мобильное-приложение)
- [Устранение неполадок](#устранение-неполадок)
- [Разработка и отладка](#разработка-и-отладка)
- [Дорожная карта](#дорожная-карта)

---

## 🎯 Обзор

**Гидропонный монитор** — это комплексная система контроля параметров гидропонных установок на базе микроконтроллера ESP32-S3, обеспечивающая:

- 📊 **Мониторинг в реальном времени** — отслеживание 6 ключевых параметров
- 🎛️ **Автоматическое управление** — коррекция pH и EC с использованием перистальтических насосов
- 📱 **Мобильное приложение** — удаленное управление через WiFi/Bluetooth
- 📈 **Логирование данных** — сохранение истории измерений в NVS
- ⚙️ **Гибкая настройка** — калибровка датчиков, установка пороговых значений
- 🖥️ **Современный интерфейс** — графический UI на базе LVGL с поддержкой энкодера

---

## ✨ Основные возможности

### 🔬 Мониторинг датчиков

| Датчик | Параметр | Диапазон | Точность |
|--------|----------|----------|----------|
| **Trema pH** | Уровень pH | 0 - 14 pH | ±0.1 pH |
| **Trema EC** | Электропроводность | 0 - 20 mS/cm | ±2% |
| **SHT3x** | Температура | -40°C - 85°C | ±0.2°C |
| **SHT3x** | Влажность | 0 - 100% | ±2% RH |
| **Trema Lux** | Освещенность | 0 - 65535 lux | — |
| **CCS811** | CO₂ | 400 - 8192 ppm | ±10% |

### 🎯 Автоматический контроль

- **Коррекция pH** с использованием насосов pH+ и pH-
- **Коррекция EC** с дозированием питательных растворов A, B, C
- **Программируемые пороги** срабатывания для каждого параметра
- **Интервальное управление** с настраиваемыми задержками
- **Защита от переполнения** и аварийные остановки

### 📱 Интерфейс и управление

- **Цветной TFT дисплей** ILI9341 240×320 пикселей
- **Ротационный энкодер** для навигации по меню
- **Многоэкранный интерфейс** с детализацией по каждому датчику
- **Графики в реальном времени** (опционально)
- **Система уведомлений** с цветовой индикацией
- **Поддержка кириллицы** в интерфейсе

### 🌐 Сетевые возможности

- **WiFi подключение** (режимы STA/AP/Hybrid)
- **Bluetooth LE** для мобильного приложения
- **HTTP REST API** для интеграции
- **WebSocket** для данных в реальном времени
- **mDNS** для автоматического обнаружения
- **OTA обновления** прошивки по воздуху

### 💾 Хранение данных

- **NVS (Non-Volatile Storage)** для настроек и калибровки
- **Логирование событий** с временными метками
- **История измерений** с автоматической ротацией
- **Резервное копирование** конфигурации

---

## 🏗️ Архитектура системы

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 (Dual Core)                     │
│                                                               │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   Core 0    │  │   Core 1     │  │   PSRAM      │        │
│  │  (UI/LVGL)  │  │  (Sensors)   │  │   8 MB       │        │
│  └─────────────┘  └──────────────┘  └──────────────┘        │
│                                                               │
│  ┌───────────────────────────────────────────────────┐      │
│  │            FreeRTOS Task Scheduler                │      │
│  │  ┌────────┐ ┌─────────┐ ┌────────┐ ┌──────────┐  │      │
│  │  │Sensor  │ │Display  │ │pH/EC   │ │ Logger   │  │      │
│  │  │Task    │ │Task     │ │Control │ │Task      │  │      │
│  │  └────────┘ └─────────┘ └────────┘ └──────────┘  │      │
│  └───────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
        │                    │                    │
    ┌───▼────┐          ┌────▼────┐         ┌────▼────┐
    │  I2C   │          │   SPI   │         │  GPIO   │
    │ Sensors│          │ Display │         │  Pumps  │
    └────────┘          └─────────┘         └─────────┘
```

### Основные модули

1. **Sensor Layer** — драйверы датчиков с поддержкой fallback значений
2. **Controller Layer** — логика управления pH/EC с алгоритмами PID
3. **UI Layer** — графический интерфейс на базе LVGL 9.x
4. **Storage Layer** — управление конфигурацией и логами в NVS
5. **Network Layer** — WiFi/BLE интеграция и REST API
6. **Error Handler** — централизованная обработка ошибок

---

## 🛠️ Аппаратные требования

### Минимальная конфигурация

| Компонент | Спецификация | Примечания |
|-----------|--------------|------------|
| **Микроконтроллер** | ESP32-S3 (Dual Core) | 240 MHz, WiFi, BLE 5.0 |
| **Память** | 512 KB RAM + 8 MB PSRAM | PSRAM для графики LVGL |
| **Flash** | 4 MB минимум | Для прошивки и OTA |
| **Дисплей** | ILI9341 240×320 SPI | TFT LCD 16-bit color |
| **Энкодер** | Ротационный с кнопкой | Quadrature encoder |
| **Питание** | 5V / 2A минимум | Через USB или внешний БП |

### Датчики

- **Trema pH** — I2C адрес 0x4D
- **Trema EC** — I2C адрес 0x48
- **SHT3x** — I2C адрес 0x44 (температура/влажность)
- **Trema Lux** — I2C адрес 0x12 (освещенность)
- **CCS811** — I2C адрес 0x5A (CO₂/TVOC)

### Исполнительные устройства

- **6× Перистальтических насосов** (pH+, pH-, EC-A, EC-B, EC-C, Water)
- **4× Реле** (освещение, вентилятор, обогрев, резерв)
- **Trema Expander** — расширитель GPIO через I2C

---

## 🔌 Схемы подключения

### Подключение дисплея ILI9341

```
ESP32-S3                 ILI9341 LCD
─────────────────────────────────────
GND         ────────────► GND
3V3         ────────────► VCC
GPIO12      ────────────► SCK (SCLK)
GPIO11      ────────────► MOSI (SDI)
GPIO9       ────────────► DC (D/C)
GPIO14      ────────────► RST (RESET)
GPIO10      ────────────► CS (Chip Select)
GPIO15      ────────────► BLK (Backlight)
─────────────────────────────────────
```

**Примечание:** Подсветка управляется через GPIO15. Уровень яркости настраивается программно (0-100%).

### Подключение I2C шины

```
ESP32-S3                 I2C Устройства
─────────────────────────────────────
GPIO17 (SCL) ────────────► SCL всех датчиков
GPIO18 (SDA) ────────────► SDA всех датчиков
3V3          ────────────► VCC датчиков
GND          ────────────► GND датчиков
─────────────────────────────────────
```

**Частота I2C:** 100 kHz (стандартный режим)  
**Pull-up резисторы:** Встроенные (можно добавить внешние 4.7kΩ для стабильности)

### Подключение энкодера

```
ESP32-S3                 Encoder
─────────────────────────────────────
GPIO1       ────────────► CLK (A)
GPIO2       ────────────► DT (B)
GPIO3       ────────────► SW (Button)
GND         ────────────► GND
─────────────────────────────────────
```

### Подключение насосов

| Насос | Назначение | GPIO IA | GPIO IB |
|-------|------------|---------|---------|
| **Насос 1** | pH Up | 19 | 20 |
| **Насос 2** | pH Down | 21 | 47 |
| **Насос 3** | EC A | 38 | 39 |
| **Насос 4** | EC B | 40 | 41 |
| **Насос 5** | EC C | 26 | 27 |
| **Насос 6** | Water | 4 | 5 |

**Примечание:** Используются H-мосты для управления направлением вращения насосов.

---

## 🚀 Установка и настройка

### Предварительные требования

1. **ESP-IDF v5.0+** установлен и настроен
2. **Python 3.8+** с pip
3. **Git** для клонирования репозитория
4. **USB драйвер** для ESP32-S3

### Быстрый старт

```bash
# 1. Клонирование репозитория
git clone https://github.com/yourusername/hydro1.0.git
cd hydro1.0

# 2. Настройка окружения ESP-IDF
. $HOME/esp/esp-idf/export.sh  # Linux/Mac
# или
C:\Espressif\idf_cmd_init.bat  # Windows

# 3. Установка зависимостей
idf.py install

# 4. Конфигурация проекта (опционально)
idf.py menuconfig

# 5. Сборка проекта
idf.py build

# 6. Прошивка устройства
idf.py -p COM3 flash monitor  # Замените COM3 на ваш порт
```

### Конфигурация menuconfig

Ключевые настройки в `idf.py menuconfig`:

```
Component config → LVGL configuration
├── Color settings
│   ├── Color depth: 16 bits
│   └── Swap 16 bit color: YES
├── HAL Settings
│   ├── Default display refresh period: 30 ms
│   └── Default DPI: 130
└── Font usage
    └── Enable built-in fonts: Montserrat 14, 20
```

---

## 🧩 Компоненты системы

### 1. **Датчики и измерения**

#### CCS811 — Датчик CO₂ и TVOC
```c
#include "ccs811.h"

// Инициализация
bool success = ccs811_init();

// Чтение данных
float co2, tvoc;
if (ccs811_read_data(&co2, &tvoc)) {
    printf("CO2: %.0f ppm, TVOC: %.0f ppb\n", co2, tvoc);
}

// Компенсация температуры и влажности
ccs811_set_environmental_data(humidity, temperature);
```

**Особенности:**
- Требуется прогрев 20 минут после включения
- Режимы измерения: 1s, 10s, 60s, 250ms
- Автоматическое переключение на stub значения при отсутствии датчика

#### SHT3x — Температура и влажность
```c
#include "sht3x.h"

float temp, hum;
if (sht3x_read(&temp, &hum)) {
    printf("Температура: %.1f°C, Влажность: %.1f%%\n", temp, hum);
}
```

#### Trema pH — Измерение pH
```c
#include "trema_ph.h"

// Инициализация
trema_ph_init();

// Чтение значения
float ph_value;
if (trema_ph_read(&ph_value)) {
    printf("pH: %.2f\n", ph_value);
}

// Калибровка (3 точки)
trema_ph_calibrate(1, 4.0);   // Буфер pH 4.0
trema_ph_calibrate(2, 7.0);   // Буфер pH 7.0
trema_ph_calibrate(3, 10.0);  // Буфер pH 10.0
```

#### Trema EC — Электропроводность
```c
#include "trema_ec.h"

float ec_value;
if (trema_ec_read(&ec_value)) {
    printf("EC: %.2f mS/cm\n", ec_value);
}

// Калибровка
trema_ec_calibrate(1, 1413);  // Раствор 1413 μS/cm
```

#### Trema Lux — Освещенность
```c
#include "trema_lux.h"

float lux_value;
if (trema_lux_read_float(&lux_value)) {
    printf("Освещенность: %.0f lux\n", lux_value);
}
```

### 2. **Управление и контроль**

#### pH/EC контроллер
```c
#include "ph_ec_controller.h"

// Инициализация
ph_ec_controller_init();

// Настройка параметров pH
ph_control_params_t ph_params = {
    .target_ph = 6.5,
    .deadband = 0.2,
    .max_correction_step = 0.5,
    .correction_interval_ms = 60000  // 1 минута
};
ph_ec_controller_set_ph_params(&ph_params);

// Автоматическая коррекция
float current_ph = 7.2;
ph_ec_controller_correct_ph(current_ph);  // Автоматически запустит насос pH-
```

**Алгоритм коррекции:**
1. Проверка текущего значения и целевого
2. Расчет необходимой дозы
3. Проверка интервалов между коррекциями
4. Запуск соответствующего насоса
5. Логирование действия

#### Перистальтические насосы
```c
#include "peristaltic_pump.h"

// Инициализация пары насосов
pump_init(GPIO_IA, GPIO_IB);

// Запуск на определенное время
pump_run_ms(GPIO_IA, GPIO_IB, 5000);  // 5 секунд
```

#### Реле управления
```c
#include "trema_relay.h"

// Инициализация
trema_relay_init();

// Включение/выключение канала
trema_relay_digital_write(0, 1);  // Канал 0 ON
trema_relay_digital_write(0, 0);  // Канал 0 OFF

// Чтение состояния
uint8_t state = trema_relay_digital_read(0);

// Watchdog для автоматического отключения
trema_relay_enable_wdt(30);  // 30 секунд
trema_relay_reset_wdt();     // Сброс таймера
```

### 3. **Пользовательский интерфейс**

#### Инициализация LVGL UI
```c
#include "lcd_ili9341.h"
#include "lvgl_ui.h"

// Инициализация дисплея
lv_display_t *disp = lcd_ili9341_init();

// Инициализация пользовательского интерфейса
lvgl_main_init();

// Обновление значений датчиков
lvgl_update_sensor_values(ph, ec, temp, hum, lux, co2);
```

#### Навигация энкодером
```c
#include "encoder.h"

// Настройка пинов
encoder_set_pins(GPIO_CLK, GPIO_DT, GPIO_SW);

// Инициализация
encoder_init();

// Получение очереди событий
QueueHandle_t queue = encoder_get_event_queue();

// Обработка событий
encoder_event_t event;
if (xQueueReceive(queue, &event, pdMS_TO_TICKS(10))) {
    switch (event.type) {
        case ENCODER_EVENT_ROTATE_CW:
            // Поворот по часовой
            break;
        case ENCODER_EVENT_ROTATE_CCW:
            // Поворот против часовой
            break;
        case ENCODER_EVENT_BUTTON_PRESS:
            // Нажатие кнопки
            break;
    }
}
```

### 4. **Система обработки ошибок**

```c
#include "error_handler.h"

// Инициализация
error_handler_init(true);  // true = показывать всплывающие окна

// Установка русского шрифта
extern const lv_font_t montserrat14_ru;
error_handler_set_font(&montserrat14_ru);

// Примеры использования

// Простая ошибка датчика
ERROR_CHECK_SENSOR(err, "SHT3X", "Не удалось прочитать температуру");

// Ошибка I2C с деталями
ERROR_CHECK_I2C(err, "TREMA_PH", "Таймаут на адресе 0x%02X", 0x4D);

// Критическая ошибка
ERROR_CRITICAL(ERROR_CATEGORY_SYSTEM, ESP_ERR_NO_MEM, "MAIN",
              "Недостаточно памяти для инициализации UI");

// Предупреждение
ERROR_WARN(ERROR_CATEGORY_SENSOR, "CCS811",
          "Датчик требует прогрева (осталось %d мин)", remaining_min);
```

**Подробная документация:** См. [`components/error_handler/README.md`](components/error_handler/README.md)

### 5. **Конфигурация системы**

```c
#include "config_manager.h"

// Инициализация
config_manager_init();

// Загрузка конфигурации
system_config_t config;
config_load(&config);

// Изменение параметров
config.sensor_config[SENSOR_INDEX_PH].target_value = 6.5;
config.sensor_config[SENSOR_INDEX_PH].alarm_low = 5.5;
config.sensor_config[SENSOR_INDEX_PH].alarm_high = 7.5;

// Сохранение
config_save(&config);

// Сброс к значениям по умолчанию
config_manager_reset_to_defaults(&config);
```

### 6. **Логирование данных**

```c
#include "data_logger.h"

// Инициализация (максимум 1000 записей)
data_logger_init(1000);

// Логирование данных датчиков
data_logger_log_sensor_data(ph, ec, temp, hum, lux, co2);

// Логирование действия насоса
data_logger_log_pump_action(PUMP_INDEX_PH_UP, 5000, "Коррекция pH");

// Логирование тревоги
data_logger_log_alarm(LOG_LEVEL_WARNING, "pH вне диапазона");

// Логирование системного события
data_logger_log_system_event(LOG_LEVEL_INFO, "Система запущена");

// Сохранение в NVS
data_logger_save_to_nvs();

// Загрузка из NVS
data_logger_load_from_nvs();

// Автоматическая очистка старых записей (7 дней)
data_logger_set_auto_cleanup(true, 7);
```

### 7. **Система уведомлений**

```c
#include "notification_system.h"

// Инициализация
notification_system_init(50);  // Максимум 50 уведомлений

// Создание уведомления
uint32_t notif_id = notification_create(
    NOTIF_TYPE_WARNING,
    NOTIF_PRIORITY_HIGH,
    NOTIF_SOURCE_SENSOR,
    "pH выше нормы: 7.8"
);

// Подтверждение уведомления
notification_acknowledge(notif_id);

// Получение количества непрочитанных
uint32_t unread = notification_get_unread_count();

// Проверка критических уведомлений
if (notification_has_critical()) {
    // Обработка критической ситуации
}

// Очистка всех уведомлений
notification_clear_all();
```

### 8. **Сетевые возможности**

#### WiFi подключение
```c
#include "network_manager.h"

// Инициализация в режиме STA
network_manager_init(NETWORK_MODE_STA);

// Подключение к WiFi
network_wifi_config_t wifi_cfg = {
    .ssid = "YourSSID",
    .password = "YourPassword",
    .channel = 0,  // Автовыбор
    .auto_reconnect = true
};
network_manager_connect_wifi(&wifi_cfg);

// Проверка подключения
network_status_t status = network_manager_get_status();
if (status == NETWORK_STATUS_CONNECTED) {
    char ip[16];
    network_manager_get_ip(ip);
    printf("Подключено! IP: %s\n", ip);
}

// Запуск HTTP сервера
network_manager_start_http_server(80);

// Запуск mDNS
network_manager_start_mdns("hydromonitor", "http", 80);
```

#### Мобильное приложение
```c
#include "mobile_app_interface.h"

// Инициализация
mobile_app_interface_init(NETWORK_MODE_AP);

// Отправка данных датчиков
mobile_sensor_data_t sensor_data = {
    .temperature = 24.5,
    .humidity = 65.0,
    .ph = 6.5,
    .ec = 1.8,
    .lux = 1200,
    .co2 = 450,
    .timestamp = time(NULL)
};
mobile_app_send_sensor_data(&sensor_data);

// Проверка подключения
if (mobile_app_is_connected()) {
    printf("Мобильное приложение подключено\n");
}

// Отправка уведомления
mobile_app_send_notification("warning", "pH высокий", 3);
```

---

## ⚙️ Конфигурация

### Файл system_config.h

Основные константы и параметры по умолчанию:

```c
// Целевые значения датчиков
#define PH_TARGET_DEFAULT           6.5f
#define EC_TARGET_DEFAULT           1.8f
#define TEMP_TARGET_DEFAULT         24.0f
#define HUMIDITY_TARGET_DEFAULT     65.0f
#define LUX_TARGET_DEFAULT          1000.0f
#define CO2_TARGET_DEFAULT          600.0f

// Пороги срабатывания тревог
#define PH_ALARM_LOW_DEFAULT        5.5f
#define PH_ALARM_HIGH_DEFAULT       7.5f
#define EC_ALARM_LOW_DEFAULT        1.2f
#define EC_ALARM_HIGH_DEFAULT       2.4f

// Параметры насосов
#define PUMP_FLOW_RATE_DEFAULT      1.0f    // мл/сек
#define PUMP_MIN_DURATION_MS        100     // мс
#define PUMP_MAX_DURATION_MS        30000   // 30 секунд
#define PUMP_COOLDOWN_MS            60000   // 1 минута

// I2C конфигурация
#define I2C_MASTER_SCL_IO           17
#define I2C_MASTER_SDA_IO           18
#define I2C_MASTER_FREQ_HZ          100000  // 100 kHz
```

### Конфигурация в runtime

```c
// Загрузка конфигурации
system_config_t config;
config_load(&config);

// Изменение параметров pH
config.sensor_config[SENSOR_INDEX_PH].target_value = 6.5;
config.sensor_config[SENSOR_INDEX_PH].alarm_low = 5.8;
config.sensor_config[SENSOR_INDEX_PH].alarm_high = 7.2;
config.sensor_config[SENSOR_INDEX_PH].enabled = true;

// Настройка насоса
pump_config_t *pump = &config.pump_config[PUMP_INDEX_PH_UP];
pump->enabled = true;
pump->flow_rate_ml_per_sec = 1.0;
pump->min_duration_ms = 100;
pump->max_duration_ms = 10000;
pump->cooldown_ms = 60000;
pump->concentration_factor = 1.5;  // Концентрация раствора

// Сохранение
config_save(&config);
```

---

## 🔬 Калибровка датчиков

### Калибровка pH (3-точечная)

**Необходимые буферные растворы:** pH 4.0, pH 7.0, pH 10.0

```c
#include "trema_ph.h"

// 1. Погрузить электрод в буфер pH 4.0
trema_ph_calibrate(1, 4.0);
// Подождать стабилизации (30-60 сек)

// 2. Промыть электрод, погрузить в буфер pH 7.0
trema_ph_calibrate(2, 7.0);

// 3. Промыть электрод, погрузить в буфер pH 10.0
trema_ph_calibrate(3, 10.0);

// Проверка результата
if (trema_ph_get_calibration_result()) {
    printf("Калибровка успешна!\n");
    // Калибровка автоматически сохраняется в NVS
}
```

**Через UI:** Настройки pH → Калибровка → Следуйте инструкциям на экране

### Калибровка EC (1-точечная)

**Необходимый раствор:** 1413 μS/cm (стандартный калибровочный раствор)

```c
#include "trema_ec.h"

// Погрузить датчик в раствор 1413 μS/cm
trema_ec_calibrate(1, 1413);

// Проверка статуса
uint8_t status = trema_ec_get_calibration_status();
if (status == 100) {
    printf("EC калибровка завершена\n");
}
```

### Сохранение калибровки pH через UI
```c
#include "ph_screen.h"

// Сохранение параметров pH в NVS
ph_params_t params = {
    .current_value = 6.5,
    .target_value = 6.5,
    .notification_high = 7.0,
    .notification_low = 6.0,
    .pump_high = 7.2,
    .pump_low = 5.8,
    .cal_point1_ref = 4.0,
    .cal_point1_raw = 1.234,  // Измеренное значение
    .cal_point2_ref = 7.0,
    .cal_point2_raw = 2.567,
    .cal_point3_ref = 10.0,
    .cal_point3_raw = 3.890,
    .calibration_valid = true
};
ph_set_params(&params);
ph_save_to_nvs();
```

---

## 📱 Мобильное приложение

### REST API Endpoints

#### Получение данных датчиков
```
GET /api/sensors
Response: {
    "ph": 6.5,
    "ec": 1.8,
    "temperature": 24.5,
    "humidity": 65.0,
    "lux": 1200,
    "co2": 450,
    "timestamp": 1704715200
}
```

#### Получение статуса системы
```
GET /api/status
Response: {
    "system_uptime": 86400,
    "heap_free": 245760,
    "wifi_connected": true,
    "wifi_rssi": -45,
    "auto_control_enabled": true,
    "active_pumps": []
}
```

#### Обновление настроек
```
POST /api/settings
Body: {
    "ph_target": 6.5,
    "ec_target": 1.8,
    "auto_control": true
}
```

#### Управление насосами
```
POST /api/control
Body: {
    "command": "run_pump",
    "pump_id": 0,
    "duration_ms": 5000
}
```

### WebSocket для реального времени

```javascript
// Подключение к WebSocket
const ws = new WebSocket('ws://192.168.4.1:80/ws');

// Подписка на данные датчиков
ws.send(JSON.stringify({
    type: 'subscribe',
    events: ['sensors', 'alarms', 'pumps']
}));

// Получение данных
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    console.log('Sensor data:', data);
};
```

---

## 🐛 Устранение неполадок

### Дисплей

| Проблема | Возможная причина | Решение |
|----------|-------------------|---------|
| Экран не включается | Неправильное подключение питания | Проверьте VCC и GND |
| Белый/черный экран | Неверные настройки SPI | Проверьте пины SCK, MOSI, CS, DC, RST |
| Размытый текст | Неверные настройки LVGL | См. [WORKING_DISPLAY_SETTINGS.md](WORKING_DISPLAY_SETTINGS.md) |
| Инверсные цвета | Неверный порядок RGB/BGR | Установите `LCD_RGB_ELEMENT_ORDER_BGR` |
| Мерцание | Низкая частота обновления | Увеличьте `LCD_PIXEL_CLOCK_HZ` до 20 MHz |

### Датчики I2C

| Проблема | Код ошибки | Решение |
|----------|------------|---------|
| Датчик не отвечает | ESP_ERR_TIMEOUT (263) | Проверьте адрес, питание, подключение SDA/SCL |
| Ошибка CRC | ESP_ERR_INVALID_CRC (265) | Добавьте pull-up резисторы 4.7kΩ, уменьшите длину проводов |
| Устройство занято | NACK | Подождите или добавьте повторные попытки |
| Множественные ошибки | — | Проверьте питание датчиков (3.3V стабильное) |

**Диагностика I2C:**
```c
#include "i2c_bus.h"

// Сканирование шины I2C
for (uint8_t addr = 0x03; addr < 0x78; addr++) {
    uint8_t test_byte;
    esp_err_t err = i2c_bus_read(addr, &test_byte, 1);
    if (err == ESP_OK) {
        printf("Найдено устройство на адресе 0x%02X\n", addr);
    }
}
```

### Система

| Проблема | Решение |
|----------|---------|
| Watchdog reset | Добавьте `vTaskDelay()` в длительные циклы |
| Stack overflow | Увеличьте размер стека задачи в `xTaskCreate()` |
| Heap exhausted | Освободите неиспользуемую память, проверьте утечки |
| Guru Meditation | Проверьте backtrace: `addr2line -e build/hydroponics.elf ADDR` |

**Проверка памяти:**
```c
uint32_t free_heap = esp_get_free_heap_size();
uint32_t min_heap = esp_get_minimum_free_heap_size();
printf("Heap: %lu байт свободно (минимум: %lu)\n", free_heap, min_heap);
```

---

## 🔧 Разработка и отладка

### Логирование

Установите уровень логирования в `sdkconfig`:

```
CONFIG_LOG_DEFAULT_LEVEL=4  # DEBUG
```

Или через `menuconfig`:
```
Component config → Log output → Default log verbosity → Debug
```

### Мониторинг последовательного порта

```bash
idf.py -p COM3 monitor

# Фильтрация по тегу
idf.py -p COM3 monitor | grep "SHT3X"

# Сохранение логов в файл
idf.py -p COM3 monitor > logs.txt
```

### Отладка с GDB

```bash
# Запуск GDB сервера
openocd -f board/esp32s3-builtin.cfg

# В другом терминале
xtensa-esp32s3-elf-gdb build/hydroponics.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) continue
```

### Полезные команды idf.py

```bash
# Полная очистка и пересборка
idf.py fullclean
idf.py build

# Только прошивка (без сборки)
idf.py -p COM3 flash

# Стирание flash
idf.py -p COM3 erase-flash

# Размер прошивки
idf.py size-components

# Проверка конфигурации
idf.py menuconfig
```

---

## 📊 Структура проекта

```
hydro1.0/
├── main/
│   ├── app_main.c              # Точка входа приложения
│   ├── system_config.h         # Конфигурация системы
│   └── montserrat14_ru.c       # Русский шрифт для LVGL
│
├── components/
│   ├── ccs811/                 # Датчик CO₂
│   ├── config_manager/         # Управление конфигурацией
│   ├── data_logger/            # Логирование данных
│   ├── encoder/                # Драйвер энкодера
│   ├── error_handler/          # Обработка ошибок
│   │   ├── README.md           # Документация
│   │   ├── ERROR_CODES_RU.md   # Справочник кодов ошибок
│   │   └── ERROR_CHEATSHEET_RU.md  # Шпаргалка
│   ├── i2c_bus/                # I2C шина
│   ├── lcd_ili9341/            # Драйвер дисплея
│   ├── lvgl_ui/                # Пользовательский интерфейс
│   │   ├── lvgl_ui.c           # Главный UI
│   │   ├── ph_screen.c         # Экраны pH
│   │   ├── sensor_screens_optimized.c
│   │   └── ui_manager.c        # Менеджер UI
│   ├── mobile_app_interface/   # Интерфейс мобильного приложения
│   ├── network_manager/        # Управление сетью
│   ├── notification_system/    # Система уведомлений
│   ├── ota_update/             # OTA обновления
│   ├── peristaltic_pump/       # Управление насосами
│   ├── ph_ec_controller/       # Контроллер pH/EC
│   ├── sht3x/                  # Датчик температуры/влажности
│   ├── system_interfaces/      # Системные интерфейсы
│   ├── system_monitor/         # Мониторинг системы
│   ├── system_tasks/           # Системные задачи
│   ├── task_scheduler/         # Планировщик задач
│   ├── trema_ec/               # Датчик EC
│   ├── trema_expander/         # GPIO расширитель
│   ├── trema_lux/              # Датчик освещенности
│   ├── trema_ph/               # Датчик pH
│   └── trema_relay/            # Управление реле
│
├── managed_components/
│   ├── espressif__esp_lcd_ili9341/  # Драйвер ILI9341
│   └── lvgl__lvgl/             # LVGL библиотека v9.x
│
├── build/                      # Результаты сборки (генерируется)
├── CMakeLists.txt              # Основной файл сборки
├── sdkconfig                   # Конфигурация ESP-IDF
├── partitions.csv              # Таблица разделов flash
└── README.md                   # Этот файл
```

---

## 🎨 Пользовательский интерфейс

### Экраны

1. **Главный экран** — карточки всех 6 датчиков с текущими значениями
2. **Экран детализации** — подробная информация по выбранному датчику
3. **Экран настроек** — конфигурация порогов и параметров
4. **Экран калибровки** — пошаговая калибровка датчиков
5. **Системный статус** — информация о системе, памяти, сети

### Навигация

- **Вращение энкодера** — выбор элемента
- **Нажатие энкодера** — подтверждение/переход
- **Длительное нажатие** — возврат на главный экран (опционально)
- **Автоскрытие фокуса** — через 30 секунд бездействия

### Цветовая схема

- 🟢 **Зеленый** — параметр в норме
- 🟠 **Оранжевый** — предупреждение (близко к порогу)
- 🔴 **Красный** — тревога (за пределами допустимого диапазона)
- 🔵 **Бирюзовый** — акцентные элементы, фокус
- ⚫ **Темный фон** — для комфорта глаз

---

## 📡 Системные задачи

Система использует FreeRTOS с несколькими задачами:

| Задача | Приоритет | Стек | Частота | Описание |
|--------|-----------|------|---------|----------|
| **sensor_task** | 5 | 4096 | 5 сек | Опрос всех датчиков |
| **display_task** | 6 | 4096 | 200 мс | Обновление UI |
| **ph_ec_task** | 4 | 4096 | 10 сек | Контроль pH/EC |
| **data_logger_task** | 3 | 3072 | 60 сек | Логирование данных |
| **notification_task** | 4 | 3072 | 1 сек | Обработка уведомлений |
| **scheduler_task** | 2 | 2048 | 1 сек | Планировщик задач |
| **encoder_task** | 5 | 4096 | 20 мс | Обработка энкодера |
| **lvgl_task** | 2 | 20KB | 2-40 мс | Обработчик LVGL |

---

## 🔐 Безопасность

### Защита от критических ситуаций

- **Аварийная остановка насосов** при выходе параметров за критические пороги
- **Watchdog таймеры** для предотвращения зависаний
- **Проверка диапазонов** значений датчиков
- **Ограничение дозирования** (максимальное время работы насоса)
- **Cooldown периоды** между запусками насосов

### Обработка ошибок

Централизованная система обработки ошибок обеспечивает:
- Автоматическое логирование всех ошибок
- Всплывающие уведомления на экране
- Интеграция с системой уведомлений
- Статистика ошибок по категориям
- Детальные сообщения с рекомендациями

**Документация:** [`components/error_handler/`](components/error_handler/)

---

## 🌐 Сетевые режимы

### Режим точки доступа (AP)
```c
ap_config_t ap_cfg = {
    .ssid = "HydroMonitor",
    .password = "12345678",
    .channel = 1,
    .max_connection = 4,
    .ssid_hidden = false
};
network_manager_start_ap(&ap_cfg);
```
**IP адрес:** 192.168.4.1  
**Доступ:** http://192.168.4.1

### Режим станции (STA)
```c
network_wifi_config_t sta_cfg = {
    .ssid = "YourWiFi",
    .password = "YourPassword",
    .auto_reconnect = true
};
network_manager_connect_wifi(&sta_cfg);
```

### Гибридный режим
```c
network_manager_init(NETWORK_MODE_HYBRID);
// Одновременно STA и AP
```

---

## 📈 Производительность

### Требования к памяти

| Компонент | Heap | Stack | PSRAM |
|-----------|------|-------|-------|
| LVGL буферы | ~100 KB | — | 0-2 MB (опционально) |
| Sensor данные | ~10 KB | — | — |
| Логи (1000 записей) | ~130 KB | — | — |
| Network stack | ~50 KB | — | — |
| **Итого** | **~300 KB** | **~60 KB** | **0-2 MB** |

**Свободная heap после инициализации:** ~200 KB  
**Минимальная свободная heap в работе:** ~150 KB

### Оптимизация

Рекомендации для улучшения производительности:

1. **Включите PSRAM** для графических буферов LVGL
2. **Используйте двухъядерность** (Core 0 для UI, Core 1 для датчиков)
3. **Настройте частоты** задач согласно требованиям
4. **Отключите неиспользуемые датчики** в конфигурации
5. **Уменьшите глубину истории** в графиках

---

## 🔄 OTA обновления

### Подготовка обновления

```bash
# Сборка образа для OTA
idf.py build

# Образ находится в
build/hydroponics.bin
```

### OTA через HTTP

```c
#include "network_manager.h"

// Проверка обновлений
if (network_manager_check_ota_update("1.0.0")) {
    printf("Доступно обновление!\n");
}

// Запуск обновления
const char *ota_url = "http://192.168.1.100/firmware.bin";
esp_err_t err = network_manager_start_ota_update(ota_url);

if (err == ESP_OK) {
    // Обновление успешно, перезагрузка
    esp_restart();
}
```

### OTA через мобильное приложение

Мобильное приложение может инициировать OTA обновление через REST API:

```
POST /api/ota/start
Body: {
    "url": "http://server.com/firmware.bin"
}
```

---

## 📚 Документация компонентов

### Error Handler
- [Основная документация](components/error_handler/README.md)
- [Справочник кодов ошибок](components/error_handler/ERROR_CODES_RU.md)
- [Шпаргалка](components/error_handler/ERROR_CHEATSHEET_RU.md)
- [Индекс документации](components/error_handler/DOCS_INDEX.md)

### Настройки дисплея
- [Рабочие настройки ILI9341](WORKING_DISPLAY_SETTINGS.md)

---

## 🧪 Тестирование

### Тестирование I2C шины

```c
#include "test_i2c_bus.h"

void app_main(void) {
    test_i2c_bus();  // Автоматическое тестирование I2C операций
}
```

### Ручное тестирование датчиков

```c
// Тест SHT3x
float temp, hum;
if (sht3x_read(&temp, &hum)) {
    printf("✓ SHT3X OK: %.1f°C, %.1f%%\n", temp, hum);
} else {
    printf("✗ SHT3X FAIL\n");
}

// Тест Trema pH
float ph;
if (trema_ph_read(&ph)) {
    printf("✓ pH OK: %.2f\n", ph);
} else {
    printf("✗ pH FAIL\n");
}

// Аналогично для других датчиков
```

### Тестирование насосов

```c
// Короткий запуск каждого насоса (1 секунда)
for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
    printf("Тест насоса %d...\n", i);
    pump_run_ms(PUMP_PINS[i].ia, PUMP_PINS[i].ib, 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));  // Задержка между тестами
}
```

---

## 📦 Зависимости

### ESP-IDF компоненты

- `driver` — SPI, I2C, GPIO драйверы
- `nvs_flash` — энергонезависимое хранилище
- `esp_lcd` — LCD панели
- `esp_timer` — высокоточные таймеры
- `freertos` — операционная система реального времени
- `esp_http_server` — HTTP сервер
- `esp_wifi` — WiFi стек
- `bt` — Bluetooth стек (опционально)

### Внешние библиотеки

- **LVGL v9.x** — графическая библиотека UI
- **esp_lcd_ili9341** — драйвер дисплея ILI9341

Зависимости указаны в `main/idf_component.yml` и загружаются автоматически при сборке.

---

## 🎯 Примеры использования

### Пример 1: Базовая инициализация

```c
#include "app_main.h"

void app_main(void)
{
    // 1. Инициализация NVS
    nvs_flash_init();
    
    // 2. Инициализация I2C шины
i2c_bus_init();
    
    // 3. Инициализация датчиков
    sht3x_init();
    trema_ph_init();
    trema_ec_init();
trema_lux_init();
    ccs811_init();
    
    // 4. Инициализация дисплея
    lv_display_t *disp = lcd_ili9341_init();
    
    // 5. Инициализация энкодера
    encoder_set_pins(1, 2, 3);
    encoder_init();
    
    // 6. Инициализация UI
    lvgl_main_init();
    
    // 7. Инициализация систем
    error_handler_init(true);
    notification_system_init(50);
    data_logger_init(1000);
    config_manager_init();
    
    // 8. Инициализация контроллеров
    ph_ec_controller_init();
    
    // 9. Запуск системных задач
    system_task_handles_t handles;
    system_tasks_create_all(&handles);
    
    printf("Система инициализирована успешно!\n");
}
```

### Пример 2: Чтение всех датчиков

```c
void read_all_sensors_example(void)
{
    sensor_data_t data = {0};
    
    // Температура и влажность
    sht3x_read(&data.temperature, &data.humidity);
    
    // pH
    trema_ph_read(&data.ph);
    
    // EC
    trema_ec_read(&data.ec);
    
    // Освещенность
    trema_lux_read_float(&data.lux);
    
    // CO2
    ccs811_read_eco2(&data.co2);
    
    // Обновление UI
    lvgl_update_sensor_values(
        data.ph, data.ec, data.temperature,
        data.humidity, data.lux, data.co2
    );
    
    // Логирование
    data_logger_log_sensor_data(
        data.ph, data.ec, data.temperature,
        data.humidity, data.lux, data.co2
    );
}
```

### Пример 3: Автоматическая коррекция pH

```c
void auto_ph_control_example(void)
{
    // Настройка параметров
    ph_control_params_t params = {
        .target_ph = 6.5,
        .deadband = 0.2,              // Не корректировать если 6.3-6.7
        .max_correction_step = 0.5,   // Максимум за раз
        .correction_interval_ms = 300000  // 5 минут между коррекциями
    };
    ph_ec_controller_set_ph_params(&params);
    
    // Включение автоматического режима
    ph_ec_controller_set_auto_mode(true, false);  // pH auto, EC manual
    
    // В цикле опроса датчиков
    float current_ph;
    if (trema_ph_read(&current_ph)) {
        // Обновление значения в контроллере
        ph_ec_controller_update_values(current_ph, 0);
        
        // Контроллер автоматически запустит коррекцию если нужно
        ph_ec_controller_process();
    }
}
```

### Пример 4: Создание уведомления

```c
void create_alarm_example(float ph_value)
{
    if (ph_value > 7.5) {
        // Создание уведомления
        notification_create(
            NOTIF_TYPE_WARNING,
            NOTIF_PRIORITY_HIGH,
            NOTIF_SOURCE_SENSOR,
            "pH выше нормы: 7.8"
        );
        
        // Логирование тревоги
        data_logger_log_alarm(
            LOG_LEVEL_WARNING,
            "pH критически высокий"
        );
        
        // Отчет об ошибке
        ERROR_WARN(ERROR_CATEGORY_SENSOR, "TREMA_PH",
                  "pH = %.2f (норма: 5.5-7.0)", ph_value);
    }
}
```

---

## 🔧 Настройка параметров по умолчанию

### Целевые значения

Рекомендуемые значения для различных культур:

#### Салат и зелень
```c
ph_target = 6.0;
ec_target = 1.2;
temp_target = 20.0;
```

#### Томаты
```c
ph_target = 6.5;
ec_target = 2.5;
temp_target = 24.0;
```

#### Клубника
```c
ph_target = 6.0;
ec_target = 1.8;
temp_target = 22.0;
```

### Пороги тревог

```c
// Пример для pH
sensor_config[SENSOR_INDEX_PH].target_value = 6.5;
sensor_config[SENSOR_INDEX_PH].alarm_low = 5.5;    // Тревога
sensor_config[SENSOR_INDEX_PH].alarm_high = 7.5;   // Тревога

// Пример для EC
sensor_config[SENSOR_INDEX_EC].target_value = 1.8;
sensor_config[SENSOR_INDEX_EC].alarm_low = 1.0;
sensor_config[SENSOR_INDEX_EC].alarm_high = 2.5;
```

---

## 🧰 Дополнительные утилиты

### Планировщик задач

```c
#include "task_scheduler.h"

// Инициализация
task_scheduler_init();

// Добавление задачи
void my_periodic_task(void *arg) {
    printf("Выполнение периодической задачи\n");
}

task_scheduler_add_task(1, 600, my_periodic_task, NULL);  // Каждые 10 минут

// Запуск планировщика
task_scheduler_start();
```

### Системный монитор

```c
#include "system_monitor.h"

system_monitor_config_t mon_cfg = {
    .enable_cpu_monitoring = true,
    .enable_memory_monitoring = true,
    .enable_temperature_monitoring = true,
    .monitoring_interval_ms = 5000
};
system_monitor_init(&mon_cfg);

// Получение статистики
system_performance_stats_t stats;
system_monitor_get_stats(&stats);
printf("CPU Core 0: %.1f%%, Core 1: %.1f%%\n", 
       stats.cpu_usage_core0, stats.cpu_usage_core1);
printf("Heap свободно: %lu байт\n", stats.heap_free);
```

---

## 🌍 Интернационализация

### Поддержка кириллицы

Проект включает русский шрифт Montserrat с поддержкой кириллицы:

```c
// Объявление шрифта (main/montserrat14_ru.c)
LV_FONT_DECLARE(montserrat14_ru);

// Установка для error_handler
error_handler_set_font(&montserrat14_ru);

// Использование в UI
lv_obj_set_style_text_font(label, &montserrat14_ru, 0);
lv_label_set_text(label, "Температура");
```

**Генерация собственных шрифтов:**
1. Используйте [LVGL Font Converter](https://lvgl.io/tools/fontconverter)
2. Выберите диапазон Unicode для кириллицы: 0x0400-0x04FF
3. Формат: C array
4. Bpp: 4 (антиалиасинг)

---

## 📖 Справочная информация

### I2C адреса устройств

| Устройство | Адрес | Альтернативный |
|------------|-------|----------------|
| Trema pH | 0x4D | — |
| Trema EC | 0x48 | — |
| SHT3x | 0x44 | 0x45 |
| Trema Lux | 0x12 | — |
| CCS811 | 0x5A | 0x5B |
| Trema Expander | 0x09 | — |
| Trema Relay | 0x21 | — |

### GPIO распределение

| GPIO | Назначение | Примечание |
|------|------------|------------|
| 1 | Encoder CLK | Quadrature A |
| 2 | Encoder DT | Quadrature B |
| 3 | Encoder SW | Button (active low) |
| 4-5 | Pump 6 (Water) | IA/IB |
| 9 | LCD DC | Command/Data |
| 10 | LCD CS | Chip Select |
| 11 | LCD MOSI | SPI Data |
| 12 | LCD SCK | SPI Clock |
| 14 | LCD RST | Reset |
| 15 | LCD Backlight | PWM control |
| 17 | I2C SCL | All I2C devices |
| 18 | I2C SDA | All I2C devices |
| 19-20 | Pump 1 (pH Up) | IA/IB |
| 21, 47 | Pump 2 (pH Down) | IA/IB |
| 26-27 | Pump 5 (EC C) | IA/IB |
| 38-39 | Pump 3 (EC A) | IA/IB |
| 40-41 | Pump 4 (EC B) | IA/IB |

---

## 🚦 Индикация состояний

### Статусные индикаторы

- **🟢 Зеленая точка** — датчик в норме
- **🟠 Оранжевая точка** — приближается к порогу
- **🔴 Красная точка** — за пределами допустимого

### Всплывающие окна ошибок

- **🔵 Синий** — информация (3 сек)
- **🟠 Оранжевый** — предупреждение (3 сек)
- **🔴 Красный** — ошибка (5 сек)
- **🔴 Темно-красный** — критическая ошибка (10 сек)

---

## 🛡️ Лицензия

Проект распространяется под лицензией MIT. См. файл [LICENSE](LICENSE) для подробностей.

---

## 👥 Авторы и участники

- **Разработчик:** Hydro Team
- **Версия:** 1.0 (ветка ver0.2)
- **Дата:** 2025

---

## 🤝 Участие в разработке

Мы приветствуем вклад в развитие проекта!

### Как внести вклад

1. Fork репозитория
2. Создайте ветку для вашей функции (`git checkout -b feature/AmazingFeature`)
3. Commit изменений (`git commit -m 'Add some AmazingFeature'`)
4. Push в ветку (`git push origin feature/AmazingFeature`)
5. Откройте Pull Request

### Стандарты кода

- **Язык:** C11
- **Стиль:** K&R с 4 пробелами
- **Документация:** Doxygen комментарии
- **Commits:** Conventional Commits

---

## 🗺️ Дорожная карта

### ✅ Реализовано (v1.0)

- [x] Поддержка всех основных датчиков
- [x] Автоматическая коррекция pH/EC
- [x] Графический интерфейс на LVGL
- [x] Навигация энкодером
- [x] Система обработки ошибок
- [x] Логирование данных
- [x] Сохранение конфигурации в NVS
- [x] Базовая поддержка WiFi/BLE

### 🔄 В разработке (v1.1)

- [ ] Полная интеграция мобильного приложения
- [ ] Графики истории на главном экране
- [ ] WebSocket для реального времени
- [ ] Расширенная калибровка через UI
- [ ] Импорт/экспорт конфигурации

### 🔮 Планы (v2.0)

- [ ] Поддержка нескольких гидропонных систем
- [ ] Машинное обучение для предсказания
- [ ] Интеграция с облачными сервисами
- [ ] Голосовое управление
- [ ] Мультиязычный интерфейс

---

## 📞 Поддержка

### Сообщество

- **Issues:** [GitHub Issues](https://github.com/yourusername/hydro1.0/issues)
- **Discussions:** [GitHub Discussions](https://github.com/yourusername/hydro1.0/discussions)
- **Email:** support@hydromonitor.com

### Часто задаваемые вопросы

**Q: Можно ли использовать без некоторых датчиков?**  
A: Да, система автоматически использует stub значения для отсутствующих датчиков.

**Q: Как часто нужно калибровать pH электрод?**  
A: Рекомендуется калибровать раз в месяц или при отклонениях показаний.

**Q: Можно ли добавить больше насосов?**  
A: Да, через Trema Expander можно подключить до 8 дополнительных устройств.

**Q: Поддерживается ли работа без WiFi?**  
A: Да, система полностью функциональна в автономном режиме.

**Q: Как обновить прошивку?**  
A: Через USB (idf.py flash) или OTA по WiFi.

---

## 🙏 Благодарности

- **Espressif Systems** за ESP-IDF
- **LVGL Team** за отличную графическую библиотеку
- **iarduino** за библиотеки датчиков Trema
- **Сообщество ESP32** за поддержку и примеры

---

## 📸 Скриншоты

### Главный экран
![Main Screen](docs/screenshots/main_screen.png)

### Экран детализации pH
![pH Detail](docs/screenshots/ph_detail.png)

### Экран калибровки
![Calibration](docs/screenshots/calibration.png)

---

## 🔗 Полезные ссылки

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [Hydroponics Guide](https://www.maximumyield.com/definition/2255/hydroponics)
- [pH Control in Hydroponics](https://www.growweedeasy.com/ph)

---

<div align="center">

**Сделано с ❤️ для гидропонистов**

[⬆ Вернуться к началу](#-гидропонный-монитор-на-базе-esp32-s3)

</div>
