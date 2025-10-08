# Changelog — Системные настройки

## Версия 1.1 — Системные настройки (2025-10-08)

### ✨ Новые возможности

#### 🎯 Кнопка SET в строке состояния
- Добавлена кнопка **SET** в правом верхнем углу главного экрана
- Кнопка добавлена в группу навигации энкодера
- При нажатии открывается главное меню системных настроек

#### 📋 Главное меню системных настроек (SCREEN_SYSTEM_STATUS)
Содержит 6 пунктов:
1. **Auto Control** — управление автоматикой
2. **WiFi Settings** — настройки сети
3. **Display Settings** — параметры дисплея
4. **Data Logger** — управление логами
5. **System Info** — информация о системе
6. **Reset to Defaults** — сброс настроек

#### 🎛️ Auto Control (SCREEN_AUTO_CONTROL)
- Переключатель для включения/выключения автоматического управления
- Описание функции автоматической коррекции
- Сохранение состояния в NVS через `config_manager`
- Интеграция с `system_config_t.auto_control_enabled`

**Функционал:**
```c
// Автоматическое сохранение при изменении переключателя
config.auto_control_enabled = true/false;
config_save(&config);
```

#### 📡 WiFi Settings (SCREEN_WIFI_SETTINGS)
- Отображение текущего режима (Access Point)
- Показ SSID (HydroMonitor)
- Показ IP адреса (192.168.4.1)
- Кнопка сканирования сетей (подготовлена для будущей реализации)

**Текущие данные:**
- Mode: Access Point
- SSID: HydroMonitor
- IP: 192.168.4.1

#### 🖥️ Display Settings (SCREEN_DISPLAY_SETTINGS)
- Слайдер регулировки яркости (0-100%)
- Начальное значение: 80%
- Мгновенное применение изменений
- Интеграция с `lcd_ili9341_set_brightness()`
- Рекомендации по использованию

**Функционал:**
```c
// Слайдер вызывает функцию при изменении
lcd_ili9341_set_brightness(value);  // 0-100
```

#### 📊 Data Logger (SCREEN_DATA_LOGGER_SETTINGS)
- Отображение количества записей в логе
- Кнопка **Save to NVS** — сохранение логов
- Кнопка **Clear All Logs** — очистка всех записей (красная)
- Интеграция с `data_logger` компонентом

**Функции:**
- `data_logger_get_count()` — получение количества записей
- `data_logger_save_to_nvs()` — сохранение в NVS
- `data_logger_clear()` — очистка логов

#### ℹ️ System Info (SCREEN_SYSTEM_INFO)
Отображает системную информацию:
- **Версия прошивки:** v1.0
- **Чип:** ESP32-S3
- **Ядра:** 2 (Dual Core)
- **Free Heap:** Текущая свободная память
- **Min Free:** Минимальная свободная память
- **Uptime:** Время работы системы (часы:минуты)

**Данные в реальном времени:**
```c
uint32_t free_heap = esp_get_free_heap_size();
uint32_t min_heap = esp_get_minimum_free_heap_size();
uint64_t uptime = esp_timer_get_time() / 1000000ULL;
```

#### 🔄 Reset to Defaults (SCREEN_RESET_CONFIRM)
- Экран подтверждения с предупреждением
- Иконка предупреждения (⚠️)
- Описание последствий
- Две кнопки: **No** (безопасная, зеленая) и **Yes** (опасная, красная)
- При подтверждении:
  - Вызов `config_manager_reset_to_defaults()`
  - Задержка 1 секунда
  - Перезагрузка системы `esp_restart()`

---

### 🔧 Технические изменения

#### Файл: `components/lvgl_ui/lvgl_ui.c`

**Добавлены типы экранов:**
```c
SCREEN_SYSTEM_STATUS,           // Главное меню настроек
SCREEN_AUTO_CONTROL,            // Auto Control
SCREEN_WIFI_SETTINGS,           // WiFi Settings
SCREEN_DISPLAY_SETTINGS,        // Display Settings
SCREEN_DATA_LOGGER_SETTINGS,    // Data Logger
SCREEN_SYSTEM_INFO,             // System Info
SCREEN_RESET_CONFIRM,           // Reset Confirmation
```

