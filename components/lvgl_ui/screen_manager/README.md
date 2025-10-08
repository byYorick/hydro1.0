# Screen Manager System
## Система управления экранами для Hydroponics Monitor v3.0

**Версия:** 1.0  
**Дата:** 2025-10-08  
**Статус:** Реализован (Core завершен)

---

## 🎯 Обзор

Screen Manager - это модульная система управления экранами пользовательского интерфейса, которая:

- ✅ **Упрощает добавление новых экранов** - от 200+ строк до ~20 строк
- ✅ **Автоматизирует навигацию** - parent/child отношения, история, back button
- ✅ **Управляет памятью** - ленивая загрузка, кэширование, автоочистка
- ✅ **Переиспользует код** - виджеты и шаблоны
- ✅ **Тестируемая** - модульная архитектура, dependency injection

---

## 🏗️ Архитектура

```
Screen Manager
├── Registry      - Реестр конфигураций экранов
├── Lifecycle     - Создание/уничтожение экземпляров
├── Navigator     - Навигация с историей
└── Manager API   - Упрощенный публичный API

Widgets           - Переиспользуемые компоненты UI
Templates         - Готовые шаблоны экранов
Base Classes      - Базовые классы экранов
```

---

## 📚 Быстрый старт

### 1. Инициализация

```c
#include "screen_manager/screen_manager.h"

void app_main(void) {
    // Инициализируем Screen Manager
    screen_manager_init(NULL);  // NULL = настройки по умолчанию
    
    // Регистрируем экраны
    my_screens_register_all();
    
    // Показываем главный экран
    screen_show("main", NULL);
}
```

### 2. Создание простого экрана

```c
// Файл: screens/my_screen.c

#include "screen_manager/screen_manager.h"
#include "widgets/back_button.h"
#include "widgets/menu_list.h"

// Callback для создания UI
static lv_obj_t* my_screen_create(void *params) {
    // Используем базовый экран
    screen_base_config_t base_cfg = {
        .title = "My Screen",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,  // Автоматическая навигация назад
    };
    
    screen_base_t base = screen_base_create(&base_cfg);
    
    // Добавляем свой контент в base.content
    lv_obj_t *label = lv_label_create(base.content);
    lv_label_set_text(label, "Hello from my screen!");
    lv_obj_center(label);
    
    return base.screen;
}

// Регистрация экрана
esp_err_t my_screen_init(void) {
    screen_config_t config = {
        .id = "my_screen",                // Уникальный ID
        .title = "My Screen",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "main",              // Родитель для кнопки "Назад"
        .can_go_back = true,
        .lazy_load = true,                // Создать при первом показе
        .create_fn = my_screen_create,    // Функция создания
    };
    
    return screen_register(&config);
}
```

### 3. Использование

```c
// Показать экран
screen_show("my_screen", NULL);

// Вернуться назад (автоматически к "main")
screen_go_back();

// Вернуться на главный экран
screen_go_home();
```

---

## 📖 API Reference

### Инициализация

```c
// Инициализация с настройками по умолчанию
screen_manager_init(NULL);

// Инициализация с custom конфигурацией
screen_manager_config_t config = {
    .enable_cache = true,
    .enable_history = true,
    .max_cache_size = 5,
    .transition_time = 300,  // мс
    .enable_animations = true,
};
screen_manager_init(&config);
```

### Регистрация экранов

```c
screen_config_t config = {
    // ОБЯЗАТЕЛЬНЫЕ
    .id = "my_screen",          // Уникальный ID (a-z, 0-9, _, -)
    .create_fn = create_func,   // Функция создания
    
    // РЕКОМЕНДУЕМЫЕ
    .title = "My Screen",       // Заголовок
    .category = SCREEN_CATEGORY_MENU,
    .parent_id = "main",        // Для автоматической навигации назад
    .can_go_back = true,
    
    // ОПЦИОНАЛЬНЫЕ
    .lazy_load = true,          // Создать при первом показе (экономия памяти)
    .destroy_on_hide = false,   // Уничтожить при скрытии (освобождение памяти)
    .cache_on_hide = true,      // Кэшировать для быстрого повторного показа
    
    .on_show = NULL,            // Callback при показе
    .on_hide = NULL,            // Callback при скрытии
    .on_update = NULL,          // Callback при обновлении данных
    .can_show_fn = NULL,        // Проверка прав перед показом
    
    .user_data = NULL,          // Произвольные данные
};

screen_register(&config);
```

### Навигация

```c
// Показать экран
screen_show("detail_ph", NULL);

// Показать с параметрами
sensor_params_t params = {.sensor_id = 0};
screen_show("detail_ph", &params);

// Навигация
screen_go_back();         // Вернуться назад (из истории)
screen_go_to_parent();    // К родителю (по parent_id)
screen_go_home();         // На главный экран

// Обновить данные без пересоздания
float new_value = 6.8f;
screen_update("detail_ph", &new_value);
```

### Управление жизненным циклом

