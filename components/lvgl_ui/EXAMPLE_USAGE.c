/**
 * @file EXAMPLE_USAGE.c
 * @brief Пример использования Screen Manager System
 * 
 * Этот файл демонстрирует как использовать новую систему управления экранами.
 * НЕ КОМПИЛИРУЕТСЯ - только для справки!
 * 
 * @date 2025-10-08
 */

#include "screen_manager/screen_manager.h"
#include "screens/base/screen_base.h"
#include "screens/base/screen_template.h"
#include "widgets/back_button.h"
#include "widgets/status_bar.h"
#include "widgets/menu_list.h"
#include "widgets/sensor_card.h"

/* =============================
 *  ПРИМЕР 1: ПРОСТОЙ ЭКРАН
 * ============================= */

// Функция создания UI
static lv_obj_t* example_screen_create(void *params) {
    // Создаем базовый экран с заголовком и кнопкой назад
    screen_base_config_t cfg = {
        .title = "Example Screen",
        .has_status_bar = true,
        .has_back_button = true,
        .back_callback = NULL,  // NULL = автоматическая навигация назад
    };
    
    screen_base_t base = screen_base_create(&cfg);
    
    // Добавляем свой контент в base.content
    lv_obj_t *label = lv_label_create(base.content);
    lv_label_set_text(label, "Hello from Example Screen!");
    lv_obj_center(label);
    
    return base.screen;
}

// Регистрация экрана
void example_screen_register(void) {
    screen_config_t config = {
        .id = "example",               // Уникальный ID
        .title = "Example Screen",
        .category = SCREEN_CATEGORY_INFO,
        .parent_id = "main",           // Родитель - главный экран
        .can_go_back = true,
        .lazy_load = true,             // Создать при первом показе
        .create_fn = example_screen_create,
    };
    
    screen_register(&config);
}

/* =============================
 *  ПРИМЕР 2: ЭКРАН МЕНЮ
 * ============================= */

// Callbacks для пунктов меню
static void on_settings_click(lv_event_t *e) {
    screen_show("settings", NULL);
}

static void on_about_click(lv_event_t *e) {
    screen_show("about", NULL);
}

static void on_exit_click(lv_event_t *e) {
    screen_go_home();
}

// Функция создания меню
static lv_obj_t* menu_screen_create(void *params) {
    // Определяем пункты меню
    menu_item_config_t items[] = {
        {
            .text = "Settings",
            .icon = LV_SYMBOL_SETTINGS,
            .callback = on_settings_click,
            .user_data = NULL,
        },
        {
            .text = "About",
            .icon = LV_SYMBOL_HOME,
            .callback = on_about_click,
            .user_data = NULL,
        },
        {
            .text = "Exit",
            .icon = LV_SYMBOL_CLOSE,
            .callback = on_exit_click,
            .user_data = NULL,
        },
    };
    
    // Используем шаблон меню
    template_menu_config_t menu_cfg = {
        .title = "Main Menu",
        .items = items,
        .item_count = 3,
        .has_back_button = false,  // Главное меню, назад некуда
    };
    
    // Получаем encoder group для этого экрана
    screen_instance_t *inst = screen_get_by_id("menu");
    lv_group_t *group = inst ? inst->encoder_group : NULL;
    
    return template_create_menu_screen(&menu_cfg, group);
}

void menu_screen_register(void) {
    screen_config_t config = {
        .id = "menu",
        .title = "Main Menu",
        .category = SCREEN_CATEGORY_MENU,
        .is_root = false,
        .parent_id = "main",
        .can_go_back = true,
        .lazy_load = true,
        .create_fn = menu_screen_create,
    };
    
    screen_register(&config);
}

/* =============================
 *  ПРИМЕР 3: ЭКРАН С ПАРАМЕТРАМИ
 * ============================= */

// Параметры для экрана детализации датчика
typedef struct {
    int sensor_id;
    const char *sensor_name;
    float current_value;
    float target_value;
} sensor_detail_params_t;

// Функция создания с использованием параметров
static lv_obj_t* sensor_detail_create(void *params) {
    sensor_detail_params_t *p = (sensor_detail_params_t*)params;
    
    // Используем шаблон детализации
    template_detail_config_t detail_cfg = {
        .title = p->sensor_name,
        .description = "Sensor monitoring screen",
        .current_value = p->current_value,
        .target_value = p->target_value,
        .unit = "pH",
        .decimals = 2,
        .settings_callback = NULL,
        .back_callback = NULL,
    };
    
    screen_instance_t *inst = screen_get_by_id("sensor_detail");
    return template_create_detail_screen(&detail_cfg, inst->encoder_group);
}

void sensor_detail_register(void) {
    screen_config_t config = {
        .id = "sensor_detail",
        .title = "Sensor Detail",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "main",
        .can_go_back = true,
        .lazy_load = true,
        .create_fn = sensor_detail_create,
    };
    
    screen_register(&config);
}