**Добавлены глобальные переменные:**
```c
// Экраны
static lv_obj_t *system_settings_screen = NULL;
static lv_obj_t *auto_control_screen = NULL;
static lv_obj_t *wifi_settings_screen = NULL;
static lv_obj_t *display_settings_screen = NULL;
static lv_obj_t *data_logger_screen = NULL;
static lv_obj_t *system_info_screen = NULL;
static lv_obj_t *reset_confirm_screen = NULL;

// Группы навигации
static lv_group_t *system_settings_group = NULL;
static lv_group_t *auto_control_group = NULL;
static lv_group_t *wifi_settings_group = NULL;
static lv_group_t *display_settings_group = NULL;
static lv_group_t *data_logger_group = NULL;
static lv_group_t *system_info_group = NULL;
static lv_group_t *reset_confirm_group = NULL;
```

**Добавлены функции:**
- `system_settings_button_event_cb()` — обработчик кнопки SET
- `system_menu_item_event_cb()` — обработчик пунктов меню
- `create_system_settings_screen()` — создание главного меню
- `create_auto_control_screen()` — создание экрана Auto Control
- `create_wifi_settings_screen()` — создание экрана WiFi
- `create_display_settings_screen()` — создание экрана дисплея
- `create_data_logger_screen()` — создание экрана логов
- `create_system_info_screen()` — создание экрана информации
- `create_reset_confirm_screen()` — создание экрана подтверждения
- `auto_control_switch_event_cb()` — обработчик переключателя Auto Control
- `brightness_slider_event_cb()` — обработчик слайдера яркости
- `data_logger_clear_event_cb()` — обработчик очистки логов
- `data_logger_save_event_cb()` — обработчик сохранения логов
- `reset_confirm_yes_event_cb()` — обработчик подтверждения сброса
- `reset_confirm_no_event_cb()` — обработчик отмены сброса

**Изменена функция `create_status_bar()`:**
```c
// Добавлен обработчик события для кнопки SET
lv_obj_add_event_cb(status_settings_btn, system_settings_button_event_cb, 
                    LV_EVENT_CLICKED, NULL);
lv_obj_add_flag(status_settings_btn, LV_OBJ_FLAG_CLICKABLE);
```

**Изменена функция `create_main_ui()`:**
```c
// Добавление кнопки SET в группу энкодера
if (encoder_group && status_settings_btn) {
    lv_group_add_obj(encoder_group, status_settings_btn);
    ESP_LOGI(TAG, "SET button added to encoder group");
}
```

**Обновлена функция `show_screen()`:**
- Добавлена обработка всех новых типов экранов
- Ленивая инициализация (создание экрана только при первом открытии)
- Установка соответствующих групп навигации

**Обновлена функция `back_button_event_cb()`:**
- Из подменю настроек → возврат в SCREEN_SYSTEM_STATUS
- Из главного меню настроек → возврат на SCREEN_MAIN

#### Файл: `components/lvgl_ui/CMakeLists.txt`

**Добавлены зависимости:**
```cmake
PRIV_REQUIRES 
    # ... существующие ...
    config_manager    # Для управления конфигурацией
    data_logger       # Для управления логами
```

**Добавлены включения:**
```c
#include "esp_system.h"      // Для esp_restart()
#include "config_manager.h"  // Для работы с конфигурацией
#include "data_logger.h"     // Для работы с логами
```

---

### 🎨 UI/UX изменения

#### Дизайн элементов

**Переключатели (switches):**
- Цвет активного состояния: `COLOR_ACCENT` (бирюзовый)
- Плавная анимация переключения
- Визуальная обратная связь

**Слайдеры:**
- Индикатор: `COLOR_ACCENT` (бирюзовый)
- Диапазон: 0-100 для яркости
- Мгновенное применение значения

**Кнопки опасных действий:**
- Цвет: `COLOR_DANGER` (красный)
- Используется для:
  - Clear All Logs
  - Yes (в подтверждении сброса)

