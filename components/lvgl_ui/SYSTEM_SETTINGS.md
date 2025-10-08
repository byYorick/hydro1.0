# Системные настройки — Документация

## Обзор

Кнопка **SET** в строке состояния открывает главное меню системных настроек с доступом ко всем основным параметрам системы.

## Структура меню

```
Main Screen
    └── [SET Button] → System Settings
            ├── Auto Control → Включение/выключение автоматического управления
            ├── WiFi Settings → Настройки WiFi (AP mode, SSID, IP)
            ├── Display Settings → Настройка яркости дисплея
            ├── Data Logger → Управление логами (сохранение, очистка)
            ├── System Info → Информация о системе (версия, память, uptime)
            └── Reset to Defaults → Сброс к заводским настройкам
```

## Описание экранов

### 1. Auto Control

**Назначение:** Включение/выключение автоматической коррекции pH и EC

**Элементы:**
- Переключатель Auto Control (ON/OFF)
- Описание функции
- Кнопка возврата

**Функции:**
```c
// Загрузка состояния из конфигурации
system_config_t config;
config_load(&config);
bool auto_enabled = config.auto_control_enabled;

// Изменение состояния
config.auto_control_enabled = true;  // или false
config_save(&config);
```

**Что происходит при включении:**
- pH контроллер автоматически корректирует pH при отклонении
- EC контроллер автоматически добавляет питательные растворы
- Насосы запускаются согласно настроенным интервалам

---

### 2. WiFi Settings

**Назначение:** Просмотр и настройка WiFi подключения

**Информация на экране:**
- Режим работы (Access Point / Station / Hybrid)
- SSID сети
- IP адрес устройства
- Кнопка сканирования сетей (заглушка)

**Текущая конфигурация:**
- **Mode:** Access Point
- **SSID:** HydroMonitor
- **IP:** 192.168.4.1
- **Password:** (устанавливается в коде)

**Будущие возможности:**
- Сканирование доступных сетей
- Подключение к существующей WiFi сети
- Изменение SSID и пароля AP
- Переключение режимов (STA/AP/Hybrid)

---

### 3. Display Settings

**Назначение:** Настройка параметров дисплея

**Элементы:**
- Слайдер яркости (0-100%)
- Текущее значение яркости
- Рекомендации по использованию

**Функции:**
```c
// Установка яркости
lcd_ili9341_set_brightness(80);  // 80%
```

**Рекомендуемые значения:**
- **60-80%** — для использования в помещении
- **40-60%** — для приглушенного освещения
- **80-100%** — для яркого света / на улице

**Примечание:** Яркость изменяется мгновенно при перемещении слайдера

---

### 4. Data Logger

**Назначение:** Управление системой логирования данных

**Статистика:**
- Общее количество записей в логе
- Информация о хранении (NVS flash)

**Действия:**
- **Save to NVS** — сохранить текущие логи в энергонезависимую память
- **Clear All Logs** — очистить все записи лога

**Функции:**
```c
// Получение количества записей
uint32_t count = data_logger_get_count();

// Сохранение в NVS
data_logger_save_to_nvs();

// Очистка всех логов
data_logger_clear();
```

**Предупреждение:** Очистка логов необратима!

**Автоматическое сохранение:**
- Логи автоматически сохраняются каждые 5 минут
- При критических событиях сохранение происходит немедленно

---

### 5. System Info

**Назначение:** Отображение системной информации

**Отображаемые данные:**

#### Основная информация
- **Version:** Версия прошивки (v1.0)
- **Chip:** Модель микроконтроллера (ESP32-S3)
- **Cores:** Количество ядер процессора (2 — Dual Core)

#### Память
- **Free Heap:** Свободная heap память в KB
- **Min Free:** Минимальная свободная heap за все время работы

#### Время работы
- **Uptime:** Время непрерывной работы системы (часы и минуты)

**Пример отображения:**
```
Version: v1.0
Chip: ESP32-S3
Cores: 2 (Dual Core)

Free Heap: 245 KB
Min Free: 198 KB

Uptime: 5h 23m
```