// Использование с параметрами
void show_sensor_detail(int sensor_id, float value) {
    sensor_detail_params_t *params = malloc(sizeof(sensor_detail_params_t));
    params->sensor_id = sensor_id;
    params->sensor_name = "pH Sensor";
    params->current_value = value;
    params->target_value = 7.0f;
    
    screen_show("sensor_detail", params);  // Ownership переходит к экрану
}

/* =============================
 *  ПРИМЕР 4: ДИНАМИЧЕСКОЕ ОБНОВЛЕНИЕ
 * ============================= */

// Callback для обновления данных без пересоздания экрана
static esp_err_t sensor_detail_on_update(lv_obj_t *screen_obj, void *data) {
    float *new_value = (float*)data;
    
    // Обновляем UI (в реальности нужно сохранить ссылки на лейблы)
    // lv_label_set_text_fmt(current_value_label, "%.2f", *new_value);
    
    ESP_LOGI("SENSOR", "Updated value to %.2f", *new_value);
    return ESP_OK;
}

void sensor_detail_with_update_register(void) {
    screen_config_t config = {
        .id = "sensor_detail",
        .title = "Sensor Detail",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "main",
        .lazy_load = true,
        .create_fn = sensor_detail_create,
        .on_update = sensor_detail_on_update,  // <-- Callback для обновления
    };
    
    screen_register(&config);
}

// Позже обновляем значение без пересоздания
void update_sensor_value(float new_value) {
    screen_update("sensor_detail", &new_value);
}

/* =============================
 *  ПРИМЕР 5: УСЛОВНЫЙ ПОКАЗ
 * ============================= */

// Проверка прав перед показом экрана
static bool check_admin_access(void) {
    // Проверяем, залогинен ли админ
    // return is_admin_logged_in();
    return true;  // Для примера всегда разрешаем
}

void admin_screen_register(void) {
    screen_config_t config = {
        .id = "admin_settings",
        .title = "Admin Settings",
        .category = SCREEN_CATEGORY_SETTINGS,
        .parent_id = "system_menu",
        .can_go_back = true,
        .lazy_load = true,
        .create_fn = NULL,  // Функция создания
        .can_show_fn = check_admin_access,  // <-- Проверка перед показом
    };
    
    screen_register(&config);
}

// При попытке показать без прав
void try_show_admin() {
    esp_err_t ret = screen_show("admin_settings", NULL);
    if (ret == ESP_ERR_NOT_ALLOWED) {
        ESP_LOGW("APP", "Access denied to admin settings");
        // Показать сообщение об ошибке
    }
}

/* =============================
 *  ПРИМЕР 6: ПОЛНАЯ ИНТЕГРАЦИЯ
 * ============================= */

// Регистрация всех экранов приложения
void app_screens_register_all(void) {
    // Главный экран
    main_screen_register();         // id: "main", is_root=true
    
    // Экраны датчиков
    sensor_detail_register();       // id: "detail_ph", parent: "main"
    sensor_settings_register();     // id: "settings_ph", parent: "detail_ph"
    // ... повторить для всех датчиков
    
    // Системные экраны
    system_menu_register();         // id: "system_menu", parent: "main"
    wifi_settings_register();       // id: "wifi_settings", parent: "system_menu"
    auto_control_register();        // id: "auto_control", parent: "system_menu"
    
    ESP_LOGI("APP", "All screens registered");
}