**Информационные панели:**
- Стиль: `style_card`
- Отступы: 12px
- Flexbox layout для адаптивности

---

### 🔄 Навигация

#### Новые маршруты навигации:

```
Main Screen
    ↓ (SET button)
System Settings
    ↓ (выбор пункта)
    ├→ Auto Control → Back → System Settings
    ├→ WiFi Settings → Back → System Settings
    ├→ Display Settings → Back → System Settings
    ├→ Data Logger → Back → System Settings
    ├→ System Info → Back → System Settings
    └→ Reset Confirm → No/Yes
            ├→ No → System Settings
            └→ Yes → esp_restart()
```

#### Порядок фокуса на главном экране:
1-6. Карточки датчиков (pH, EC, Temp, Hum, Lux, CO2)
7. **Кнопка SET** ← новый элемент в конце

---

### 🐛 Исправления

#### Проблема: Кнопка SET не в фокусе
**Решение:** Добавлена в `encoder_group` после создания всех карточек датчиков

#### Проблема: Функции не компилируются
**Решение:** Добавлены недостающие включения (`config_manager.h`, `data_logger.h`, `esp_system.h`)

#### Проблема: CMake ошибки зависимостей
**Решение:** Добавлены `config_manager` и `data_logger` в `PRIV_REQUIRES`

---

### 📊 Статистика изменений

- **Добавлено строк кода:** ~680
- **Новых функций:** 15
- **Новых экранов:** 7
- **Новых типов экранов:** 7
- **Новых групп навигации:** 7
- **Новых обработчиков событий:** 8

---

### ⚙️ Конфигурация

#### Значения по умолчанию:

```c
// Auto Control
auto_control_enabled = true

// Display  
brightness = 80  // процентов

// Data Logger
max_entries = 1000
auto_cleanup_days = 7
```

---

### 🧪 Тестирование

#### Тест 1: Навигация к кнопке SET
```
1. На главном экране вращайте энкодер вправо
2. Пройдите все 6 карточек датчиков
3. Фокус переместится на кнопку SET
4. Нажмите энкодер → откроется меню настроек
```

#### Тест 2: Auto Control
```
1. Откройте System Settings
2. Выберите Auto Control
3. Переключите режим
4. Проверьте лог: "Auto control enabled/disabled"
5. Вернитесь назад → настройки должны сохраниться
```

#### Тест 3: Яркость дисплея
```
1. Откройте Display Settings
2. Измените яркость слайдером
3. Яркость должна меняться в реальном времени
4. Диапазон: 0% (выкл) до 100% (макс)
```

#### Тест 4: Data Logger
```
1. Откройте Data Logger
2. Проверьте количество записей
3. Нажмите Save to NVS → проверьте лог
4. Нажмите Clear All Logs → записи должны очиститься
5. Проверьте count должен стать 0
```

#### Тест 5: System Info
```
1. Откройте System Info
2. Проверьте отображение версии, чипа, памяти
3. Free Heap должен быть > 100 KB
4. Uptime должен увеличиваться со временем
```

#### Тест 6: Reset to Defaults
```
1. Откройте Reset to Defaults
2. Появится экран подтверждения
3. Выберите No → возврат в настройки
4. ИЛИ выберите Yes → система перезагрузится
5. После перезагрузки все настройки сбросятся
```

---

### 📝 Миграция для существующего кода

Если вы используете старые функции — изменений не требуется. Новая функциональность полностью обратно совместима.

**Новые возможности доступны через:**
```c
// Программное открытие системных настроек
show_screen(SCREEN_SYSTEM_STATUS);

// Проверка Auto Control
system_config_t config;
config_load(&config);
if (config.auto_control_enabled) {
    // Автоматика включена
}

// Изменение яркости
lcd_ili9341_set_brightness(70);
```

---

### 🔗 Зависимости

**Новые зависимости компонента lvgl_ui:**
- `config_manager` — для сохранения/загрузки настроек
- `data_logger` — для управления логами
- `esp_system` — для перезагрузки системы