**Использование для диагностики:**
- Если Free Heap < 100 KB — возможны проблемы с памятью
- Min Free показывает худший случай использования памяти
- Uptime полезен для отслеживания стабильности системы

---

### 6. Reset to Defaults

**Назначение:** Сброс всех настроек к заводским значениям

**Экран подтверждения:**
- ⚠️ Иконка предупреждения
- Заголовок "Reset to Defaults?"
- Описание последствий
- Две кнопки: **No** и **Yes**

**Что будет сброшено:**
- Все целевые значения датчиков (pH, EC, Temperature, etc.)
- Все пороги тревог
- Настройки насосов
- Auto Control режим
- Настройки WiFi (если сохранены)
- Калибровки датчиков (если сохранены)

**Процесс сброса:**
1. Пользователь выбирает "Reset to Defaults" в меню
2. Появляется экран подтверждения
3. При нажатии **Yes**:
   - Вызывается `config_manager_reset_to_defaults()`
   - Конфигурация сбрасывается к значениям по умолчанию
   - Система перезагружается через 1 секунду
4. При нажатии **No** — возврат в меню настроек

**Функция:**
```c
system_config_t config;
config_manager_reset_to_defaults(&config);
// После этого вызывается esp_restart()
```

**Предупреждение:** 
- Действие необратимо!
- После сброса система перезагрузится
- Все пользовательские настройки будут потеряны
- Потребуется повторная калибровка датчиков

---

## Навигация

### Энкодер
- **Вращение** — перемещение между пунктами меню
- **Нажатие** — выбор пункта / активация переключателя
- **В слайдерах** — изменение значения вращением энкодера

### Кнопка Back
- Из подменю → возврат в System Settings
- Из System Settings → возврат на главный экран

### Порядок фокуса

#### System Settings (главное меню)
1. Auto Control
2. WiFi Settings
3. Display Settings
4. Data Logger
5. System Info
6. Reset to Defaults
7. < Back

#### Display Settings
1. Brightness Slider
2. < Back

#### Data Logger
1. Save to NVS
2. Clear All Logs
3. < Back

#### Reset Confirmation
1. No
2. Yes

---

## Цветовая схема

- **Кнопки действий** — бирюзовые (`COLOR_ACCENT`)
- **Кнопка назад** — серая (`style_button_secondary`)
- **Опасные действия** — красные (`COLOR_DANGER`)
  - Clear All Logs
  - Yes (в подтверждении сброса)
- **Переключатели** — бирюзовые когда включены

---

## Реализация

### Добавление нового пункта меню

```c
// 1. Добавить экран в enum
typedef enum {
    // ...
    SCREEN_MY_NEW_SETTING,
    // ...
} screen_type_t;

// 2. Создать глобальные переменные
static lv_obj_t *my_new_screen = NULL;
static lv_group_t *my_new_group = NULL;

// 3. Создать функцию экрана
static void create_my_new_screen(void) {
    // Создание UI элементов
}

// 4. Добавить в system_menu_item_event_cb
case N:  // Номер пункта
    if (my_new_screen == NULL) {
        create_my_new_screen();
    }
    show_screen(SCREEN_MY_NEW_SETTING);
    break;

// 5. Добавить в show_screen()
case SCREEN_MY_NEW_SETTING:
    target_screen_obj = my_new_screen;
    target_group = my_new_group;
    break;

// 6. Добавить в back_button_event_cb
case SCREEN_MY_NEW_SETTING:
    show_screen(SCREEN_SYSTEM_STATUS);
    break;
```

---

## Интеграция с config_manager

Все настройки сохраняются через `config_manager`:

```c
#include "config_manager.h"

// Загрузка
system_config_t config;
config_load(&config);

// Изменение
config.auto_control_enabled = true;
config.sensor_config[SENSOR_INDEX_PH].target_value = 6.5;

// Сохранение
config_save(&config);
```

---

## Будущие улучшения