// Инициализация приложения
void app_main(void) {
    // 1. Инициализация LVGL и hardware
    lcd_init();
    encoder_init();
    lvgl_init();
    
    // 2. Инициализация Screen Manager
    screen_manager_config_t sm_config = {
        .enable_cache = true,
        .enable_history = true,
        .max_cache_size = 5,
        .transition_time = 300,
        .enable_animations = true,
    };
    screen_manager_init(&sm_config);
    
    // 3. Регистрация всех экранов
    app_screens_register_all();
    
    // 4. Показываем главный экран
    screen_show("main", NULL);
    
    ESP_LOGI("APP", "Application started");
    
    // Основной цикл
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* =============================
 *  ПРИМЕР 7: ИСПОЛЬЗОВАНИЕ В ЭНКОДЕРЕ
 * ============================= */

// Интеграция с энкодером (в lvgl_ui.c)
static void handle_encoder_event(encoder_event_t *event) {
    screen_instance_t *current = screen_get_current();
    if (!current) return;
    
    switch (event->type) {
        case ENCODER_EVENT_ROTATE_CW:
            // Делегируем группе LVGL (стандартная навигация)
            if (current->encoder_group) {
                lv_group_focus_next(current->encoder_group);
            }
            break;
            
        case ENCODER_EVENT_ROTATE_CCW:
            if (current->encoder_group) {
                lv_group_focus_prev(current->encoder_group);
            }
            break;
            
        case ENCODER_EVENT_BUTTON_PRESS:
            // Отправляем ENTER в группу
            if (current->encoder_group) {
                lv_group_send_data(current->encoder_group, LV_KEY_ENTER);
            }
            break;
    }
}

/* =============================
 *  ПРИМЕР 8: МИГРАЦИЯ СТАРОГО КОДА
 * ============================= */

// СТАРЫЙ КОД (до рефакторинга):
/*
static lv_obj_t *detail_ph_screen = NULL;
static lv_group_t *detail_ph_group = NULL;

static void create_detail_ph_screen(void) {
    detail_ph_screen = lv_obj_create(NULL);
    // 150 строк создания UI...
    detail_ph_group = lv_group_create();
    // ...
}

void show_detail_ph_screen(void) {
    if (detail_ph_screen == NULL) {
        create_detail_ph_screen();
    }
    lv_scr_load(detail_ph_screen);
    lv_indev_set_group(encoder, detail_ph_group);
}
*/

// НОВЫЙ КОД (после рефакторинга):

static lv_obj_t* detail_ph_create(void *params) {
    template_detail_config_t cfg = {
        .title = "pH Sensor",
        .description = "Monitor pH levels",
        .current_value = 6.8f,
        .target_value = 7.0f,
        .unit = "pH",
        .decimals = 2,
    };
    
    screen_instance_t *inst = screen_get_by_id("detail_ph");
    return template_create_detail_screen(&cfg, inst->encoder_group);
}

void detail_ph_register(void) {
    screen_config_t config = {
        .id = "detail_ph",
        .title = "pH Detail",
        .category = SCREEN_CATEGORY_DETAIL,
        .parent_id = "main",
        .lazy_load = true,
        .create_fn = detail_ph_create,
    };
    screen_register(&config);
}

// Использование: одна строка!
void show_detail_ph() {
    screen_show("detail_ph", NULL);  // Вся логика автоматическая!
}

/* =============================
 *  СРАВНЕНИЕ: ДО И ПОСЛЕ
 * ============================= */

/**
 * ДО РЕФАКТОРИНГА (для одного экрана):
 * 
 * 1. В lvgl_ui.c:
 *    - Добавить в enum: 1 строка
 *    - Глобальная переменная экрана: 1 строка
 *    - Глобальная переменная группы: 1 строка
 *    - Функция create_*_screen(): 50-150 строк
 *    - Case в show_screen(): 10 строк
 *    - Case в back_button_event_cb(): 5 строк
 *    - Обработка энкодера: 15 строк
 * 
 * ИТОГО: ~200+ строк в ОДНОМ файле
 * 
 * ========================================
 * 
 * ПОСЛЕ РЕФАКТОРИНГА (для одного экрана):
 * 
 * 1. В новом файле screens/my_screen.c:
 *    - Функция create: ~20 строк (используя виджеты/шаблоны)
 *    - Функция register: ~15 строк
 *    - Вызов register в main: 1 строка
 * 
 * ИТОГО: ~36 строк в ОТДЕЛЬНОМ файле
 * 
 * ЭКОНОМИЯ: 82%!
 * ПЛЮС: Код модульный, тестируемый, переиспользуемый
 */

---

/**
 * ИСПОЛЬЗОВАНИЕ В MAIN.C
 */

void app_main(void) {
    // ... инициализация hardware ...
    
    // 1. Инициализируем Screen Manager
    ESP_LOGI("APP", "Initializing Screen Manager");
    screen_manager_init(NULL);
    
    // 2. Регистрируем все экраны
    ESP_LOGI("APP", "Registering screens");
    
    example_screen_register();
    menu_screen_register();
    detail_ph_register();
    // ... остальные экраны
    
    ESP_LOGI("APP", "Registered %d screens", screen_get_registered_count());
    
    // 3. Показываем главный экран
    ESP_LOGI("APP", "Showing main screen");
    screen_show("main", NULL);
    
    // 4. Основной цикл
    while (1) {
        // Навигация теперь автоматическая через энкодер!
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

---

/**
 * НАВИГАЦИЯ - ПРИМЕРЫ
 */

void navigation_examples(void) {
    // Показать экран
    screen_show("detail_ph", NULL);
    
    // С параметрами
    sensor_params_t params = {.id = 0};
    screen_show("detail_ph", &params);
    
    // Вернуться назад
    screen_go_back();           // Из истории
    screen_go_to_parent();      // К parent_id
    screen_go_home();           // На главный экран
    
    // Обновить данные
    float new_value = 7.2f;
    screen_update("detail_ph", &new_value);
    
    // Проверить состояние
    if (screen_is_visible_check("detail_ph")) {
        ESP_LOGI("NAV", "Detail screen is visible");
    }
    
    // Информация
    screen_instance_t *current = screen_get_current();
    ESP_LOGI("NAV", "Current screen: %s", current->config->id);
    ESP_LOGI("NAV", "History size: %d", screen_get_history_count());
}

