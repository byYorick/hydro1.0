# 🌱 Hydroponics Monitor System v3.0

<div align="center">

**Профессиональная система мониторинга и управления гидропонной установкой на базе ESP32-S3**

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0-blue)](https://github.com/espressif/esp-idf)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/version-3.0.0-orange)](CHANGELOG.md)

[Функции](#-основные-функции) •
[Подключение](#-схема-подключения) •
[Установка](#-установка) •
[Использование](#-использование) •
[Архитектура](#-архитектура-системы)

</div>

---

## 📋 О проекте

Комплексная система автоматического управления гидропонной установкой с:
- 📊 Мониторингом 6 параметров (pH, EC, температура, влажность, освещенность, CO2)
- 🔧 Автоматической коррекцией pH и EC
- 💧 Управлением 6 перистальтическими насосами
- 🖥️ Цветным TFT дисплеем 240x320
- 🎛️ Rotary encoder для управления
- 📈 Логированием данных и историей
- ⚠️ Системой уведомлений и алертов
- ⏰ Планировщиком задач

## 🎯 Основные функции

### Мониторинг

- **pH**: 0-14, точность ±0.1
- **EC**: 0-5 mS/cm, точность ±0.1
- **Температура**: -40°C до +85°C
- **Влажность**: 0-100%
- **Освещенность**: 0-10000 lux
- **CO2**: 400-5000 ppm

### Автоматическое управление

- ✅ Коррекция pH (pH UP/DOWN)
- ✅ Коррекция EC (3 раствора: A, B, C)
- ✅ Подача воды
- ✅ Управление освещением
- ✅ Управление вентиляцией
- ✅ Управление температурой

### Дополнительные возможности

- 📊 Real-time графики
- 💾 Сохранение истории данных
- 📋 Экспорт в CSV/JSON
- ⚙️ Гибкая настройка параметров
- 🔔 Уведомления о критических событиях
- ⏲️ Планировщик задач

## 🔌 Схема подключения

### Общая схема системы

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ESP32-S3 DevKit                               │
│                                                                       │
│  [3.3V] [GND] [GPIO9-48]                                            │
└───┬─────┬──────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬──────┘
    │     │      │    │    │    │    │    │    │    │    │    │
    │     │      │    │    │    │    │    │    │    │    │    │
    │     │      │    │    │    │    │    │    │    │    │    │
┌───▼─────▼──┐ ┌─▼────▼────▼┐ ┌─▼────▼┐ ┌─▼────▼┐ ┌─▼────▼┐ ┌─▼───┐
│  LCD ILI9341│ │I2C Sensors│ │Encoder│ │ Pumps │ │ Relays│ │Power│
│   240x320   │ │   (x5)    │ │       │ │  (x6) │ │  (x4) │ │12V+ │
└─────────────┘ └───────────┘ └───────┘ └───────┘ └───────┘ └─────┘
```

### Подробная таблица подключений

| Компонент | ESP32-S3 GPIO | Описание |
|-----------|---------------|----------|
| **LCD ILI9341** |||
| SCK | GPIO 12 | SPI Clock |
| MOSI | GPIO 11 | SPI Data Out |
| DC | GPIO 9 | Data/Command |
| CS | GPIO 10 | Chip Select |
| RST | GPIO 14 | Reset |
| BLK | GPIO 15 | Backlight |
| **I2C Датчики** |||
| SCL | GPIO 17 | I2C Clock |
| SDA | GPIO 18 | I2C Data |
| **Encoder** |||
| CLK (A) | GPIO 1 | Encoder A |
| DT (B) | GPIO 2 | Encoder B |
| SW | GPIO 3 | Button |
| **Насос pH UP** |||
| IA | GPIO 4 | Motor A |
| IB | GPIO 5 | Motor B |
| **Насос pH DOWN** |||
| IA | GPIO 6 | Motor A |
| IB | GPIO 7 | Motor B |
| **Насос EC A** |||
| IA | GPIO 8 | Motor A |
| IB | GPIO 13 | Motor B |
| **Насос EC B** |||
| IA | GPIO 16 | Motor A |
| IB | GPIO 21 | Motor B |
| **Насос EC C** |||
| IA | GPIO 47 | Motor A |
| IB | GPIO 48 | Motor B |
| **Насос WATER** |||
| IA | GPIO 45 | Motor A |
| IB | GPIO 46 | Motor B |
| **Реле** |||
| Реле 1 (Свет) | GPIO 19 | Light Control |
| Реле 2 (Вент) | GPIO 20 | Fan Control |
| Реле 3 (Тепло) | GPIO 26 | Heater Control |
| Реле 4 (Резерв) | GPIO 27 | Reserve |

### I2C Датчики - адреса

| Датчик | I2C Адрес | Параметры |
|--------|-----------|-----------|
| SHT3x | 0x44 | Температура + Влажность |
| CCS811 | 0x5A | CO2 + VOC |
| Trema pH | 0x48 | pH 0-14 |
| Trema EC | 0x49 | EC 0-5 mS/cm |
| Trema Lux | 0x12 | Освещенность |

## 🚀 Установка

### Требования

- **Hardware:**
  - ESP32-S3 DevKit (минимум 4MB Flash, 2MB PSRAM)
  - ILI9341 TFT LCD 240x320
  - Датчики: SHT3x, CCS811, Trema pH, Trema EC, Trema Lux
  - 6x Перистальтические насосы с драйверами L298N
  - 4x Реле модуль
  - Rotary Encoder
  - Блок питания 12V/5A

- **Software:**
  - ESP-IDF v5.0 или выше
  - Python 3.7+
  - Git

### Шаг 1: Установка ESP-IDF

```bash
# Linux/Mac
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# Windows
cd C:\esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat esp32s3
export.bat
```

### Шаг 2: Клонирование проекта

```bash
git clone https://github.com/yourusername/hydroponics-monitor.git
cd hydroponics-monitor
```

### Шаг 3: Конфигурация

```bash
# Настройка проекта
idf.py set-target esp32s3
idf.py menuconfig

# Настройте:
# - Serial flasher config -> Flash size: 4MB
# - Partition Table -> Custom partition table CSV
# - Component config -> ESP32-specific -> Support for external SPI RAM
```

### Шаг 4: Сборка и прошивка

```bash
# Сборка
idf.py build

# Прошивка (замените /dev/ttyUSB0 на ваш порт)
idf.py -p /dev/ttyUSB0 flash

# Мониторинг (Ctrl+] для выхода)
idf.py -p /dev/ttyUSB0 monitor

# Все в одном
idf.py -p /dev/ttyUSB0 flash monitor
```

## 💻 Использование

### Первый запуск

1. После прошивки система автоматически инициализируется
2. На дисплее появится главный экран с карточками датчиков
3. Используйте энкодер для навигации:
   - **Поворот**: Переключение между датчиками
   - **Короткое нажатие**: Открыть детали датчика
   - **Длинное нажатие**: Открыть меню настроек

### Главный экран

```
┌──────────────────────────────────┐
│     Hydroponics Monitor v3.0     │
├──────────────────────────────────┤
│  ┌──────┐  ┌──────┐  ┌──────┐  │
│  │ pH   │  │ EC   │  │ Temp │  │
│  │ 6.8  │  │ 1.5  │  │ 24°C │  │
│  └──────┘  └──────┘  └──────┘  │
│  ┌──────┐  ┌──────┐  ┌──────┐  │
│  │ Hum  │  │ Lux  │  │ CO2  │  │
│  │ 65%  │  │ 500  │  │ 450  │  │
│  └──────┘  └──────┘  └──────┘  │
├──────────────────────────────────┤
│ Status: All systems operational  │
└──────────────────────────────────┘
```

### Экран детализации

```
┌──────────────────────────────────┐
│  pH Sensor Detail                │
├──────────────────────────────────┤
│  Current:     6.8                │
│  Target:      6.8                │
│  Status:      ✓ Normal           │
│  Range:       6.0 - 7.5          │
│                                   │
│  Last update: 12:34:56           │
│  Readings:    1,234              │
│                                   │
│  [Calibrate] [History] [Back]   │
└──────────────────────────────────┘
```

### Автоматическая коррекция

Система автоматически корректирует pH и EC при отклонении от целевых значений:

```
pH Correction Algorithm:
1. Measure current pH
2. Compare with target ± tolerance
3. Calculate required solution volume
4. Activate pump (pH UP or pH DOWN)
5. Wait 5 minutes for mixing
6. Re-measure and repeat if needed
```

## 🏗️ Архитектура системы

### Диаграмма компонентов

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                             │
│  ┌────────────┐  ┌───────────────┐  ┌──────────────┐          │
│  │ UI Manager │  │ pH/EC Control │  │  Scheduler   │          │
│  └────────────┘  └───────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────────────┐
│                    Service Layer                                 │
│  ┌──────────────┐  ┌───────────┐  ┌─────────────┐            │
│  │ Config Mgr   │  │ Data Log  │  │Notification │            │
│  └──────────────┘  └───────────┘  └─────────────┘            │
└─────────────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────────────┐
│                    Hardware Abstraction Layer                    │
│  ┌─────────┐  ┌──────────┐  ┌────────┐  ┌─────────┐          │
│  │ Sensors │  │   LCD    │  │ Pumps  │  │ Encoder │          │
│  └─────────┘  └──────────┘  └────────┘  └─────────┘          │
└─────────────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────────────┐
│                    FreeRTOS / ESP-IDF                            │
│  Tasks | Queues | Mutexes | I2C | SPI | GPIO | NVS            │
└─────────────────────────────────────────────────────────────────┘
```

### Потоки выполнения (FreeRTOS Tasks)

| Task | Priority | Stack | Interval | Назначение |
|------|----------|-------|----------|------------|
| pH/EC Control | 8 | 2KB | 500ms | Критическая коррекция параметров |
| Scheduler | 7 | 2KB | 1s | Выполнение запланированных задач |
| Display | 6 | 4KB | 1s | Обновление UI на дисплее |
| Sensor Reading | 5 | 4KB | 2s | Чтение всех датчиков |
| Notification | 4 | 2KB | 5s | Обработка уведомлений |
| Data Logger | 3 | 2KB | 60s | Логирование данных |

### Потокобезопасность

Система использует следующие механизмы синхронизации:

1. **Мьютексы (Mutexes)**:
   - `i2c_bus_mutex` - защита I2C шины
   - `ui_mutex` - защита LVGL операций
   - `config_mutex` - защита конфигурации
   - `sensor_data_mutex` - защита глобальных данных датчиков

2. **Очереди (Queues)**:
   - `sensor_data_queue` - передача данных от sensor task
   - `encoder_queue` - передача событий энкодера
   - `notification_queue` - передача уведомлений

3. **Семафоры (Semaphores)**:
   - `button_press_sem` - синхронизация нажатий кнопки
   - `button_release_sem` - синхронизация отпусканий кнопки

## 📊 Структура файлов проекта

```
hydroponics-monitor/
├── main/
│   ├── app_main.c              # Главный файл приложения
│   ├── system_config.h         # Централизованная конфигурация
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── config_manager/         # Управление конфигурацией (NVS)
│   ├── notification_system/    # Система уведомлений
│   ├── data_logger/            # Логирование данных
│   ├── task_scheduler/         # Планировщик задач
│   ├── ph_ec_controller/       # Контроллер pH/EC
│   ├── ui_manager/             # Менеджер UI
│   ├── lcd_ili9341/            # Драйвер LCD
│   ├── encoder/                # Драйвер энкодера
│   ├── sht3x/                  # Датчик температуры/влажности
│   ├── ccs811/                 # Датчик CO2
│   ├── trema_ph/               # Датчик pH
│   ├── trema_ec/               # Датчик EC
│   ├── trema_lux/              # Датчик освещенности
│   ├── peristaltic_pump/       # Драйвер насосов
│   ├── trema_relay/            # Драйвер реле
│   └── i2c_bus/                # Общая I2C шина
├── docs/
│   ├── ARCHITECTURE_ANALYSIS_AND_REFACTORING.md
│   ├── INTEGRATION_AND_FEATURES_REPORT.md
│   ├── TASK_SCHEDULER_AND_PH_EC_CONTROL_REPORT.md
│   └── schemas/                # Схемы подключения
├── README.md                   # Этот файл
├── CMakeLists.txt
├── partitions.csv              # Таблица разделов Flash
└── sdkconfig.defaults          # Конфигурация по умолчанию
```

## 🔧 Конфигурация

Все настройки находятся в `main/system_config.h`:

```c
// Целевые значения
#define PH_TARGET_DEFAULT     6.8f
#define EC_TARGET_DEFAULT     1.5f
#define TEMP_TARGET_DEFAULT   24.0f

// Пороги тревоги
#define PH_ALARM_LOW_DEFAULT  6.0f
#define PH_ALARM_HIGH_DEFAULT 7.5f

// Параметры насосов
#define PUMP_FLOW_RATE_DEFAULT 1.0f  // мл/сек
#define PH_MAX_CORRECTION_ML   50.0f // макс. объем
```

## 🐛 Отладка

### Логирование

```bash
# Уровни логирования в sdkconfig
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y

# Просмотр логов
idf.py monitor

# Фильтрация логов
idf.py monitor | grep "SENSOR"
```

### Общие проблемы

**Проблема**: Дисплей не включается
```
Solution: Проверьте подключение BLK (backlight) к GPIO15
```

**Проблема**: Датчики не обнаруживаются
```
Solution: 
1. Проверьте I2C подключение (SCL=17, SDA=18)
2. Используйте i2c_scanner для проверки адресов
3. Проверьте питание датчиков (3.3V)
```

**Проблема**: Насосы не работают
```
Solution:
1. Проверьте питание 12V для L298N
2. Проверьте подключение GPIO к IA/IB
3. Убедитесь, что GPIO не конфликтуют
```

## 📚 API Документация

### Sensor API

```c
// Чтение температуры и влажности
esp_err_t sht3x_read(float *temp, float *humidity);

// Чтение pH
esp_err_t trema_ph_read(float *ph);

// Чтение EC
esp_err_t trema_ec_read(float *ec);
```

### Pump Control API

```c
// Коррекция pH
esp_err_t ph_ec_controller_correct_ph(
    float current_ph, 
    float target_ph, 
    const char *reason
);

// Коррекция EC
esp_err_t ph_ec_controller_correct_ec(
    float current_ec, 
    float target_ec,
    uint8_t solution_type,  // 0=A, 1=B, 2=C
    const char *reason
);
```

### Scheduler API

```c
// Создание задачи коррекции pH
esp_err_t task_scheduler_create_ph_correction_task(
    float target_ph,
    float current_ph,
    uint32_t pump_duration_ms
);
```

## 🤝 Вклад в проект

Мы приветствуем вклад в проект! См. [CONTRIBUTING.md](CONTRIBUTING.md)

## 📄 Лицензия

Этот проект распространяется под лицензией MIT. См. [LICENSE](LICENSE)

## 👥 Авторы

- **Hydroponics Monitor Team** - *Разработка и поддержка*

## 🙏 Благодарности

- Espressif Systems за ESP-IDF
- LVGL Team за графическую библиотеку
- Сообщество Open Source

## 📞 Поддержка

- 📧 Email: support@hydroponics-monitor.com
- 💬 Discord: [Join our server](https://discord.gg/...)
- 🐛 Issues: [GitHub Issues](https://github.com/yourusername/hydroponics-monitor/issues)

---

<div align="center">

Made with ❤️ for hydroponics enthusiasts

[⬆ Back to top](#-hydroponics-monitor-system-v30)

</div>