### Auto Control
- [ ] Индивидуальные переключатели для pH и EC
- [ ] Настройка интервалов коррекции
- [ ] График истории коррекций

### WiFi Settings
- [ ] Реальное сканирование сетей
- [ ] Форма ввода SSID/Password
- [ ] Переключение режимов
- [ ] Отображение силы сигнала

### Display Settings
- [ ] Настройка таймаута подсветки
- [ ] Выбор цветовой схемы
- [ ] Настройка размера шрифтов

### Data Logger
- [ ] Просмотр последних записей
- [ ] Фильтрация по типу событий
- [ ] Экспорт логов в файл
- [ ] Настройка автоматической очистки

### System Info
- [ ] Информация о датчиках (подключены/отключены)
- [ ] Статус насосов
- [ ] Температура чипа
- [ ] История перезагрузок

---

## API функции

### Управление экранами

```c
// Открыть системные настройки
show_screen(SCREEN_SYSTEM_STATUS);

// Открыть конкретный раздел
show_screen(SCREEN_AUTO_CONTROL);
show_screen(SCREEN_WIFI_SETTINGS);
show_screen(SCREEN_DISPLAY_SETTINGS);
show_screen(SCREEN_DATA_LOGGER_SETTINGS);
show_screen(SCREEN_SYSTEM_INFO);
show_screen(SCREEN_RESET_CONFIRM);
```

### Проверка текущего экрана

```c
if (current_screen == SCREEN_SYSTEM_STATUS) {
    // Код для главного меню настроек
}

if (current_screen >= SCREEN_AUTO_CONTROL && 
    current_screen <= SCREEN_RESET_CONFIRM) {
    // Код для любого подменю настроек
}
```

---

## Примеры использования

### Программное изменение настроек

```c
// Включение Auto Control программно
system_config_t config;
config_load(&config);
config.auto_control_enabled = true;
config_save(&config);

// Установка яркости дисплея
lcd_ili9341_set_brightness(70);

// Очистка логов
data_logger_clear();

// Сброс к defaults
system_config_t new_config;
config_manager_reset_to_defaults(&new_config);
```

### Обработка событий из настроек

```c
// Callback для изменения Auto Control
static void auto_control_changed(bool enabled) {
    if (enabled) {
        ph_ec_controller_set_auto_mode(true, true);
        ESP_LOGI(TAG, "Auto control activated");
    } else {
        ph_ec_controller_set_auto_mode(false, false);
        ESP_LOGI(TAG, "Auto control deactivated");
    }
}
```

---

## Отладка

### Логирование

Все действия в системных настройках логируются:

```
I (12345) LVGL_MAIN: System settings button clicked
I (12346) LVGL_MAIN: System menu item 0 clicked
I (12347) LVGL_MAIN: Auto control screen created
I (12500) LVGL_MAIN: Auto control enabled
```

### Проверка памяти

При создании каждого экрана проверяется доступная память:

```c
uint32_t free_heap = esp_get_free_heap_size();
ESP_LOGI(TAG, "Free heap before screen creation: %lu KB", free_heap / 1024);
```

---

## Технические детали

### Группы навигации

Каждый экран имеет свою группу для энкодера:

- `system_settings_group` — главное меню
- `auto_control_group` — Auto Control
- `wifi_settings_group` — WiFi Settings
- `display_settings_group` — Display Settings
- `data_logger_group` — Data Logger
- `system_info_group` — System Info
- `reset_confirm_group` — Reset Confirmation

### Ленивая инициализация

Экраны создаются только при первом открытии (lazy initialization):

```c
if (auto_control_screen == NULL) {
    create_auto_control_screen();
}
show_screen(SCREEN_AUTO_CONTROL);
```

Это экономит память и ускоряет запуск системы.

---

## Связанные компоненты

- **config_manager** — загрузка/сохранение конфигурации
- **data_logger** — управление логами
- **lcd_ili9341** — управление дисплеем
- **ph_ec_controller** — автоматическое управление

---

**Дата создания:** 2025-10-08  
**Версия:** 1.0

