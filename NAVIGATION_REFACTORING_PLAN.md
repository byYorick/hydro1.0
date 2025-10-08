# План рефакторинга системы навигации
## Гидропонная система мониторинга v3.0

**Дата:** 2025-10-08  
**Статус:** Аналитический документ  
**Версия:** 1.0

---

## 📋 Содержание

1. [Анализ текущей системы](#анализ-текущей-системы)
2. [Выявленные проблемы](#выявленные-проблемы)
3. [Архитектура решения](#архитектура-решения)
4. [План имплементации](#план-имплементации)
5. [API и примеры использования](#api-и-примеры-использования)
6. [Миграция](#миграция)

---

## 🔍 Анализ текущей системы

### Структура компонентов

```
components/lvgl_ui/
├── lvgl_ui.c        (3345 строк) - основной файл навигации
├── lvgl_ui.h        (117 строк)  - публичный API
├── ui_manager.c     (814 строк)  - старая система (не используется)
├── ui_manager.h     (93 строки)  - старый API
├── ph_screen.c      (840 строк)  - модульный экран pH
├── ph_screen.h      (76 строк)   - API экрана pH
└── sensor_screens_optimized.c/h  - обертки
```

### Типы экранов (27 экранов)

```c
typedef enum {
    // Главный экран
    SCREEN_MAIN = 0,
    
    // Детализация датчиков (6 экранов)
    SCREEN_DETAIL_PH, SCREEN_DETAIL_EC, SCREEN_DETAIL_TEMP,
    SCREEN_DETAIL_HUMIDITY, SCREEN_DETAIL_LUX, SCREEN_DETAIL_CO2,
    
    // Настройки датчиков (6 экранов)
    SCREEN_SETTINGS_PH, SCREEN_SETTINGS_EC, SCREEN_SETTINGS_TEMP,
    SCREEN_SETTINGS_HUMIDITY, SCREEN_SETTINGS_LUX, SCREEN_SETTINGS_CO2,
    
    // Системные экраны (13 экранов)
    SCREEN_SYSTEM_STATUS, SCREEN_AUTO_CONTROL, SCREEN_WIFI_SETTINGS,
    SCREEN_DISPLAY_SETTINGS, SCREEN_DATA_LOGGER_SETTINGS,
    SCREEN_SYSTEM_INFO, SCREEN_RESET_CONFIRM, SCREEN_NETWORK_SETTINGS,
    SCREEN_MOBILE_CONNECT, SCREEN_OTA_UPDATE, SCREEN_CALIBRATION,
    SCREEN_DATA_EXPORT, SCREEN_ABOUT,
    
    SCREEN_COUNT
} screen_type_t;
```

### Глобальные переменные (25+ переменных)

```c
// Экраны
static lv_obj_t *main_screen;
static lv_obj_t *system_settings_screen;
static lv_obj_t *auto_control_screen;
static lv_obj_t *wifi_settings_screen;
static lv_obj_t *display_settings_screen;
static lv_obj_t *data_logger_screen;
static lv_obj_t *system_info_screen;
static lv_obj_t *reset_confirm_screen;
// + 6 detail_screens[], 6 settings_screens[]

// Группы энкодера
static lv_group_t *encoder_group;
static lv_group_t *system_settings_group;
static lv_group_t *auto_control_group;
static lv_group_t *wifi_settings_group;
static lv_group_t *display_settings_group;
static lv_group_t *data_logger_group;
static lv_group_t *system_info_group;
static lv_group_t *reset_confirm_group;
// + 6 detail_screen_groups[], 6 settings_screen_groups[]

// Текущее состояние
static screen_type_t current_screen = SCREEN_MAIN;
```

### Функции создания экранов (17 функций)

```c
static void create_main_ui(void);
static void create_detail_screen(uint8_t sensor_index);     // x6
static void create_settings_screen(uint8_t sensor_index);   // x6
static void create_system_settings_screen(void);
static void create_auto_control_screen(void);
static void create_wifi_settings_screen(void);
static void create_display_settings_screen(void);
static void create_data_logger_screen(void);
static void create_system_info_screen(void);
static void create_reset_confirm_screen(void);
```

### Навигация

#### 1. Функция `show_screen()` - 120 строк, огромный switch

```c
static void show_screen(screen_type_t screen)
{
    current_screen = screen;
    lv_group_t *target_group = NULL;
    lv_obj_t *target_screen_obj = NULL;

    switch (screen) {
        case SCREEN_MAIN:
            target_screen_obj = main_screen;
            target_group = encoder_group;
            break;
        case SCREEN_DETAIL_PH:
            ph_show_detail_screen();
            target_group = ph_get_detail_group();
            target_screen_obj = ph_get_detail_screen();
            break;
        // ... 25+ case statements
        default:
            break;
    }

    if (target_screen_obj) {
        switch_to_screen(target_screen_obj, screen, target_group);
    }
}
```

#### 2. Функция `back_button_event_cb()` - 60 строк, switch с логикой возврата

```c
static void back_button_event_cb(lv_event_t *e)
{
    switch (current_screen) {
        case SCREEN_DETAIL_PH:
        case SCREEN_DETAIL_EC:
        // ... 6 case statements
            show_screen(SCREEN_MAIN);
            break;
            
        case SCREEN_SETTINGS_PH:
        // ... сложная логика возврата
            break;
        
        // ... 20+ case statements
    }
}
```

---

## 🚨 Выявленные проблемы

### 1. **Высокое дублирование кода** ⚠️⚠️⚠️

**Проблема:** Каждая функция `create_*_screen()` содержит 50-150 строк похожего кода

**Пример дублирования:**
```c
// Повторяется 17 раз!
lv_obj_t *back_btn = lv_btn_create(screen);
lv_obj_add_style(back_btn, &style_card, 0);
lv_obj_set_size(back_btn, 60, 30);
lv_obj_align(back_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);

lv_obj_t *back_label = lv_label_create(back_btn);
lv_label_set_text(back_label, "←");
lv_obj_center(back_label);
```

**Оценка:** ~70% кода дублируется между функциями создания

### 2. **Жесткая связанность** ⚠️⚠️⚠️

**Проблема:** Добавление нового экрана требует изменений в 5+ местах

**Для добавления одного экрана нужно:**
1. Добавить enum в `screen_type_t`
2. Создать глобальную переменную `lv_obj_t *new_screen`
3. Создать глобальную переменную `lv_group_t *new_group`
4. Написать функцию `create_new_screen()` (50-150 строк)
5. Добавить case в `show_screen()` switch
6. Добавить case в `back_button_event_cb()` switch
7. Добавить обработку в `handle_encoder_event()`

**Риск:** Высокая вероятность ошибок, забыть добавить в одном месте

### 3. **Отсутствие абстракции** ⚠️⚠️

**Проблема:** Нет единого интерфейса для работы с экранами

**Текущая ситуация:**
- Для pH используется `ph_screen.c` с модульным API
- Для остальных датчиков - встроенные функции
- Для системных экранов - еще один подход
- Старая система `ui_manager.c` не используется

**Результат:** 3 разных паттерна создания экранов

### 4. **Ручное управление памятью** ⚠️⚠️

**Проблема:** Нет централизованного управления жизненным циклом экранов

```c
// Ленивая инициализация разбросана по коду
if (system_settings_screen == NULL) {
    create_system_settings_screen();
}

if (auto_control_screen == NULL) {
    create_auto_control_screen();
}

// Нет механизма очистки неиспользуемых экранов
// Нет контроля утечек памяти
```

### 5. **Сложная навигационная логика** ⚠️⚠️

**Проблема:** Граф навигации хардкоден в switch statements

**Пример:**
```c
// Логика возврата разбросана по коду
case SCREEN_AUTO_CONTROL:
case SCREEN_WIFI_SETTINGS:
// ... 4 экрана
    show_screen(SCREEN_SYSTEM_STATUS);  // Возврат к родителю
    break;

case SCREEN_SETTINGS_PH:
    uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
    show_screen(SCREEN_DETAIL_PH + sensor_index);  // Возврат к детализации
    break;
```

**Проблема:** Невозможно визуализировать структуру навигации

### 6. **Масштабируемость** ⚠️⚠️

**Проблема:** Чем больше экранов, тем сложнее поддержка

**Текущая ситуация:**
- 27 экранов
- 3345 строк в lvgl_ui.c
- 25+ глобальных переменных
- 120+ строк в switch statements

**При добавлении 10 новых экранов:**
- ~4000+ строк кода
- ~35+ глобальных переменных
- ~160+ строк в switch
- Файл станет немейнтейнабельным

### 7. **Тестируемость** ⚠️

**Проблема:** Невозможно юнит-тестировать навигацию

- Глобальное состояние
- Жесткая связанность с LVGL
- Нет инверсии зависимостей

### 8. **Производительность памяти** ⚠️

**Проблема:** Все экраны остаются в памяти после создания

- Нет механизма выгрузки
- Ленивая загрузка работает только при первом открытии
- На ESP32-S3 память ограничена

---

## 🏗️ Архитектура решения

### Концепция: Screen Manager Pattern

Центральная идея - **декларативное** описание экранов и **автоматическое** управление навигацией.

```
┌─────────────────────────────────────────────────────────────┐
│                     Screen Manager                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Registry    │  │  Navigator   │  │ Lifecycle    │     │
│  │              │  │              │  │ Manager      │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────▼───────┐     ┌───────▼───────┐     ┌──────▼──────┐
│  Screen Base  │     │  Screen Base  │     │ Screen Base │
│  (Abstract)   │     │  (Abstract)   │     │ (Abstract)  │
└───────┬───────┘     └───────┬───────┘     └──────┬──────┘
        │                     │                     │
┌───────▼───────┐     ┌───────▼───────┐     ┌──────▼──────┐
│ Main Screen   │     │ Detail Screen │     │Settings     │
│               │     │  (Template)   │     │Screen       │
└───────────────┘     └───────────────┘     └─────────────┘
```

### Компоненты новой архитектуры

#### 1. **Screen Descriptor** - Декларативное описание

```c
typedef enum {
    SCREEN_TYPE_MAIN,
    SCREEN_TYPE_DETAIL,
    SCREEN_TYPE_SETTINGS,
    SCREEN_TYPE_MENU,
    SCREEN_TYPE_FORM,
    SCREEN_TYPE_DIALOG
} screen_category_t;

typedef struct screen_config_t {
    const char *id;                    // Уникальный ID (например, "main", "detail_ph")
    const char *title;                 // Заголовок экрана
    screen_category_t category;        // Категория экрана
    
    // Навигация
    const char *parent_id;             // ID родительского экрана (для back)
    bool can_go_back;                  // Можно ли вернуться назад
    
    // Жизненный цикл
    bool lazy_load;                    // Ленивая загрузка
    bool cache_on_hide;                // Кэшировать при скрытии
    bool destroy_on_hide;              // Уничтожать при скрытии
    
    // Создание
    screen_create_fn_t create_fn;      // Функция создания
    screen_destroy_fn_t destroy_fn;    // Функция уничтожения
    
    // Callbacks
    screen_show_fn_t on_show;          // При показе
    screen_hide_fn_t on_hide;          // При скрытии
    screen_update_fn_t on_update;      // При обновлении данных
    
    // Данные
    void *user_data;                   // Произвольные данные
} screen_config_t;
```

#### 2. **Screen Instance** - Экземпляр экрана

```c
typedef struct screen_instance_t {
    screen_config_t *config;           // Конфигурация
    lv_obj_t *screen_obj;              // LVGL объект
    lv_group_t *encoder_group;         // Группа энкодера
    
    // Состояние
    bool is_created;
    bool is_visible;
    uint32_t create_time;              // Timestamp создания
    uint32_t last_show_time;           // Timestamp последнего показа
    
    // Навигация
    struct screen_instance_t *parent;  // Родитель
    struct screen_instance_t *children[MAX_CHILDREN];  // Дети
} screen_instance_t;
```

#### 3. **Screen Manager** - Менеджер экранов

```c
typedef struct {
    // Реестр экранов
    screen_config_t *screens[MAX_SCREENS];
    uint8_t screen_count;
    
    // Активные экземпляры
    screen_instance_t *instances[MAX_INSTANCES];
    uint8_t instance_count;
    
    // Текущий экран
    screen_instance_t *current_screen;
    
    // История навигации
    screen_instance_t *history[MAX_HISTORY];
    uint8_t history_index;
    
    // Конфигурация
    screen_manager_config_t config;
    
    // Мьютекс
    SemaphoreHandle_t mutex;
} screen_manager_t;
```

### API функции

#### Инициализация

```c
esp_err_t screen_manager_init(screen_manager_config_t *config);
esp_err_t screen_manager_deinit(void);
```

#### Регистрация экранов

```c
esp_err_t screen_register(const screen_config_t *config);
esp_err_t screen_unregister(const char *screen_id);
```

#### Навигация

```c
esp_err_t screen_show(const char *screen_id, void *params);
esp_err_t screen_hide(const char *screen_id);
esp_err_t screen_go_back(void);
esp_err_t screen_go_to_parent(void);
esp_err_t screen_go_home(void);
```

#### Управление жизненным циклом

```c
esp_err_t screen_create(const char *screen_id);
esp_err_t screen_destroy(const char *screen_id);
esp_err_t screen_reload(const char *screen_id);
```

#### Утилиты

```c
screen_instance_t* screen_get_current(void);
screen_instance_t* screen_get_by_id(const char *screen_id);
bool screen_is_visible(const char *screen_id);
uint8_t screen_get_history_count(void);
```

---

## 📐 Новая структура файлов

```
components/lvgl_ui/
├── screen_manager/
│   ├── screen_manager.c        // Менеджер экранов
│   ├── screen_manager.h        // API
│   ├── screen_registry.c       // Реестр экранов
│   ├── screen_navigator.c      // Навигация
│   ├── screen_lifecycle.c      // Управление жизненным циклом
│   └── screen_types.h          // Типы данных
│
├── screens/
│   ├── base/
│   │   ├── screen_base.c       // Базовый экран
│   │   ├── screen_template.c   // Шаблоны экранов
│   │   └── screen_factory.c    // Фабрика экранов
│   │
│   ├── main_screen.c           // Главный экран
│   │
│   ├── sensor/
│   │   ├── sensor_detail_screen.c     // Шаблон детализации
│   │   ├── sensor_settings_screen.c   // Шаблон настроек
│   │   └── ph_screen.c                // pH (сохраняем модуль)
│   │
│   └── system/
│       ├── system_menu_screen.c
│       ├── wifi_settings_screen.c
│       ├── auto_control_screen.c
│       └── ...
│
├── widgets/
│   ├── status_bar.c            // Виджет статус-бара
│   ├── back_button.c           // Виджет кнопки назад
│   ├── menu_list.c             // Виджет списка меню
│   └── sensor_card.c           // Виджет карточки датчика
│
├── navigation/
│   ├── navigation_config.c     // Конфигурация навигации
│   └── screen_graph.c          // Граф навигации
│
└── lvgl_ui.c                   // Точка входа (упрощенная)
```

---

## 🎯 Преимущества новой архитектуры

### 1. **Простота добавления экранов** ✅✅✅

**Было** (5+ файлов, 200+ строк):
```c
// 1. Добавить enum
typedef enum { ..., SCREEN_NEW, ... } screen_type_t;

// 2. Глобальные переменные
static lv_obj_t *new_screen = NULL;
static lv_group_t *new_group = NULL;

// 3. Функция создания (50-150 строк)
static void create_new_screen(void) { ... }

// 4. Switch в show_screen()
case SCREEN_NEW:
    if (new_screen == NULL) create_new_screen();
    ...

// 5. Switch в back_button_event_cb()
case SCREEN_NEW:
    show_screen(SCREEN_PARENT);
    break;

// 6. Обработка энкодера
case SCREEN_NEW:
    // навигация
    break;
```

**Стало** (1 место, ~20 строк):
```c
// В файле screens/my_new_screen.c

static lv_obj_t* my_screen_create(void *params) {
    lv_obj_t *screen = lv_obj_create(NULL);
    // ... создание UI (переиспользование виджетов)
    return screen;
}

static esp_err_t my_screen_init(void) {
    screen_config_t config = {
        .id = "my_new_screen",
        .title = "My New Screen",
        .category = SCREEN_TYPE_MENU,
        .parent_id = "main",
        .create_fn = my_screen_create,
        .lazy_load = true,
    };
    return screen_register(&config);
}

// Регистрация в main
my_screen_init();

// Навигация автоматическая!
screen_show("my_new_screen", NULL);
```

**Сокращение кода:** 90% меньше кода для нового экрана

### 2. **Переиспользование кода** ✅✅✅

**Пример:** Шаблон для экранов настроек датчиков

```c
// Один шаблон для всех 6 датчиков
esp_err_t sensor_settings_register_all(void) {
    for (int i = 0; i < 6; i++) {
        screen_config_t config = {
            .id = sensor_settings_ids[i],
            .title = sensor_names[i],
            .category = SCREEN_TYPE_SETTINGS,
            .parent_id = sensor_detail_ids[i],
            .create_fn = sensor_settings_create_template,  // Общая функция!
            .user_data = &sensor_configs[i],               // Параметризация
        };
        screen_register(&config);
    }
}
```

**Результат:** 1 функция вместо 6

### 3. **Декларативная навигация** ✅✅

**Граф навигации** можно визуализировать:

```
main
├── detail_ph
│   ├── settings_ph
│   └── calibration_ph
├── detail_ec
│   └── settings_ec
├── ...
└── system_status
    ├── auto_control
    ├── wifi_settings
    └── display_settings
```

**Код конфигурации:**
```c
static navigation_graph_t nav_graph = {
    { "main", NULL },                          // Корень
    { "detail_ph", "main" },                   // Родитель: main
    { "settings_ph", "detail_ph" },            // Родитель: detail_ph
    { "system_status", "main" },
    { "auto_control", "system_status" },
    // ...
};
```

**Навигация автоматическая:**
```c
screen_go_back();        // Автоматически вернется к родителю
screen_go_home();        // Автоматически вернется к main
```

### 4. **Управление памятью** ✅✅

**Автоматическая выгрузка:**
```c
screen_config_t config = {
    .id = "rarely_used_screen",
    .destroy_on_hide = true,      // Уничтожать при скрытии
    .cache_on_hide = false,       // Не кэшировать
};

// При уходе с экрана - память освобождается автоматически!
```

**Кэширование:**
```c
screen_config_t config = {
    .id = "often_used_screen",
    .cache_on_hide = true,        // Кэшировать
    .destroy_on_hide = false,
};

// При повторном показе - мгновенно, без пересоздания
```

### 5. **Тестируемость** ✅

**Можно тестировать навигацию:**
```c
// Юнит-тест
void test_navigation(void) {
    screen_show("detail_ph", NULL);
    assert(screen_get_current()->config->id == "detail_ph");
    
    screen_go_back();
    assert(screen_get_current()->config->id == "main");
}
```

**Можно мокать:**
```c
// Мок функции создания для тестов
static lv_obj_t* mock_screen_create(void *params) {
    return (lv_obj_t*)0xDEADBEEF;  // Заглушка
}
```

### 6. **Расширяемость** ✅✅

**Легко добавить новые фичи:**

```c
// Анимации переходов
screen_config_t config = {
    .transition_type = TRANSITION_SLIDE_LEFT,
    .transition_duration = 300,
};

// Условная навигация
screen_config_t config = {
    .can_show_fn = check_permissions,  // Проверка прав
};

// Middleware
screen_config_t config = {
    .before_show = log_analytics,      // Аналитика
    .after_hide = save_state,          // Сохранение состояния
};
```

---

## 📝 План имплементации

### Фаза 1: Подготовка (1-2 дня)

#### 1.1. Создать структуру директорий
```bash
mkdir -p components/lvgl_ui/screen_manager
mkdir -p components/lvgl_ui/screens/base
mkdir -p components/lvgl_ui/screens/sensor
mkdir -p components/lvgl_ui/screens/system
mkdir -p components/lvgl_ui/widgets
mkdir -p components/lvgl_ui/navigation
```

#### 1.2. Создать базовые типы данных
- Файл: `screen_manager/screen_types.h`
- Определить структуры:
  - `screen_config_t`
  - `screen_instance_t`
  - `screen_manager_t`
  - Callback типы

#### 1.3. Документировать текущую навигацию
- Создать граф навигации
- Задокументировать все 27 экранов
- Определить родительские связи

### Фаза 2: Core Manager (2-3 дня)

#### 2.1. Screen Registry
Файл: `screen_manager/screen_registry.c`

```c
// Функции:
- screen_registry_init()
- screen_register()
- screen_unregister()
- screen_find_by_id()
- screen_get_all()
```

#### 2.2. Screen Lifecycle
Файл: `screen_manager/screen_lifecycle.c`

```c
// Функции:
- screen_create_instance()
- screen_destroy_instance()
- screen_show_instance()
- screen_hide_instance()
- screen_reload_instance()
```

#### 2.3. Screen Navigator
Файл: `screen_manager/screen_navigator.c`

```c
// Функции:
- navigator_init()
- navigator_show()
- navigator_go_back()
- navigator_go_to_parent()
- navigator_push_history()
- navigator_pop_history()
```

#### 2.4. Screen Manager API
Файл: `screen_manager/screen_manager.c`

```c
// Главный API объединяющий все компоненты
- screen_manager_init()
- screen_show()
- screen_hide()
- screen_go_back()
// ...
```

#### 2.5. Юнит-тесты
- Тестировать каждый компонент отдельно
- Mock LVGL функции

### Фаза 3: Виджеты (1-2 дня)

#### 3.1. Базовые виджеты
```c
// widgets/back_button.c
lv_obj_t* widget_create_back_button(lv_obj_t *parent, 
                                     lv_event_cb_t callback);

// widgets/status_bar.c
lv_obj_t* widget_create_status_bar(lv_obj_t *parent, 
                                    const char *title);

// widgets/menu_list.c
lv_obj_t* widget_create_menu_list(lv_obj_t *parent, 
                                   menu_item_t *items, 
                                   uint8_t count);

// widgets/sensor_card.c
lv_obj_t* widget_create_sensor_card(lv_obj_t *parent,
                                     sensor_config_t *config);
```

### Фаза 4: Шаблоны экранов (2-3 дня)

#### 4.1. Screen Base
Файл: `screens/base/screen_base.c`

```c
// Базовая структура экрана:
lv_obj_t* screen_base_create(screen_config_t *config) {
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Применяем базовый стиль
    lv_obj_add_style(screen, &style_bg, 0);
    
    // Создаем статус-бар (если нужен)
    if (config->has_status_bar) {
        widget_create_status_bar(screen, config->title);
    }
    
    // Создаем кнопку назад (если можно)
    if (config->can_go_back) {
        widget_create_back_button(screen, back_callback);
    }
    
    return screen;
}
```

#### 4.2. Screen Templates
Файл: `screens/base/screen_template.c`

```c
// Шаблон экрана меню
lv_obj_t* template_menu_screen(const char *title,
                                menu_item_t *items,
                                uint8_t count);

// Шаблон экрана формы
lv_obj_t* template_form_screen(const char *title,
                                form_field_t *fields,
                                uint8_t count);

// Шаблон экрана детализации
lv_obj_t* template_detail_screen(const char *title,
                                  detail_config_t *config);
```

#### 4.3. Screen Factory
Файл: `screens/base/screen_factory.c`

```c
// Фабрика для создания экранов по конфигу
lv_obj_t* screen_factory_create(screen_config_t *config);
```

### Фаза 5: Миграция главного экрана (1 день)

#### 5.1. Портировать главный экран
```c
// screens/main_screen.c

static lv_obj_t* main_screen_create(void *params) {
    lv_obj_t *screen = screen_base_create(&main_config);
    
    // Создаем контент главного экрана
    // Переиспользуем виджеты
    for (int i = 0; i < SENSOR_COUNT; i++) {
        widget_create_sensor_card(screen, &sensor_configs[i]);
    }
    
    return screen;
}

esp_err_t main_screen_init(void) {
    screen_config_t config = {
        .id = "main",
        .title = "Hydroponics Monitor",
        .category = SCREEN_TYPE_MAIN,
        .parent_id = NULL,
        .create_fn = main_screen_create,
    };
    return screen_register(&config);
}
```

### Фаза 6: Миграция экранов датчиков (2 дня)

#### 6.1. Шаблон детализации датчика
```c
// screens/sensor/sensor_detail_screen.c

static lv_obj_t* sensor_detail_create(void *params) {
    sensor_config_t *cfg = (sensor_config_t*)params;
    return template_detail_screen(cfg->name, &cfg->detail_config);
}

esp_err_t sensor_detail_screens_register(void) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        screen_config_t config = {
            .id = sensor_detail_ids[i],
            .title = sensor_names[i],
            .category = SCREEN_TYPE_DETAIL,
            .parent_id = "main",
            .create_fn = sensor_detail_create,
            .user_data = &sensor_configs[i],
        };
        screen_register(&config);
    }
}
```

#### 6.2. Шаблон настроек датчика
```c
// screens/sensor/sensor_settings_screen.c
// Аналогично детализации
```

#### 6.3. Сохранить pH screen как модуль
```c
// screens/sensor/ph_screen.c
// Оставляем как есть, интегрируем через адаптер

esp_err_t ph_screen_register(void) {
    screen_config_t config = {
        .id = "detail_ph",
        .title = "pH Details",
        .category = SCREEN_TYPE_DETAIL,
        .parent_id = "main",
        .create_fn = ph_screen_adapter_create,  // Адаптер к существующему
    };
    return screen_register(&config);
}
```

### Фаза 7: Миграция системных экранов (1-2 дня)

#### 7.1. Экран системных настроек
```c
// screens/system/system_menu_screen.c

static menu_item_t system_menu_items[] = {
    { "Auto Control", "auto_control", ICON_AUTO },
    { "WiFi Settings", "wifi_settings", ICON_WIFI },
    { "Display Settings", "display_settings", ICON_DISPLAY },
    // ...
};

static lv_obj_t* system_menu_create(void *params) {
    lv_obj_t *screen = screen_base_create(&system_config);
    widget_create_menu_list(screen, system_menu_items, 
                           sizeof(system_menu_items)/sizeof(menu_item_t));
    return screen;
}
```

#### 7.2. Остальные системные экраны
- WiFi Settings
- Auto Control
- Display Settings
- Data Logger
- System Info
- Reset Confirm

### Фаза 8: Интеграция с энкодером (1 день)

#### 8.1. Обновить handle_encoder_event()
```c
static void handle_encoder_event(encoder_event_t *event) {
    screen_instance_t *current = screen_get_current();
    
    switch (event->type) {
        case ENCODER_EVENT_ROTATE_CW:
            // Делегируем группе LVGL (уже работает)
            lv_group_focus_next(current->encoder_group);
            break;
            
        case ENCODER_EVENT_BUTTON_PRESS:
            // Отправляем в группу
            lv_group_send_data(current->encoder_group, LV_KEY_ENTER);
            break;
    }
    
    // Вся навигация между экранами - через screen_manager!
}
```

### Фаза 9: Тестирование (2 дня)

#### 9.1. Функциональные тесты
- Навигация между всеми экранами
- Кнопка "Назад" работает корректно
- Энкодер работает на всех экранах
- Память не утекает

#### 9.2. Нагрузочные тесты
- Быстрое переключение между экранами
- Создание/уничтожение экранов
- История навигации

#### 9.3. Тесты на устройстве
- Проверка на реальном ESP32-S3
- Измерение использования памяти
- Производительность

### Фаза 10: Документация (1 день)

#### 10.1. Документация API
- Doxygen комментарии
- Примеры использования
- Руководство по добавлению экранов

#### 10.2. Архитектурная документация
- Диаграммы UML
- Граф навигации
- Руководство разработчика

### Фаза 11: Очистка (1 день)

#### 11.1. Удалить старый код
- Удалить ui_manager.c/h (если не используется)
- Удалить дублированные функции
- Очистить lvgl_ui.c

#### 11.2. Рефакторинг
- Переименовать файлы
- Обновить CMakeLists.txt
- Обновить include пути

---

## 💻 API и примеры использования

### Пример 1: Добавление простого экрана

```c
// 1. Создаем файл: screens/system/my_feature_screen.c

#include "screen_manager.h"
#include "screen_template.h"

static lv_obj_t* my_feature_screen_create(void *params) {
    // Используем шаблон меню
    menu_item_t items[] = {
        { "Option 1", on_option1_click, NULL },
        { "Option 2", on_option2_click, NULL },
        { "Option 3", on_option3_click, NULL },
    };
    
    return template_menu_screen("My Feature", items, 3);
}

esp_err_t my_feature_screen_init(void) {
    screen_config_t config = {
        .id = "my_feature",
        .title = "My Feature",
        .category = SCREEN_TYPE_MENU,
        .parent_id = "system_status",     // Родитель - системное меню
        .create_fn = my_feature_screen_create,
        .lazy_load = true,                // Ленивая загрузка
        .destroy_on_hide = true,          // Освобождать память
    };
    return screen_register(&config);
}

// 2. Регистрируем в main
void app_main(void) {
    // ...
    screen_manager_init(NULL);
    main_screen_init();
    system_menu_init();
    my_feature_screen_init();  // <-- Добавили одну строку!
}

// 3. Навигация автоматическая!
// При нажатии на пункт меню "My Feature":
screen_show("my_feature", NULL);

// При нажатии "Назад" - автоматически вернется к "system_status"
```

### Пример 2: Экран с параметрами

```c
typedef struct {
    int sensor_index;
    float current_value;
} sensor_detail_params_t;

static lv_obj_t* sensor_detail_create(void *params) {
    sensor_detail_params_t *p = (sensor_detail_params_t*)params;
    
    lv_obj_t *screen = screen_base_create(&detail_config);
    
    // Отображаем значение
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text_fmt(label, "Sensor %d: %.2f", 
                          p->sensor_index, p->current_value);
    
    return screen;
}

// Использование
sensor_detail_params_t params = {
    .sensor_index = 0,
    .current_value = 6.8f,
};
screen_show("detail_ph", &params);
```

### Пример 3: Динамическое обновление

```c
static esp_err_t sensor_detail_on_update(void *params) {
    sensor_detail_params_t *p = (sensor_detail_params_t*)params;
    
    // Обновляем UI
    lv_label_set_text_fmt(value_label, "%.2f", p->current_value);
    
    return ESP_OK;
}

screen_config_t config = {
    // ...
    .on_update = sensor_detail_on_update,
};

// Позже можно обновить экран без пересоздания
sensor_detail_params_t new_params = { .current_value = 7.2f };
screen_update("detail_ph", &new_params);
```

### Пример 4: Условная навигация

```c
static bool check_admin_access(void) {
    // Проверяем права администратора
    return is_admin_logged_in();
}

screen_config_t config = {
    .id = "admin_settings",
    // ...
    .can_show_fn = check_admin_access,  // Проверка перед показом
};

// Если пользователь не админ - покажется ошибка
esp_err_t ret = screen_show("admin_settings", NULL);
if (ret == ESP_ERR_NOT_ALLOWED) {
    show_error_message("Access denied");
}
```

---

## 🔄 Миграция существующего кода

### Стратегия миграции

**Постепенная миграция** без остановки разработки:

1. ✅ **Фаза 1**: Новая система работает параллельно со старой
2. ✅ **Фаза 2**: Постепенно переносим экраны
3. ✅ **Фаза 3**: Удаляем старый код

### Совместимость

```c
// Адаптер для старого API
esp_err_t lvgl_open_detail_screen(int index) {
    const char *screen_ids[] = {
        "detail_ph", "detail_ec", "detail_temp",
        "detail_humidity", "detail_lux", "detail_co2"
    };
    return screen_show(screen_ids[index], NULL);
}

void lvgl_close_detail_screen(void) {
    screen_go_back();
}
```

### Таблица миграции

| Старая функция | Новая функция | Статус |
|---------------|---------------|--------|
| `show_screen(SCREEN_MAIN)` | `screen_show("main", NULL)` | ✅ Эквивалентно |
| `back_button_event_cb()` | `screen_go_back()` | ✅ Упрощено |
| `create_detail_screen(i)` | Шаблон `template_detail_screen()` | ✅ Переиспользование |
| `lvgl_set_focus(i)` | LVGL группы (без изменений) | ✅ Работает как есть |

---

## 📊 Метрики успеха

### Измеримые цели

1. **Сокращение кода**: -50% строк в lvgl_ui.c
2. **Время добавления экрана**: с 2 часов до 30 минут
3. **Дублирование кода**: с 70% до 10%
4. **Покрытие тестами**: 0% → 80%
5. **Использование памяти**: -20% (за счет автовыгрузки)

### KPI

- ✅ Добавление нового экрана требует < 50 строк кода
- ✅ Граф навигации визуализируем в 1 файле
- ✅ 0 глобальных переменных (инкапсуляция в manager)
- ✅ Все экраны покрыты юнит-тестами
- ✅ Документация API на уровне Doxygen

---

## ⚠️ Риски и митигация

### Риск 1: Нарушение работы энкодера
**Вероятность:** Средняя  
**Митигация:** 
- Постепенная миграция с тестами
- Сохранить старую логику энкодера
- Тестировать на каждом этапе

### Риск 2: Увеличение потребления памяти
**Вероятность:** Низкая  
**Митигация:**
- Автоматическая выгрузка экранов
- Профилирование памяти
- Оптимизация структур данных

### Риск 3: Сложность для новых разработчиков
**Вероятность:** Средняя  
**Митигация:**
- Подробная документация
- Примеры кода
- Шаблоны для типовых экранов

### Риск 4: Время на разработку
**Вероятность:** Высокая  
**Митигация:**
- Фазовая реализация (можно остановиться на любом этапе)
- Начать с самого ценного (Screen Manager)
- Параллельная работа со старой системой

---

## 🎓 Выводы и рекомендации

### Главные преимущества рефакторинга

1. ✅ **Масштабируемость**: Легко добавлять новые экраны
2. ✅ **Поддержка**: Код чистый и понятный
3. ✅ **Надежность**: Централизованное управление
4. ✅ **Производительность**: Оптимизация памяти
5. ✅ **Тестируемость**: Модульная архитектура

### Рекомендуемый план

**Минимальный MVP** (1 неделя):
- Фазы 1-2: Screen Manager Core
- Фаза 5: Миграция главного экрана
- Работающий прототип

**Полная миграция** (3 недели):
- Все 11 фаз
- Полная документация
- Все тесты

**Оптимальный подход:**
- Начать с MVP
- Оценить результаты
- Принять решение о полной миграции

### Альтернативы

Если полный рефакторинг слишком затратный:

1. **Минимальный рефакторинг**:
   - Только Screen Factory для генерации похожих экранов
   - Таблица навигации вместо switch
   - Время: 3 дня, выгода: 30%

2. **Постепенное улучшение**:
   - Выносить общий код в функции
   - Создать макросы для типовых паттернов
   - Время: 1 неделя, выгода: 50%

3. **Гибридный подход**:
   - Screen Manager только для новых экранов
   - Старые экраны оставить как есть
   - Время: 2 недели, выгода: 70%

---

## 📚 Дополнительные материалы

### Паттерны проектирования

- **Screen Manager**: Facade Pattern
- **Screen Registry**: Registry Pattern
- **Screen Factory**: Factory Pattern
- **Screen Templates**: Template Method Pattern
- **Navigation**: Chain of Responsibility

### Похожие реализации

- Android Navigation Component
- iOS UINavigationController
- Flutter Navigator 2.0
- React Navigation

### Ссылки

- [LVGL Documentation](https://docs.lvgl.io/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/)
- Design Patterns (Gang of Four)

---

**Автор:** AI Assistant (Claude)  
**Дата:** 2025-10-08  
**Версия документа:** 1.0  
**Проект:** Hydroponics Monitor v3.0