```c
// Создать экран вручную (опционально)
screen_create("my_screen");

// Уничтожить экран
screen_destroy("my_screen");

// Перезагрузить экран (пересоздать)
screen_reload("my_screen");
```

### Информация

```c
// Текущий экран
screen_instance_t *current = screen_get_current();
ESP_LOGI(TAG, "Current screen: %s", current->config->id);

// Проверка видимости
if (screen_is_visible_check("detail_ph")) {
    // Экран виден
}

// Размер истории
uint8_t history_size = screen_get_history_count();
ESP_LOGI(TAG, "History size: %d", history_size);
```

---

## 🔧 Структура файлов

```
components/lvgl_ui/
├── screen_manager/              # Ядро системы
│   ├── screen_types.h           # Типы данных
│   ├── screen_registry.h/c      # Реестр экранов
│   ├── screen_lifecycle.h/c     # Управление жизненным циклом
│   ├── screen_navigator.h/c     # Навигация
│   ├── screen_manager.h/c       # Главный API
│   └── README.md                # Эта документация
│
├── screens/                     # Конкретные экраны
│   ├── base/                    # Базовые классы
│   │   ├── screen_base.h/c      # Базовая структура экрана
│   │   └── screen_template.h/c  # Шаблоны типовых экранов
│   ├── sensor/                  # Экраны датчиков
│   └── system/                  # Системные экраны
│
└── widgets/                     # Переиспользуемые виджеты
    ├── back_button.h/c          # Кнопка назад
    ├── status_bar.h/c           # Статус-бар
    ├── menu_list.h/c            # Список меню
    └── sensor_card.h/c          # Карточка датчика
```

---

## 💡 Примеры использования

### Пример 1: Экран меню

```c
#include "screen_manager/screen_manager.h"
#include "screens/base/screen_template.h"

// Callback для пунктов меню
static void on_option1_click(lv_event_t *e) {
    screen_show("option1_screen", NULL);
}

static void on_option2_click(lv_event_t *e) {
    screen_show("option2_screen", NULL);
}

// Функция создания
static lv_obj_t* menu_screen_create(void *params) {
    menu_item_config_t items[] = {
        { .text = "Option 1", .callback = on_option1_click },
        { .text = "Option 2", .callback = on_option2_click },
        { .text = "Option 3", .callback = NULL },
    };
    
    template_menu_config_t menu_cfg = {
        .title = "Menu",
        .items = items,
        .item_count = 3,
        .has_back_button = true,
    };
    
    // Получаем группу энкодера из текущего экземпляра
    screen_instance_t *inst = screen_get_by_id("menu_screen");
    lv_group_t *group = inst ? inst->encoder_group : NULL;
    
    return template_create_menu_screen(&menu_cfg, group);
}

// Регистрация
esp_err_t menu_screen_init(void) {
    screen_config_t config = {
        .id = "menu_screen",
        .title = "Menu",
        .category = SCREEN_CATEGORY_MENU,
        .parent_id = "main",
        .can_go_back = true,
        .lazy_load = true,
        .create_fn = menu_screen_create,
    };
    return screen_register(&config);
}
```

### Пример 2: Экран с параметрами

```c
typedef struct {
    int sensor_index;
    const char *sensor_name;
} sensor_detail_params_t;

static lv_obj_t* sensor_detail_create(void *params) {
    sensor_detail_params_t *p = (sensor_detail_params_t*)params;
    
    // Используем шаблон детализации
    template_detail_config_t detail_cfg = {
        .title = p->sensor_name,
        .description = "Sensor description here",
        .current_value = 6.8f,
        .target_value = 7.0f,
        .unit = "pH",
        .decimals = 2,
    };
    
    screen_instance_t *inst = screen_get_by_id("sensor_detail");
    return template_create_detail_screen(&detail_cfg, inst->encoder_group);
}

// Использование с параметрами
sensor_detail_params_t params = {
    .sensor_index = 0,
    .sensor_name = "pH Sensor",
};
screen_show("sensor_detail", &params);
```

### Пример 3: Динамическое обновление

```c
// Callback для обновления данных
static esp_err_t sensor_detail_on_update(lv_obj_t *screen_obj, void *data) {
    float *new_value = (float*)data;
    
    // Обновляем лейбл значения
    // (в реальности нужно сохранить ссылку на лейбл)
    
    ESP_LOGI(TAG, "Updated sensor value to %.2f", *new_value);
    return ESP_OK;
}

// При регистрации
screen_config_t config = {
    .id = "sensor_detail",
    .on_update = sensor_detail_on_update,  // <-- Callback для обновления
    // ...
};

// Позже обновляем без пересоздания экрана
float new_value = 7.2f;
screen_update("sensor_detail", &new_value);
```

---

## 🎨 Использование виджетов

### Back Button

```c
#include "widgets/back_button.h"

// Создать кнопку назад
lv_obj_t *btn = widget_create_back_button(parent, my_callback, user_data);

// Добавить в группу энкодера
widget_back_button_add_to_group(btn, encoder_group);
```

### Status Bar

