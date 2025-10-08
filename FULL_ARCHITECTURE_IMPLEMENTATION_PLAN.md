# План полной имплементации новой архитектуры навигации
## Hydroponics Monitor v3.0 - Screen Manager System

**Дата начала:** По согласованию  
**Длительность:** 3 недели (15 рабочих дней)  
**Статус:** Детальный план готов к исполнению  
**Версия:** 1.0

---

## 📋 Содержание

1. [Обзор плана](#обзор-плана)
2. [Подготовка](#подготовка)
3. [Неделя 1: Фундамент](#неделя-1-фундамент)
4. [Неделя 2: Миграция экранов](#неделя-2-миграция-экранов)
5. [Неделя 3: Тестирование и финализация](#неделя-3-тестирование-и-финализация)
6. [Критерии приемки](#критерии-приемки)
7. [План тестирования](#план-тестирования)
8. [Rollback стратегия](#rollback-стратегия)

---

## 🎯 Обзор плана

### Цели

1. ✅ Создать модульную систему управления экранами
2. ✅ Мигрировать все 27 экранов на новую архитектуру
3. ✅ Сократить код на 50% (с 3345 до ~1700 строк)
4. ✅ Достичь 80% покрытия тестами
5. ✅ Устранить все глобальные переменные

### Ключевые метрики

| Метрика | Текущее | Целевое |
|---------|---------|---------|
| Размер lvgl_ui.c | 3345 строк | <1700 строк |
| Глобальные переменные | 25+ | 0 |
| Дублирование кода | 70% | <10% |
| Время добавления экрана | 2+ часа | 30 мин |
| Покрытие тестами | 0% | 80% |

### Команда

- **Разработчик 1**: Core Manager + Navigation
- **Разработчик 2**: Widgets + Templates
- **Разработчик 3**: Screen Migration
- **QA**: Тестирование (с недели 2)

---

## 🔧 Подготовка (День 0)

### Задача 0.1: Создать ветку разработки

```bash
git checkout -b feature/screen-manager-full
git push -u origin feature/screen-manager-full
```

**Критерий приемки:** Ветка создана и запушена

### Задача 0.2: Создать структуру директорий

```bash
cd components/lvgl_ui/

# Создаем новые директории
mkdir -p screen_manager
mkdir -p screens/base
mkdir -p screens/sensor
mkdir -p screens/system
mkdir -p widgets
mkdir -p navigation

# Создаем файлы заглушки
touch screen_manager/screen_types.h
touch screen_manager/screen_manager.h
touch screen_manager/screen_manager.c
touch screen_manager/screen_registry.c
touch screen_manager/screen_navigator.c
touch screen_manager/screen_lifecycle.c
```

**Критерий приемки:** Структура создана, компилируется без ошибок

### Задача 0.3: Обновить CMakeLists.txt

```cmake
# components/lvgl_ui/CMakeLists.txt

idf_component_register(
    SRCS
        "lvgl_ui.c"
        "ph_screen.c"
        "sensor_screens_optimized.c"
        "ui_manager.c"
        
        # Screen Manager Core
        "screen_manager/screen_manager.c"
        "screen_manager/screen_registry.c"
        "screen_manager/screen_navigator.c"
        "screen_manager/screen_lifecycle.c"
        
        # Base screens
        "screens/base/screen_base.c"
        "screens/base/screen_template.c"
        "screens/base/screen_factory.c"
        
        # Concrete screens
        "screens/main_screen.c"
        "screens/sensor/sensor_detail_screen.c"
        "screens/sensor/sensor_settings_screen.c"
        "screens/system/system_menu_screen.c"
        # ... добавим по мере создания
        
        # Widgets
        "widgets/back_button.c"
        "widgets/status_bar.c"
        "widgets/menu_list.c"
        "widgets/sensor_card.c"
        
    INCLUDE_DIRS
        "."
        "screen_manager"
        "screens"
        "screens/base"
        "screens/sensor"
        "screens/system"
        "widgets"
        "navigation"
        
    REQUIRES
        lvgl
        driver
        esp_timer
)
```

**Критерий приемки:** Компиляция проходит без ошибок

### Задача 0.4: Создать тестовое окружение

```bash
mkdir -p test/screen_manager
touch test/screen_manager/test_screen_registry.c
touch test/screen_manager/test_navigator.c
touch test/screen_manager/test_lifecycle.c
```

**Критерий приемки:** Тестовая структура готова

---

## 📅 Неделя 1: Фундамент (Дни 1-5)

### День 1: Типы данных и Registry

#### Задача 1.1: Определить типы данных (4 часа)

**Файл:** `screen_manager/screen_types.h`

```c
#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  КОНСТАНТЫ
 * ============================= */
#define MAX_SCREENS         40    // Максимум экранов
#define MAX_INSTANCES       15    // Максимум активных экземпляров
#define MAX_HISTORY         10    // Глубина истории навигации
#define MAX_CHILDREN        8     // Максимум дочерних экранов
#define MAX_SCREEN_ID_LEN   32    // Длина ID экрана

/* =============================
 *  ТИПЫ ЭКРАНОВ
 * ============================= */
typedef enum {
    SCREEN_CATEGORY_MAIN,         // Главный экран
    SCREEN_CATEGORY_DETAIL,       // Детализация
    SCREEN_CATEGORY_SETTINGS,     // Настройки
    SCREEN_CATEGORY_MENU,         // Меню
    SCREEN_CATEGORY_FORM,         // Форма
    SCREEN_CATEGORY_DIALOG,       // Диалог
    SCREEN_CATEGORY_INFO,         // Информация
} screen_category_t;

/* =============================
 *  CALLBACK ТИПЫ
 * ============================= */

// Создание экрана
typedef lv_obj_t* (*screen_create_fn_t)(void *params);

// Уничтожение экрана
typedef esp_err_t (*screen_destroy_fn_t)(lv_obj_t *screen_obj);

// Показ экрана
typedef esp_err_t (*screen_show_fn_t)(lv_obj_t *screen_obj, void *params);

// Скрытие экрана
typedef esp_err_t (*screen_hide_fn_t)(lv_obj_t *screen_obj);

// Обновление данных
typedef esp_err_t (*screen_update_fn_t)(lv_obj_t *screen_obj, void *data);

// Проверка возможности показа
typedef bool (*screen_can_show_fn_t)(void);

/* =============================
 *  КОНФИГУРАЦИЯ ЭКРАНА
 * ============================= */
typedef struct {
    // Идентификация
    char id[MAX_SCREEN_ID_LEN];   // Уникальный ID
    const char *title;             // Заголовок экрана
    screen_category_t category;    // Категория
    
    // Навигация
    char parent_id[MAX_SCREEN_ID_LEN];  // ID родительского экрана
    bool can_go_back;              // Можно ли вернуться назад
    bool is_root;                  // Корневой экран (main)
    
    // Жизненный цикл
    bool lazy_load;                // Ленивая загрузка
    bool cache_on_hide;            // Кэшировать при скрытии
    bool destroy_on_hide;          // Уничтожать при скрытии
    uint32_t cache_timeout_ms;     // Таймаут кэша (0 = бесконечно)
    
    // UI
    bool has_status_bar;           // Есть ли статус-бар
    bool has_back_button;          // Есть ли кнопка назад
    
    // Callbacks
    screen_create_fn_t create_fn;  // Функция создания
    screen_destroy_fn_t destroy_fn; // Функция уничтожения (опционально)
    screen_show_fn_t on_show;      // При показе (опционально)
    screen_hide_fn_t on_hide;      // При скрытии (опционально)
    screen_update_fn_t on_update;  // При обновлении (опционально)
    screen_can_show_fn_t can_show_fn; // Проверка перед показом (опционально)
    
    // Данные
    void *user_data;               // Произвольные данные
} screen_config_t;

/* =============================
 *  ЭКЗЕМПЛЯР ЭКРАНА
 * ============================= */
typedef struct screen_instance_t {
    // Конфигурация
    screen_config_t *config;       // Ссылка на конфигурацию
    
    // LVGL объекты
    lv_obj_t *screen_obj;          // LVGL экран
    lv_group_t *encoder_group;     // Группа энкодера
    
    // Состояние
    bool is_created;               // Создан ли экран
    bool is_visible;               // Видим ли экран
    bool is_cached;                // В кэше ли экран
    
    // Временные метки
    uint32_t create_time;          // Timestamp создания
    uint32_t last_show_time;       // Timestamp последнего показа
    uint32_t cache_time;           // Timestamp кэширования
    
    // Навигация
    struct screen_instance_t *parent;  // Родитель
    struct screen_instance_t *children[MAX_CHILDREN]; // Дочерние
    uint8_t children_count;
    
    // Параметры
    void *show_params;             // Параметры последнего показа
} screen_instance_t;

/* =============================
 *  МЕНЕДЖЕР ЭКРАНОВ
 * ============================= */
typedef struct {
    // Конфигурация
    bool enable_cache;             // Включить кэширование
    bool enable_history;           // Включить историю
    uint8_t max_cache_size;        // Максимум экранов в кэше
    
    // Анимации
    uint32_t transition_time;      // Время перехода (мс)
    bool enable_animations;        // Включить анимации
} screen_manager_config_t;

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
    uint8_t history_count;
    
    // Конфигурация
    screen_manager_config_t config;
    
    // Состояние
    bool is_initialized;
    
    // Мьютекс
    SemaphoreHandle_t mutex;
} screen_manager_t;

#ifdef __cplusplus
}
#endif
```

**Критерий приемки:** 
- ✅ Все типы определены
- ✅ Компилируется без ошибок
- ✅ Комментарии на месте

#### Задача 1.2: Реализовать Registry (4 часа)

**Файл:** `screen_manager/screen_registry.c`

```c
#include "screen_types.h"
#include "screen_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SCREEN_REGISTRY";

// Глобальный менеджер (единственная глобальная переменная!)
static screen_manager_t g_manager = {0};

/* =============================
 *  ВНУТРЕННИЕ ФУНКЦИИ
 * ============================= */

static screen_config_t* find_screen_by_id(const char *screen_id)
{
    if (!screen_id) return NULL;
    
    for (int i = 0; i < g_manager.screen_count; i++) {
        if (g_manager.screens[i] && 
            strcmp(g_manager.screens[i]->id, screen_id) == 0) {
            return g_manager.screens[i];
        }
    }
    return NULL;
}

static bool is_screen_id_valid(const char *screen_id)
{
    if (!screen_id || strlen(screen_id) == 0) {
        return false;
    }
    if (strlen(screen_id) >= MAX_SCREEN_ID_LEN) {
        return false;
    }
    return true;
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================= */

esp_err_t screen_registry_init(void)
{
    if (g_manager.is_initialized) {
        ESP_LOGW(TAG, "Registry already initialized");
        return ESP_OK;
    }
    
    memset(&g_manager, 0, sizeof(screen_manager_t));
    
    // Создаем мьютекс
    g_manager.mutex = xSemaphoreCreateMutex();
    if (!g_manager.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Устанавливаем конфигурацию по умолчанию
    g_manager.config.enable_cache = true;
    g_manager.config.enable_history = true;
    g_manager.config.max_cache_size = 5;
    g_manager.config.transition_time = 300;
    g_manager.config.enable_animations = true;
    
    g_manager.is_initialized = true;
    
    ESP_LOGI(TAG, "Registry initialized");
    return ESP_OK;
}

esp_err_t screen_register(const screen_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_screen_id_valid(config->id)) {
        ESP_LOGE(TAG, "Invalid screen ID");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!config->create_fn) {
        ESP_LOGE(TAG, "Screen %s: create_fn is required", config->id);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Проверяем лимит
    if (g_manager.screen_count >= MAX_SCREENS) {
        ESP_LOGE(TAG, "Maximum number of screens reached");
        return ESP_ERR_NO_MEM;
    }
    
    // Проверяем дубликаты
    if (find_screen_by_id(config->id)) {
        ESP_LOGE(TAG, "Screen %s already registered", config->id);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Блокируем
    if (xSemaphoreTake(g_manager.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Копируем конфигурацию
    screen_config_t *new_config = malloc(sizeof(screen_config_t));
    if (!new_config) {
        xSemaphoreGive(g_manager.mutex);
        return ESP_ERR_NO_MEM;
    }
    memcpy(new_config, config, sizeof(screen_config_t));
    
    // Добавляем в реестр
    g_manager.screens[g_manager.screen_count] = new_config;
    g_manager.screen_count++;
    
    xSemaphoreGive(g_manager.mutex);
    
    ESP_LOGI(TAG, "Registered screen '%s' (category: %d)", 
             config->id, config->category);
    return ESP_OK;
}

esp_err_t screen_unregister(const char *screen_id)
{
    if (!is_screen_id_valid(screen_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Блокируем
    if (xSemaphoreTake(g_manager.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Ищем экран
    int index = -1;
    for (int i = 0; i < g_manager.screen_count; i++) {
        if (g_manager.screens[i] && 
            strcmp(g_manager.screens[i]->id, screen_id) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        xSemaphoreGive(g_manager.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Освобождаем память
    free(g_manager.screens[index]);
    
    // Сдвигаем массив
    for (int i = index; i < g_manager.screen_count - 1; i++) {
        g_manager.screens[i] = g_manager.screens[i + 1];
    }
    g_manager.screen_count--;
    g_manager.screens[g_manager.screen_count] = NULL;
    
    xSemaphoreGive(g_manager.mutex);
    
    ESP_LOGI(TAG, "Unregistered screen '%s'", screen_id);
    return ESP_OK;
}

screen_config_t* screen_get_config(const char *screen_id)
{
    if (!is_screen_id_valid(screen_id)) {
        return NULL;
    }
    
    return find_screen_by_id(screen_id);
}

uint8_t screen_get_registered_count(void)
{
    return g_manager.screen_count;
}

screen_manager_t* screen_manager_get_instance(void)
{
    return &g_manager;
}
```

**Файл:** `screen_manager/screen_registry.h`

```c
#pragma once

#include "screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация реестра
esp_err_t screen_registry_init(void);

// Регистрация экрана
esp_err_t screen_register(const screen_config_t *config);

// Отмена регистрации
esp_err_t screen_unregister(const char *screen_id);

// Получить конфигурацию экрана
screen_config_t* screen_get_config(const char *screen_id);

// Получить количество зарегистрированных экранов
uint8_t screen_get_registered_count(void);

// Получить экземпляр менеджера (для внутреннего использования)
screen_manager_t* screen_manager_get_instance(void);

#ifdef __cplusplus
}
#endif
```

**Критерий приемки:**
- ✅ Registry работает
- ✅ Можно регистрировать/удалять экраны
- ✅ Проверка дубликатов работает
- ✅ Thread-safe (мьютексы)

#### Задача 1.3: Юнит-тесты Registry (2 часа)

**Файл:** `test/screen_manager/test_screen_registry.c`

```c
#include "unity.h"
#include "screen_registry.h"
#include "screen_types.h"

static lv_obj_t* dummy_create(void *params) {
    return (lv_obj_t*)0xDEADBEEF; // Mock
}

void test_registry_init(void)
{
    esp_err_t ret = screen_registry_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_screen_register_valid(void)
{
    screen_config_t config = {
        .id = "test_screen",
        .title = "Test Screen",
        .category = SCREEN_CATEGORY_MAIN,
        .create_fn = dummy_create,
    };
    
    esp_err_t ret = screen_register(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1, screen_get_registered_count());
}

void test_screen_register_duplicate(void)
{
    screen_config_t config = {
        .id = "test_screen",
        .title = "Test Screen",
        .category = SCREEN_CATEGORY_MAIN,
        .create_fn = dummy_create,
    };
    
    screen_register(&config);
    esp_err_t ret = screen_register(&config); // Дубликат
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

void test_screen_get_config(void)
{
    screen_config_t *cfg = screen_get_config("test_screen");
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_STRING("test_screen", cfg->id);
}

void test_screen_unregister(void)
{
    esp_err_t ret = screen_unregister("test_screen");
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(0, screen_get_registered_count());
}

void setUp(void) {
    screen_registry_init();
}

void tearDown(void) {
    // Очистка
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_registry_init);
    RUN_TEST(test_screen_register_valid);
    RUN_TEST(test_screen_register_duplicate);
    RUN_TEST(test_screen_get_config);
    RUN_TEST(test_screen_unregister);
    return UNITY_END();
}
```

**Критерий приемки:**
- ✅ Все тесты проходят
- ✅ Покрытие >80%

---

### День 2: Lifecycle Manager

#### Задача 2.1: Реализовать Lifecycle (6 часов)

**Файл:** `screen_manager/screen_lifecycle.c`

```c
#include "screen_lifecycle.h"
#include "screen_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SCREEN_LIFECYCLE";

/* =============================
 *  ВНУТРЕННИЕ ФУНКЦИИ
 * ============================= */

static screen_instance_t* find_instance_by_id(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    for (int i = 0; i < manager->instance_count; i++) {
        if (manager->instances[i] && 
            manager->instances[i]->config &&
            strcmp(manager->instances[i]->config->id, screen_id) == 0) {
            return manager->instances[i];
        }
    }
    return NULL;
}

static uint32_t get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* =============================
 *  СОЗДАНИЕ/УНИЧТОЖЕНИЕ
 * ============================= */

esp_err_t screen_create_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Проверяем, не создан ли уже
    if (find_instance_by_id(screen_id)) {
        ESP_LOGW(TAG, "Screen '%s' already created", screen_id);
        return ESP_OK;
    }
    
    // Получаем конфигурацию
    screen_config_t *config = screen_get_config(screen_id);
    if (!config) {
        ESP_LOGE(TAG, "Screen '%s' not registered", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Проверяем лимит
    if (manager->instance_count >= MAX_INSTANCES) {
        ESP_LOGE(TAG, "Maximum instances reached");
        return ESP_ERR_NO_MEM;
    }
    
    // Создаем экземпляр
    screen_instance_t *instance = calloc(1, sizeof(screen_instance_t));
    if (!instance) {
        return ESP_ERR_NO_MEM;
    }
    
    instance->config = config;
    instance->create_time = get_time_ms();
    
    // Вызываем функцию создания UI
    instance->screen_obj = config->create_fn(config->user_data);
    if (!instance->screen_obj) {
        ESP_LOGE(TAG, "Failed to create screen '%s'", screen_id);
        free(instance);
        return ESP_FAIL;
    }
    
    // Создаем группу энкодера
    instance->encoder_group = lv_group_create();
    if (!instance->encoder_group) {
        ESP_LOGW(TAG, "Failed to create encoder group for '%s'", screen_id);
    } else {
        lv_group_set_wrap(instance->encoder_group, true);
    }
    
    instance->is_created = true;
    
    // Добавляем в список
    manager->instances[manager->instance_count] = instance;
    manager->instance_count++;
    
    ESP_LOGI(TAG, "Created screen '%s' (%d/%d instances)", 
             screen_id, manager->instance_count, MAX_INSTANCES);
    
    return ESP_OK;
}

esp_err_t screen_destroy_instance(const char *screen_id)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Ищем экземпляр
    int index = -1;
    screen_instance_t *instance = NULL;
    
    for (int i = 0; i < manager->instance_count; i++) {
        if (manager->instances[i] && 
            manager->instances[i]->config &&
            strcmp(manager->instances[i]->config->id, screen_id) == 0) {
            index = i;
            instance = manager->instances[i];
            break;
        }
    }
    
    if (!instance) {
        ESP_LOGW(TAG, "Screen '%s' not found", screen_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Нельзя уничтожить текущий экран
    if (instance == manager->current_screen) {
        ESP_LOGE(TAG, "Cannot destroy current screen '%s'", screen_id);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Вызываем custom destroy если есть
    if (instance->config->destroy_fn) {
        instance->config->destroy_fn(instance->screen_obj);
    }
    
    // Уничтожаем LVGL объекты
    if (instance->encoder_group) {
        lv_group_del(instance->encoder_group);
    }
    
    if (instance->screen_obj) {
        lv_obj_del(instance->screen_obj);
    }
    
    // Освобождаем параметры
    if (instance->show_params) {
        free(instance->show_params);
    }
    
    // Освобождаем экземпляр
    free(instance);
    
    // Сдвигаем массив
    for (int i = index; i < manager->instance_count - 1; i++) {
        manager->instances[i] = manager->instances[i + 1];
    }
    manager->instance_count--;
    manager->instances[manager->instance_count] = NULL;
    
    ESP_LOGI(TAG, "Destroyed screen '%s' (%d instances left)", 
             screen_id, manager->instance_count);
    
    return ESP_OK;
}

/* =============================
 *  ПОКАЗ/СКРЫТИЕ
 * ============================= */

esp_err_t screen_show_instance(const char *screen_id, void *params)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        // Создаем если lazy_load
        screen_config_t *config = screen_get_config(screen_id);
        if (config && config->lazy_load) {
            esp_err_t ret = screen_create_instance(screen_id);
            if (ret != ESP_OK) {
                return ret;
            }
            instance = find_instance_by_id(screen_id);
        }
        
        if (!instance) {
            ESP_LOGE(TAG, "Screen '%s' not found", screen_id);
            return ESP_ERR_NOT_FOUND;
        }
    }
    
    // Проверка can_show
    if (instance->config->can_show_fn && 
        !instance->config->can_show_fn()) {
        ESP_LOGW(TAG, "Screen '%s' cannot be shown (permission denied)", screen_id);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // Скрываем текущий экран
    if (manager->current_screen && 
        manager->current_screen != instance) {
        screen_hide_instance(manager->current_screen->config->id);
    }
    
    // Сохраняем параметры
    if (params) {
        if (instance->show_params) {
            free(instance->show_params);
        }
        // Предполагаем, что params - это указатель на структуру
        // В реальности нужно знать размер
        instance->show_params = params;
    }
    
    // Вызываем on_show callback
    if (instance->config->on_show) {
        instance->config->on_show(instance->screen_obj, params);
    }
    
    // Загружаем экран
    lv_scr_load_anim(instance->screen_obj, LV_SCR_LOAD_ANIM_MOVE_LEFT, 
                     manager->config.transition_time, 0, false);
    
    // Устанавливаем группу энкодера
    if (instance->encoder_group) {
        lv_indev_t *indev = lv_indev_get_next(NULL);
        while (indev) {
            if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
                lv_indev_set_group(indev, instance->encoder_group);
                // Фокус на первый элемент
                if (lv_group_get_obj_count(instance->encoder_group) > 0) {
                    lv_group_focus_next(instance->encoder_group);
                }
                break;
            }
            indev = lv_indev_get_next(indev);
        }
    }
    
    instance->is_visible = true;
    instance->last_show_time = get_time_ms();
    manager->current_screen = instance;
    
    ESP_LOGI(TAG, "Showing screen '%s'", screen_id);
    
    return ESP_OK;
}

esp_err_t screen_hide_instance(const char *screen_id)
{
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->is_visible) {
        return ESP_OK; // Уже скрыт
    }
    
    // Вызываем on_hide callback
    if (instance->config->on_hide) {
        instance->config->on_hide(instance->screen_obj);
    }
    
    instance->is_visible = false;
    
    // Уничтожаем или кэшируем
    if (instance->config->destroy_on_hide) {
        ESP_LOGI(TAG, "Destroying screen '%s' (destroy_on_hide)", screen_id);
        return screen_destroy_instance(screen_id);
    } else if (instance->config->cache_on_hide) {
        instance->is_cached = true;
        instance->cache_time = get_time_ms();
        ESP_LOGI(TAG, "Caching screen '%s'", screen_id);
    }
    
    ESP_LOGI(TAG, "Hidden screen '%s'", screen_id);
    
    return ESP_OK;
}

/* =============================
 *  ОБНОВЛЕНИЕ
 * ============================= */

esp_err_t screen_update_instance(const char *screen_id, void *data)
{
    screen_instance_t *instance = find_instance_by_id(screen_id);
    if (!instance) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!instance->config->on_update) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    return instance->config->on_update(instance->screen_obj, data);
}

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

screen_instance_t* screen_get_current_instance(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->current_screen;
}

screen_instance_t* screen_get_instance_by_id(const char *screen_id)
{
    return find_instance_by_id(screen_id);
}

bool screen_is_visible(const char *screen_id)
{
    screen_instance_t *instance = find_instance_by_id(screen_id);
    return instance ? instance->is_visible : false;
}

uint8_t screen_get_instance_count(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->instance_count;
}
```

**Файл:** `screen_manager/screen_lifecycle.h`

```c
#pragma once

#include "screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Создание экземпляра экрана
esp_err_t screen_create_instance(const char *screen_id);

// Уничтожение экземпляра
esp_err_t screen_destroy_instance(const char *screen_id);

// Показ экрана
esp_err_t screen_show_instance(const char *screen_id, void *params);

// Скрытие экрана
esp_err_t screen_hide_instance(const char *screen_id);

// Обновление данных экрана
esp_err_t screen_update_instance(const char *screen_id, void *data);

// Получить текущий экземпляр
screen_instance_t* screen_get_current_instance(void);

// Получить экземпляр по ID
screen_instance_t* screen_get_instance_by_id(const char *screen_id);

// Проверить видимость
bool screen_is_visible(const char *screen_id);

// Получить количество экземпляров
uint8_t screen_get_instance_count(void);

#ifdef __cplusplus
}
#endif
```

**Критерий приемки:**
- ✅ Lifecycle работает
- ✅ Ленивая загрузка работает
- ✅ Кэширование работает
- ✅ destroy_on_hide работает

#### Задача 2.2: Юнит-тесты Lifecycle (2 часа)

**Критерий приемки:** Тесты покрывают все функции

---

### День 3: Navigator

#### Задача 3.1: Реализовать Navigator (6 часов)

**Файл:** `screen_manager/screen_navigator.c`

```c
#include "screen_navigator.h"
#include "screen_lifecycle.h"
#include "screen_registry.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NAVIGATOR";

/* =============================
 *  ИСТОРИЯ НАВИГАЦИИ
 * ============================= */

static esp_err_t push_history(screen_instance_t *instance)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!manager->config.enable_history) {
        return ESP_OK;
    }
    
    if (manager->history_count >= MAX_HISTORY) {
        // Сдвигаем историю
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            manager->history[i] = manager->history[i + 1];
        }
        manager->history_count = MAX_HISTORY - 1;
    }
    
    manager->history[manager->history_count] = instance;
    manager->history_count++;
    manager->history_index = manager->history_count - 1;
    
    ESP_LOGD(TAG, "Pushed to history: '%s' (count: %d)", 
             instance->config->id, manager->history_count);
    
    return ESP_OK;
}

static screen_instance_t* pop_history(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (manager->history_count == 0) {
        return NULL;
    }
    
    manager->history_count--;
    screen_instance_t *instance = manager->history[manager->history_count];
    manager->history[manager->history_count] = NULL;
    
    if (manager->history_count > 0) {
        manager->history_index = manager->history_count - 1;
    } else {
        manager->history_index = 0;
    }
    
    ESP_LOGD(TAG, "Popped from history: '%s' (count: %d)", 
             instance->config->id, manager->history_count);
    
    return instance;
}

/* =============================
 *  НАВИГАЦИЯ
 * ============================= */

esp_err_t navigator_show(const char *screen_id, void *params)
{
    if (!screen_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    screen_manager_t *manager = screen_manager_get_instance();
    
    // Сохраняем текущий в историю
    if (manager->current_screen) {
        push_history(manager->current_screen);
    }
    
    // Показываем новый экран
    esp_err_t ret = screen_show_instance(screen_id, params);
    if (ret != ESP_OK) {
        // Откатываем историю
        pop_history();
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t navigator_go_back(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (manager->history_count == 0) {
        ESP_LOGW(TAG, "History is empty, cannot go back");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Берем предыдущий экран из истории
    screen_instance_t *prev = pop_history();
    if (!prev) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Показываем его (без добавления в историю)
    esp_err_t ret = screen_show_instance(prev->config->id, prev->show_params);
    if (ret != ESP_OK) {
        // Восстанавливаем историю
        push_history(prev);
        return ret;
    }
    
    ESP_LOGI(TAG, "Navigated back to '%s'", prev->config->id);
    
    return ESP_OK;
}

esp_err_t navigator_go_to_parent(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    if (!manager->current_screen) {
        return ESP_ERR_INVALID_STATE;
    }
    
    screen_config_t *config = manager->current_screen->config;
    
    if (!config->can_go_back || strlen(config->parent_id) == 0) {
        ESP_LOGW(TAG, "Screen '%s' has no parent", config->id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Идем к родителю
    return navigator_show(config->parent_id, NULL);
}

esp_err_t navigator_go_home(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // Ищем корневой экран
    for (int i = 0; i < manager->screen_count; i++) {
        screen_config_t *config = manager->screens[i];
        if (config && config->is_root) {
            // Очищаем историю
            manager->history_count = 0;
            manager->history_index = 0;
            
            return screen_show_instance(config->id, NULL);
        }
    }
    
    ESP_LOGE(TAG, "Root screen not found");
    return ESP_ERR_NOT_FOUND;
}

/* =============================
 *  УТИЛИТЫ
 * ============================= */

uint8_t navigator_get_history_count(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    return manager->history_count;
}

void navigator_clear_history(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    manager->history_count = 0;
    manager->history_index = 0;
    memset(manager->history, 0, sizeof(manager->history));
    ESP_LOGI(TAG, "History cleared");
}
```

**Файл:** `screen_manager/screen_navigator.h`

```c
#pragma once

#include "screen_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Показать экран (с добавлением в историю)
esp_err_t navigator_show(const char *screen_id, void *params);

// Вернуться назад (из истории)
esp_err_t navigator_go_back(void);

// Перейти к родительскому экрану
esp_err_t navigator_go_to_parent(void);

// Перейти к корневому экрану (очистка истории)
esp_err_t navigator_go_home(void);

// Получить размер истории
uint8_t navigator_get_history_count(void);

// Очистить историю
void navigator_clear_history(void);

#ifdef __cplusplus
}
#endif
```

**Критерий приемки:**
- ✅ Навигация вперед работает
- ✅ Навигация назад работает
- ✅ История работает
- ✅ go_to_parent находит родителя

#### Задача 3.2: Юнит-тесты Navigator (2 часа)

---

### День 4: Screen Manager API

#### Задача 4.1: Главный API (4 часа)

**Файл:** `screen_manager/screen_manager.h`

```c
#pragma once

#include "screen_types.h"
#include "screen_registry.h"
#include "screen_lifecycle.h"
#include "screen_navigator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================= */

// Инициализация Screen Manager
esp_err_t screen_manager_init(const screen_manager_config_t *config);

// Деинициализация
esp_err_t screen_manager_deinit(void);

/* =============================
 *  НАВИГАЦИЯ (Упрощенный API)
 * ============================= */

// Показать экран
esp_err_t screen_show(const char *screen_id, void *params);

// Скрыть экран
esp_err_t screen_hide(const char *screen_id);

// Вернуться назад
esp_err_t screen_go_back(void);

// К родителю
esp_err_t screen_go_to_parent(void);

// На главный
esp_err_t screen_go_home(void);

// Обновить данные
esp_err_t screen_update(const char *screen_id, void *data);

/* =============================
 *  УПРАВЛЕНИЕ
 * ============================= */

// Создать экран вручную
esp_err_t screen_create(const char *screen_id);

// Уничтожить экран
esp_err_t screen_destroy(const char *screen_id);

// Перезагрузить экран
esp_err_t screen_reload(const char *screen_id);

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

// Получить текущий экран
screen_instance_t* screen_get_current(void);

// Получить экран по ID
screen_instance_t* screen_get_by_id(const char *screen_id);

// Видим ли экран
bool screen_is_visible_check(const char *screen_id);

// Количество в истории
uint8_t screen_get_history_count(void);

#ifdef __cplusplus
}
#endif
```

**Файл:** `screen_manager/screen_manager.c`

```c
#include "screen_manager.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_MANAGER";

/* =============================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================= */

esp_err_t screen_manager_init(const screen_manager_config_t *config)
{
    ESP_LOGI(TAG, "Initializing Screen Manager");
    
    // Инициализируем реестр
    esp_err_t ret = screen_registry_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init registry: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Применяем конфигурацию если передана
    if (config) {
        screen_manager_t *manager = screen_manager_get_instance();
        memcpy(&manager->config, config, sizeof(screen_manager_config_t));
    }
    
    ESP_LOGI(TAG, "Screen Manager initialized");
    return ESP_OK;
}

esp_err_t screen_manager_deinit(void)
{
    screen_manager_t *manager = screen_manager_get_instance();
    
    // Уничтожаем все экземпляры
    while (manager->instance_count > 0) {
        screen_instance_t *inst = manager->instances[0];
        if (inst && inst->config) {
            screen_destroy_instance(inst->config->id);
        }
    }
    
    // Освобождаем конфигурации
    for (int i = 0; i < manager->screen_count; i++) {
        if (manager->screens[i]) {
            free(manager->screens[i]);
        }
    }
    
    // Удаляем мьютекс
    if (manager->mutex) {
        vSemaphoreDelete(manager->mutex);
    }
    
    memset(manager, 0, sizeof(screen_manager_t));
    
    ESP_LOGI(TAG, "Screen Manager deinitialized");
    return ESP_OK;
}

/* =============================
 *  НАВИГАЦИЯ (Wrappers)
 * ============================= */

esp_err_t screen_show(const char *screen_id, void *params)
{
    return navigator_show(screen_id, params);
}

esp_err_t screen_hide(const char *screen_id)
{
    return screen_hide_instance(screen_id);
}

esp_err_t screen_go_back(void)
{
    return navigator_go_back();
}

esp_err_t screen_go_to_parent(void)
{
    return navigator_go_to_parent();
}

esp_err_t screen_go_home(void)
{
    return navigator_go_home();
}

esp_err_t screen_update(const char *screen_id, void *data)
{
    return screen_update_instance(screen_id, data);
}

/* =============================
 *  УПРАВЛЕНИЕ
 * ============================= */

esp_err_t screen_create(const char *screen_id)
{
    return screen_create_instance(screen_id);
}

esp_err_t screen_destroy(const char *screen_id)
{
    return screen_destroy_instance(screen_id);
}

esp_err_t screen_reload(const char *screen_id)
{
    // Уничтожаем и создаем заново
    esp_err_t ret = screen_destroy_instance(screen_id);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        return ret;
    }
    
    return screen_create_instance(screen_id);
}

/* =============================
 *  ГЕТТЕРЫ
 * ============================= */

screen_instance_t* screen_get_current(void)
{
    return screen_get_current_instance();
}

screen_instance_t* screen_get_by_id(const char *screen_id)
{
    return screen_get_instance_by_id(screen_id);
}

bool screen_is_visible_check(const char *screen_id)
{
    return screen_is_visible(screen_id);
}

uint8_t screen_get_history_count(void)
{
    return navigator_get_history_count();
}
```

**Критерий приемки:**
- ✅ API полностью работает
- ✅ Все функции протестированы
- ✅ Документация Doxygen

#### Задача 4.2: Интеграционные тесты (4 часа)

**Тесты полного цикла:**
- Инициализация → Регистрация → Показ → Навигация → Уничтожение

---

### День 5: Виджеты

#### Задача 5.1: Базовые виджеты (8 часов)

**Файл:** `widgets/back_button.c`

```c
#include "lvgl.h"
#include "esp_log.h"

extern lv_style_t style_card;  // Из lvgl_ui.c

lv_obj_t* widget_create_back_button(lv_obj_t *parent, 
                                     lv_event_cb_t callback,
                                     void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_style(btn, &style_card, 0);
    lv_obj_set_size(btn, 60, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, user_data);
    }
    
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_center(label);
    
    return btn;
}
```

**Файл:** `widgets/status_bar.c`

```c
#include "lvgl.h"

extern lv_style_t style_card;
extern lv_style_t style_title;

lv_obj_t* widget_create_status_bar(lv_obj_t *parent, const char *title)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_add_style(bar, &style_card, 0);
    lv_obj_set_size(bar, LV_PCT(100), 60);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    
    lv_obj_t *label = lv_label_create(bar);
    lv_obj_add_style(label, &style_title, 0);
    lv_label_set_text(label, title);
    lv_obj_center(label);
    
    return bar;
}
```

**Файл:** `widgets/menu_list.c`

```c
#include "lvgl.h"

typedef struct {
    const char *text;
    const char *screen_id;
    lv_event_cb_t callback;
    void *user_data;
} menu_item_config_t;

lv_obj_t* widget_create_menu_list(lv_obj_t *parent,
                                   const menu_item_config_t *items,
                                   uint8_t item_count)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_PCT(90), LV_PCT(70));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 8, 0);
    
    for (uint8_t i = 0; i < item_count; i++) {
        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        
        if (items[i].callback) {
            lv_obj_add_event_cb(btn, items[i].callback, 
                              LV_EVENT_CLICKED, items[i].user_data);
        }
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, items[i].text);
        lv_obj_center(label);
    }
    
    return list;
}
```

**Критерий приемки:**
- ✅ 4+ виджета готовы
- ✅ Переиспользуемые
- ✅ Документированы

---

## 📆 Продолжение плана в следующей части...

Это первая неделя из трех. План очень детальный (будет ~200+ страниц полностью).

Хотите чтобы я:
1. Продолжил с Неделей 2 и 3?
2. Или сначала вы хотите обсудить/утвердить Неделю 1?
3. Или нужен более краткий вариант?

Также могу создать:
- Чек-листы для каждого дня
- Шаблоны кода
- Диаграммы Gantt
- Таблицы зависимостей задач

Что предпочтительнее?