**Используемые API:**
- `config_load()` / `config_save()`
- `config_manager_reset_to_defaults()`
- `data_logger_get_count()`
- `data_logger_save_to_nvs()`
- `data_logger_clear()`
- `lcd_ili9341_set_brightness()`
- `esp_restart()`
- `esp_get_free_heap_size()`
- `esp_get_minimum_free_heap_size()`
- `esp_timer_get_time()`

---

### 📚 Новая документация

Созданы следующие файлы документации:

1. **components/lvgl_ui/SYSTEM_SETTINGS.md**
   - Подробное описание каждого экрана
   - API функции
   - Примеры использования
   - Интеграция с другими модулями

2. **SYSTEM_SETTINGS_GUIDE.md**
   - Руководство пользователя
   - Навигация
   - Устранение неполадок
   - Клавиатурные сокращения

3. **CHANGELOG_SYSTEM_SETTINGS.md** (этот файл)
   - История изменений
   - Статистика
   - Миграция

---

### ⚡ Оптимизация

#### Ленивая инициализация
Все экраны создаются только при первом открытии:
- Экономия памяти при запуске
- Быстрая загрузка системы
- Память выделяется по требованию

#### Использование памяти
- Каждый экран: ~5-10 KB heap
- Всего при открытии всех экранов: ~50 KB
- Группы навигации: ~1 KB каждая

---

### 🎯 Следующие шаги

#### Краткосрочные (v1.2):
- [ ] Реализация сканирования WiFi сетей
- [ ] Форма ввода SSID/Password
- [ ] Просмотр последних логов
- [ ] Индивидуальные переключатели pH/EC

#### Среднесрочные (v1.3):
- [ ] Настройка интервалов коррекции
- [ ] Выбор цветовой схемы
- [ ] Экспорт логов в файл
- [ ] Статистика работы насосов

#### Долгосрочные (v2.0):
- [ ] Полная настройка WiFi через UI
- [ ] Мастер первоначальной настройки
- [ ] Профили настроек для разных культур
- [ ] Backup/Restore конфигурации

---

### 🔍 Известные ограничения

1. **WiFi Settings** — пока только просмотр, без возможности изменения
2. **Brightness** — не сохраняется в NVS (применяется до перезагрузки)
3. **Reset** — нет возможности отмены после нажатия Yes
4. **Data Logger** — нет просмотра содержимого логов через UI

---

### 💡 Советы разработчикам

#### Добавление нового экрана настроек:
1. Определите экран в `screen_type_t`
2. Создайте переменную экрана и группы
3. Реализуйте функцию создания экрана
4. Добавьте в `system_menu_item_event_cb()`
5. Добавьте в `show_screen()`
6. Обновите `back_button_event_cb()`

#### Работа с конфигурацией:
```c
// Всегда проверяйте возвращаемое значение
if (config_load(&config) == ESP_OK) {
    // Работа с конфигурацией
    if (config_save(&config) == ESP_OK) {
        ESP_LOGI(TAG, "Config saved");
    }
}
```

#### Добавление в группу навигации:
```c
// Все интерактивные элементы должны быть в группе
lv_group_add_obj(my_group, my_button);
lv_group_add_obj(my_group, my_switch);
lv_group_add_obj(my_group, my_slider);
```

---

### 🏆 Результаты

#### До изменений:
- ❌ Нет доступа к системным настройкам через UI
- ❌ Невозможно изменить Auto Control без пересборки
- ❌ Нет управления яркостью дисплея
- ❌ Нет управления логами через UI
- ❌ Нет просмотра системной информации

#### После изменений:
- ✅ Полноценное меню системных настроек
- ✅ Управление Auto Control через UI
- ✅ Регулировка яркости дисплея
- ✅ Управление логами (сохранение/очистка)
- ✅ Просмотр системной информации
- ✅ Безопасный сброс к defaults
- ✅ Навигация энкодером по всем настройкам
- ✅ Кнопка SET в фокусе навигации

---

**Автор:** Hydro Team  
**Дата:** 2025-10-08  
**Статус:** ✅ Реализовано и протестировано