```c
#include "widgets/status_bar.h"

// Создать статус-бар
lv_obj_t *bar = widget_create_status_bar(parent, "My Screen");

// Обновить заголовок
widget_status_bar_set_title(bar, "New Title");
```

### Menu List

```c
#include "widgets/menu_list.h"

menu_item_config_t items[] = {
    { .text = "Item 1", .callback = on_item1_click },
    { .text = "Item 2", .callback = on_item2_click },
};

lv_obj_t *list = widget_create_menu_list(parent, items, 2, encoder_group);
```

### Sensor Card

```c
#include "widgets/sensor_card.h"

sensor_card_config_t card_cfg = {
    .name = "pH",
    .unit = "",
    .current_value = 6.8f,
    .decimals = 2,
    .on_click = on_card_click,
};

lv_obj_t *card = widget_create_sensor_card(parent, &card_cfg);

// Обновить значение
widget_sensor_card_update_value(card, 7.0f);
```

---

## 📊 Жизненный цикл экрана

```
Регистрация (screen_register)
    ↓
[lazy_load=true] → Ожидание первого показа
    ↓
Создание (screen_create_instance)
    ↓
Показ (screen_show_instance)
    ↓
Использование
    ↓
Скрытие (screen_hide_instance)
    ↓
[cache_on_hide=true] → Кэш
[destroy_on_hide=true] → Уничтожение
```

---

## ⚙️ Конфигурация

### Стратегии управления памятью

#### 1. Часто используемые экраны (Main, Detail)
```c
.lazy_load = false,         // Создать сразу
.cache_on_hide = true,      // Кэшировать
.destroy_on_hide = false,
```

**Результат:** Быстрые переходы, но занимает память

#### 2. Редко используемые экраны (Settings, Info)
```c
.lazy_load = true,          // Создать при первом показе
.cache_on_hide = false,
.destroy_on_hide = true,    // Уничтожить при скрытии
```

**Результат:** Экономия памяти, небольшая задержка при открытии

#### 3. Средние по частоте (System меню)
```c
.lazy_load = true,
.cache_on_hide = true,      // Кэшировать после первого показа
.destroy_on_hide = false,
.cache_timeout_ms = 60000,  // Таймаут кэша 60 секунд
```

**Результат:** Баланс памяти и производительности

---

## 🔍 Отладка

### Логирование

Screen Manager подробно логирует все операции:

```
I (123) SCREEN_MANAGER: Initializing Screen Manager
I (124) SCREEN_REGISTRY: Registry initialized (max screens: 40, max instances: 15)
I (125) SCREEN_REGISTRY: Registered screen 'main' (category: 0, lazy_load: 0)
I (126) SCREEN_LIFECYCLE: Creating screen 'main'...
I (127) SCREEN_LIFECYCLE: Created screen 'main' (1/15 instances active)
I (128) SCREEN_LIFECYCLE: Showing screen 'main'...
I (129) NAVIGATOR: Navigating to 'detail_ph'
I (130) NAVIGATOR: Pushed 'main' to history (count: 1/10)
```

### Проверка состояния

```c
// Текущий экран
screen_instance_t *current = screen_get_current();
if (current) {
    ESP_LOGI(TAG, "Current: %s", current->config->id);
    ESP_LOGI(TAG, "Category: %d", current->config->category);
    ESP_LOGI(TAG, "Visible: %d", current->is_visible);
}

// Количество активных экземпляров
uint8_t instances = screen_get_instance_count();
ESP_LOGI(TAG, "Active instances: %d/%d", instances, MAX_INSTANCES);

// История
uint8_t history = screen_get_history_count();
ESP_LOGI(TAG, "History size: %d/%d", history, MAX_HISTORY);
```

---

## 🚀 Следующие шаги

1. ✅ **Неделя 1 завершена** - Core система готова
2. 🔄 **Неделя 2** - Миграция экранов
   - День 7: Главный экран
   - Дни 8-10: Экраны датчиков
3. 📋 **Неделя 3** - Системные экраны и тесты

---

## 📝 Дополнительные документы

- [Полный план рефакторинга](../../../NAVIGATION_REFACTORING_PLAN.md)
- [Детальный план имплементации](../../../FULL_ARCHITECTURE_IMPLEMENTATION_PLAN.md)
- [Чек-лист задач](../../../IMPLEMENTATION_CHECKLIST.md)

---

## 🎓 Best Practices

### DO ✅

- Используйте строковые ID вместо enum для экранов
- Регистрируйте экраны при инициализации приложения
- Используйте lazy_load для редко используемых экранов
- Переиспользуйте виджеты вместо дублирования кода
- Используйте шаблоны для типовых экранов
- Добавляйте кнопки в encoder_group в функции create

### DON'T ❌

- Не создавайте экраны вручную без регистрации
- Не используйте глобальные переменные для экранов
- Не дублируйте код создания UI
- Не забывайте про parent_id для навигации
- Не уничтожайте текущий видимый экран

---

**Статус:** ✅ Core завершен, готов к использованию  
**Следующий шаг:** Миграция экранов  
**Автор:** Hydroponics Monitor Team

