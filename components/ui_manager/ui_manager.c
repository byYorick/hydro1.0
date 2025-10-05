#include "ui_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_MANAGER";

// Глобальные переменные
static ui_screen_t screens[UI_SCREEN_COUNT][SENSOR_COUNT];
static sensor_data_t sensor_data[SENSOR_COUNT];
static sensor_type_t current_focus = SENSOR_PH;
static ui_theme_t current_theme;
static SemaphoreHandle_t ui_mutex = NULL;
static bool ui_initialized = false;

// Стили UI
static lv_style_t style_bg;
static lv_style_t style_card;
static lv_style_t style_title;
static lv_style_t style_value;
static lv_style_t style_value_large;
static lv_style_t style_unit;
static lv_style_t style_focus;
static lv_style_t style_button;
static lv_style_t style_chart;
static bool styles_initialized = false;

// Метаданные датчиков
static const struct {
    const char *name;
    const char *unit;
    const char *description;
    float min_value;
    float max_value;
    float default_target;
    float alarm_low;
    float alarm_high;
    uint8_t decimals;
} sensor_metadata[SENSOR_COUNT] = {
    [SENSOR_PH] = {
        .name = "pH",
        .unit = "",
        .description = "Keep the nutrient solution balanced for optimal uptake.",
        .min_value = 4.0f,
        .max_value = 9.0f,
        .default_target = 6.8f,
        .alarm_low = 6.0f,
        .alarm_high = 7.5f,
        .decimals = 2
    },
    [SENSOR_EC] = {
        .name = "EC",
        .unit = "mS/cm",
        .description = "Electrical conductivity shows nutrient strength. Stay in range!",
        .min_value = 0.0f,
        .max_value = 3.0f,
        .default_target = 1.5f,
        .alarm_low = 0.8f,
        .alarm_high = 2.0f,
        .decimals = 2
    },
    [SENSOR_TEMPERATURE] = {
        .name = "Temperature",
        .unit = "°C",
        .description = "Keep solution and air temperature comfortable for the crop.",
        .min_value = 15.0f,
        .max_value = 35.0f,
        .default_target = 24.0f,
        .alarm_low = 18.0f,
        .alarm_high = 30.0f,
        .decimals = 1
    },
    [SENSOR_HUMIDITY] = {
        .name = "Humidity",
        .unit = "%",
        .description = "Stable humidity reduces stress and supports steady growth.",
        .min_value = 20.0f,
        .max_value = 100.0f,
        .default_target = 70.0f,
        .alarm_low = 45.0f,
        .alarm_high = 75.0f,
        .decimals = 1
    },
    [SENSOR_LUX] = {
        .name = "Light",
        .unit = "lux",
        .description = "Monitor light levels to maintain healthy photosynthesis.",
        .min_value = 0.0f,
        .max_value = 2500.0f,
        .default_target = 500.0f,
        .alarm_low = 400.0f,
        .alarm_high = 1500.0f,
        .decimals = 0
    },
    [SENSOR_CO2] = {
        .name = "CO2",
        .unit = "ppm",
        .description = "Avoid excessive CO2 to keep plants and people comfortable.",
        .min_value = 0.0f,
        .max_value = 2000.0f,
        .default_target = 450.0f,
        .alarm_low = 0.0f,  // Только верхний порог
        .alarm_high = 800.0f,
        .decimals = 0
    }
};

// Внутренние функции
static esp_err_t init_styles(void);
static esp_err_t init_sensor_data(void);
static esp_err_t create_main_screen(void);
static esp_err_t create_sensor_detail_screen(sensor_type_t sensor_type);
static esp_err_t create_sensor_settings_screen(sensor_type_t sensor_type);
static esp_err_t update_sensor_display(sensor_type_t sensor_type);
static void back_button_event_cb(lv_event_t *e);
static void settings_button_event_cb(lv_event_t *e);
static void sensor_card_event_cb(lv_event_t *e);

esp_err_t ui_manager_init(void)
{
    if (ui_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing UI Manager");

    // Создаем мьютекс
    ui_mutex = xSemaphoreCreateMutex();
    if (ui_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create UI mutex");
        return ESP_ERR_NO_MEM;
    }

    // Инициализируем стили
    esp_err_t ret = init_styles();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize styles");
        return ret;
    }

    // Инициализируем данные датчиков
    ret = init_sensor_data();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor data");
        return ret;
    }

    // Инициализируем экраны
    memset(screens, 0, sizeof(screens));

    // Создаем главный экран
    ret = create_main_screen();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create main screen");
        return ret;
    }

    ui_initialized = true;
    ESP_LOGI(TAG, "UI Manager initialized successfully");
    return ESP_OK;
}

static esp_err_t init_styles(void)
{
    if (styles_initialized) {
        return ESP_OK;
    }

    // Инициализируем тему по умолчанию
    current_theme.bg_color = lv_color_hex(0x1a1a1a);
    current_theme.card_color = lv_color_hex(0x2d2d2d);
    current_theme.accent_color = lv_color_hex(0x00ff88);
    current_theme.text_color = lv_color_hex(0xffffff);
    current_theme.text_muted_color = lv_color_hex(0xcccccc);
    current_theme.danger_color = lv_color_hex(0xff4444);
    current_theme.warning_color = lv_color_hex(0xffaa00);
    current_theme.normal_color = lv_color_hex(0x00ff88);

    // Стиль фона
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, current_theme.bg_color);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);

    // Стиль карточки
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, current_theme.card_color);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 16);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x404040));

    // Стиль заголовка
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, current_theme.text_color);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_title, LV_OPA_COVER);

    // Стиль значения
    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, current_theme.text_color);
    lv_style_set_text_font(&style_value, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value, LV_OPA_COVER);

    // Стиль большого значения
    lv_style_init(&style_value_large);
    lv_style_set_text_color(&style_value_large, current_theme.accent_color);
    lv_style_set_text_font(&style_value_large, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_large, LV_OPA_COVER);

    // Стиль единиц измерения
    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, current_theme.text_muted_color);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_unit, LV_OPA_COVER);

    // Стиль фокуса
    lv_style_init(&style_focus);
    lv_style_set_border_color(&style_focus, current_theme.accent_color);
    lv_style_set_border_width(&style_focus, 2);
    lv_style_set_outline_color(&style_focus, current_theme.accent_color);
    lv_style_set_outline_width(&style_focus, 2);

    // Стиль кнопки
    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, lv_color_hex(0x404040));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_radius(&style_button, 5);
    lv_style_set_pad_all(&style_button, 10);

    // Стиль графика
    lv_style_init(&style_chart);
    lv_style_set_bg_color(&style_chart, lv_color_hex(0x2a2a2a));
    lv_style_set_bg_opa(&style_chart, LV_OPA_COVER);
    lv_style_set_border_color(&style_chart, lv_color_hex(0x404040));
    lv_style_set_border_width(&style_chart, 1);
    lv_style_set_radius(&style_chart, 5);

    styles_initialized = true;
    return ESP_OK;
}

static esp_err_t init_sensor_data(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_data[i].current_value = 0.0f;
        sensor_data[i].target_value = sensor_metadata[i].default_target;
        sensor_data[i].min_value = sensor_metadata[i].min_value;
        sensor_data[i].max_value = sensor_metadata[i].max_value;
        sensor_data[i].alarm_enabled = true;
        sensor_data[i].alarm_low = sensor_metadata[i].alarm_low;
        sensor_data[i].alarm_high = sensor_metadata[i].alarm_high;
        sensor_data[i].unit = sensor_metadata[i].unit;
        sensor_data[i].name = sensor_metadata[i].name;
        sensor_data[i].description = sensor_metadata[i].description;
        sensor_data[i].decimals = sensor_metadata[i].decimals;
    }
    return ESP_OK;
}

static esp_err_t create_main_screen(void)
{
    ui_screen_t *screen = &screens[UI_SCREEN_MAIN][0];
    if (screen->is_initialized) {
        return ESP_OK;
    }

    // Создаем главный экран
    screen->screen = lv_scr_act();
    screen->type = UI_SCREEN_MAIN;
    screen->sensor_type = SENSOR_PH; // Не используется для главного экрана
    screen->is_initialized = true;
    screen->is_visible = true;

    lv_obj_add_style(screen->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(screen->screen, 16, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_add_style(header, &style_card, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Hydroponics Monitor");
    lv_obj_center(title);

    // Контейнер для карточек датчиков
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 80);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_pad_column(content, 8, 0);

    // Создаем карточки для каждого датчика
    for (int i = 0; i < SENSOR_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(content);
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, LV_PCT(48), 90);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_style_pad_row(card, 6, 0);

        // Название датчика
        lv_obj_t *name_label = lv_label_create(card);
        lv_obj_add_style(name_label, &style_unit, 0);
        lv_label_set_text(name_label, sensor_metadata[i].name);

        // Текущее значение
        lv_obj_t *value_label = lv_label_create(card);
        lv_obj_add_style(value_label, &style_value_large, 0);
        lv_label_set_text(value_label, "--");

        // Единицы измерения
        lv_obj_t *unit_label = lv_label_create(card);
        lv_obj_add_style(unit_label, &style_unit, 0);
        lv_label_set_text(unit_label, sensor_metadata[i].unit);

        // Статус
        lv_obj_t *status_label = lv_label_create(card);
        lv_obj_add_style(status_label, &style_unit, 0);
        lv_label_set_text(status_label, "Normal");

        // Делаем карточку кликабельной
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, sensor_card_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Сохраняем ссылки на элементы для обновления
        // Сохраняем ссылки на элементы для обновления
        // В LVGL 8.x нужно использовать другой подход для хранения данных
    }

    ESP_LOGI(TAG, "Main screen created");
    return ESP_OK;
}

static esp_err_t create_sensor_detail_screen(sensor_type_t sensor_type)
{
    if (sensor_type >= SENSOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    ui_screen_t *screen = &screens[UI_SCREEN_SENSOR_DETAIL][sensor_type];
    if (screen->is_initialized) {
        return ESP_OK;
    }

    // Создаем экран детализации
    screen->screen = lv_obj_create(NULL);
    screen->type = UI_SCREEN_SENSOR_DETAIL;
    screen->sensor_type = sensor_type;
    screen->is_initialized = true;
    screen->is_visible = false;

    lv_obj_add_style(screen->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(screen->screen, 16, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_add_style(header, &style_card, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_add_style(back_btn, &style_button, 0);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text_fmt(title, "%s Details", sensor_metadata[sensor_type].name);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Основной контент
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 80);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);

    // Текущее значение
    lv_obj_t *current_container = lv_obj_create(content);
    lv_obj_remove_style_all(current_container);
    lv_obj_set_size(current_container, LV_PCT(100), 80);
    lv_obj_align(current_container, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *current_label = lv_label_create(current_container);
    lv_obj_add_style(current_label, &style_unit, 0);
    lv_label_set_text(current_label, "Current:");
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *current_value = lv_label_create(current_container);
    lv_obj_add_style(current_value, &style_value_large, 0);
    lv_label_set_text(current_value, "--");
    lv_obj_align(current_value, LV_ALIGN_TOP_LEFT, 0, 25);

    // Целевое значение
    lv_obj_t *target_label = lv_label_create(current_container);
    lv_obj_add_style(target_label, &style_unit, 0);
    lv_label_set_text(target_label, "Target:");
    lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *target_value = lv_label_create(current_container);
    lv_obj_add_style(target_value, &style_value_large, 0);
    lv_label_set_text(target_value, "--");
    lv_obj_align(target_value, LV_ALIGN_TOP_RIGHT, 0, 25);

    // Информация о диапазоне
    lv_obj_t *range_info = lv_obj_create(content);
    lv_obj_remove_style_all(range_info);
    lv_obj_set_size(range_info, LV_PCT(100), 80);
    lv_obj_align(range_info, LV_ALIGN_TOP_MID, 0, 100);
    
    lv_obj_t *min_label = lv_label_create(range_info);
    lv_obj_add_style(min_label, &style_unit, 0);
    lv_label_set_text_fmt(min_label, "Min: %.2f %s", 
                          sensor_metadata[sensor_type].min_value,
                          sensor_metadata[sensor_type].unit);
    lv_obj_align(min_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *max_label = lv_label_create(range_info);
    lv_obj_add_style(max_label, &style_unit, 0);
    lv_label_set_text_fmt(max_label, "Max: %.2f %s", 
                          sensor_metadata[sensor_type].max_value,
                          sensor_metadata[sensor_type].unit);
    lv_obj_align(max_label, LV_ALIGN_TOP_LEFT, 0, 30);
    
    // Описание датчика
    lv_obj_t *desc_label = lv_label_create(range_info);
    lv_obj_add_style(desc_label, &style_unit, 0);
    lv_label_set_text(desc_label, sensor_metadata[sensor_type].description);
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc_label, LV_PCT(90));
    lv_obj_align(desc_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Кнопка настроек
    lv_obj_t *settings_btn = lv_btn_create(content);
    lv_obj_add_style(settings_btn, &style_button, 0);
    lv_obj_set_size(settings_btn, 120, 40);
    lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(settings_btn, settings_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)sensor_type);

    lv_obj_t *settings_label = lv_label_create(settings_btn);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);

    // Сохраняем ссылки на элементы
    // В LVGL 8.x нужно использовать другой подход для хранения данных
    // lv_obj_set_user_data(screen->screen, "current_value", current_value);
    // lv_obj_set_user_data(screen->screen, "target_value", target_value);
    // lv_obj_set_user_data(screen->screen, "chart", chart);
    // lv_obj_set_user_data(screen->screen, "series", series);

    ESP_LOGI(TAG, "Detail screen created for sensor %d", sensor_type);
    return ESP_OK;
}

static esp_err_t create_sensor_settings_screen(sensor_type_t sensor_type)
{
    if (sensor_type >= SENSOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    ui_screen_t *screen = &screens[UI_SCREEN_SENSOR_SETTINGS][sensor_type];
    if (screen->is_initialized) {
        return ESP_OK;
    }

    // Создаем экран настроек
    screen->screen = lv_obj_create(NULL);
    screen->type = UI_SCREEN_SENSOR_SETTINGS;
    screen->sensor_type = sensor_type;
    screen->is_initialized = true;
    screen->is_visible = false;

    lv_obj_add_style(screen->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(screen->screen, 16, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_add_style(header, &style_card, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_add_style(back_btn, &style_button, 0);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text_fmt(title, "%s Settings", sensor_metadata[sensor_type].name);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Основной контент
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 80);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);

    // Пункты настроек
    const char *settings_items[] = {
        "Calibration",
        "Alarm Thresholds",
        "Data Logging",
        "Display Options",
        "Reset to Default"
    };

    for (int i = 0; i < 5; i++) {
        lv_obj_t *item = lv_btn_create(content);
        lv_obj_add_style(item, &style_button, 0);
        lv_obj_set_size(item, LV_PCT(100), 40);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, i * 50 + 20);

        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_center(item_label);
    }

    ESP_LOGI(TAG, "Settings screen created for sensor %d", sensor_type);
    return ESP_OK;
}

// Обработчики событий
static void back_button_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Back button clicked");
    ui_show_main_screen();
}

static void settings_button_event_cb(lv_event_t *e)
{
    sensor_type_t sensor_type = (sensor_type_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Settings button clicked for sensor %d", sensor_type);
    ui_show_screen(UI_SCREEN_SENSOR_SETTINGS, sensor_type);
}

static void sensor_card_event_cb(lv_event_t *e)
{
    sensor_type_t sensor_type = (sensor_type_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Sensor card clicked: %d", sensor_type);
    ui_show_screen(UI_SCREEN_SENSOR_DETAIL, sensor_type);
}

// Публичные функции
esp_err_t ui_show_screen(ui_screen_type_t screen_type, sensor_type_t sensor_type)
{
    if (!ui_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    // Скрываем все экраны
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        for (int j = 0; j < SENSOR_COUNT; j++) {
            if (screens[i][j].is_initialized && screens[i][j].is_visible) {
                lv_obj_add_flag(screens[i][j].screen, LV_OBJ_FLAG_HIDDEN);
                screens[i][j].is_visible = false;
            }
        }
    }

    // Показываем нужный экран
    if (screen_type == UI_SCREEN_MAIN) {
        screens[UI_SCREEN_MAIN][0].is_visible = true;
        lv_obj_clear_flag(screens[UI_SCREEN_MAIN][0].screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(screens[UI_SCREEN_MAIN][0].screen);
    } else {
        if (sensor_type >= SENSOR_COUNT) {
            ret = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        ui_screen_t *screen = &screens[screen_type][sensor_type];
        
        // Создаем экран если еще не создан
        if (!screen->is_initialized) {
            if (screen_type == UI_SCREEN_SENSOR_DETAIL) {
                ret = create_sensor_detail_screen(sensor_type);
            } else if (screen_type == UI_SCREEN_SENSOR_SETTINGS) {
                ret = create_sensor_settings_screen(sensor_type);
            }
            
            if (ret != ESP_OK) {
                goto exit;
            }
        }

        screen->is_visible = true;
        lv_obj_clear_flag(screen->screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(screen->screen);
        
        // Обновляем данные на экране
        update_sensor_display(sensor_type);
    }

exit:
    xSemaphoreGive(ui_mutex);
    return ret;
}

esp_err_t ui_show_main_screen(void)
{
    return ui_show_screen(UI_SCREEN_MAIN, SENSOR_PH);
}

bool ui_is_screen_visible(ui_screen_type_t screen_type, sensor_type_t sensor_type)
{
    if (screen_type >= UI_SCREEN_COUNT || sensor_type >= SENSOR_COUNT) {
        return false;
    }
    return screens[screen_type][sensor_type].is_visible;
}

esp_err_t ui_update_sensor_data(sensor_type_t sensor_type, const sensor_data_t *data)
{
    if (!ui_initialized || sensor_type >= SENSOR_COUNT || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    sensor_data[sensor_type] = *data;
    update_sensor_display(sensor_type);

    xSemaphoreGive(ui_mutex);
    return ESP_OK;
}

esp_err_t ui_get_sensor_data(sensor_type_t sensor_type, sensor_data_t *data)
{
    if (!ui_initialized || sensor_type >= SENSOR_COUNT || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *data = sensor_data[sensor_type];

    xSemaphoreGive(ui_mutex);
    return ESP_OK;
}

static esp_err_t update_sensor_display(sensor_type_t sensor_type)
{
    if (sensor_type >= SENSOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const sensor_data_t *data = &sensor_data[sensor_type];

    // Обновляем главный экран
    if (screens[UI_SCREEN_MAIN][0].is_visible) {
        // Находим карточку датчика на главном экране
        lv_obj_t *main_screen = screens[UI_SCREEN_MAIN][0].screen;
        lv_obj_t *content = lv_obj_get_child(main_screen, 1); // Контейнер с карточками
        lv_obj_t *card = lv_obj_get_child(content, sensor_type);
        
        if (card) {
        // В LVGL 9.x нужно использовать другой подход для получения данных
        // Получаем дочерние элементы напрямую
        lv_obj_t *value_label = lv_obj_get_child(card, 1); // Первый дочерний элемент - value_label
        lv_obj_t *status_label = lv_obj_get_child(card, 2); // Второй дочерний элемент - status_label
        
        if (value_label) {
                char value_text[32];
                snprintf(value_text, sizeof(value_text), "%.*f", 
                        sensor_metadata[sensor_type].decimals, data->current_value);
                lv_label_set_text(value_label, value_text);
            }
            
            if (status_label) {
                const char *status_text = "Normal";
                lv_color_t status_color = current_theme.normal_color;
                
                if (data->alarm_enabled) {
                    if (data->current_value < data->alarm_low || data->current_value > data->alarm_high) {
                        status_text = "Critical";
                        status_color = current_theme.danger_color;
                    } else if (data->current_value < data->alarm_low * 1.1f || data->current_value > data->alarm_high * 0.9f) {
                        status_text = "Warning";
                        status_color = current_theme.warning_color;
                    }
                }
                
                lv_label_set_text(status_label, status_text);
                lv_obj_set_style_text_color(status_label, status_color, 0);
            }
        }
    }

    // Обновляем экран детализации
    ui_screen_t *detail_screen = &screens[UI_SCREEN_SENSOR_DETAIL][sensor_type];
    if (detail_screen->is_initialized && detail_screen->is_visible) {
        // В LVGL 9.x получаем дочерние элементы напрямую
        lv_obj_t *current_value = lv_obj_get_child(detail_screen->screen, 1); // Первый дочерний элемент
        lv_obj_t *target_value = lv_obj_get_child(detail_screen->screen, 2); // Второй дочерний элемент
        lv_obj_t *chart = lv_obj_get_child(detail_screen->screen, 3); // Третий дочерний элемент
        lv_chart_series_t *series = lv_chart_get_series_next(chart, NULL); // Получаем первую серию
        
        if (current_value) {
            char value_text[32];
            snprintf(value_text, sizeof(value_text), "%.*f %s", 
                    sensor_metadata[sensor_type].decimals, data->current_value, data->unit);
            lv_label_set_text(current_value, value_text);
        }
        
        if (target_value) {
            char value_text[32];
            snprintf(value_text, sizeof(value_text), "%.*f %s", 
                    sensor_metadata[sensor_type].decimals, data->target_value, data->unit);
            lv_label_set_text(target_value, value_text);
        }
        
        if (chart && series) {
            lv_chart_set_next_value(chart, series, (int32_t)(data->current_value * 100));
        }
    }

    return ESP_OK;
}

esp_err_t ui_set_focus(sensor_type_t sensor_type)
{
    if (!ui_initialized || sensor_type >= SENSOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    current_focus = sensor_type;
    return ESP_OK;
}

sensor_type_t ui_get_focus(void)
{
    return current_focus;
}

esp_err_t ui_handle_encoder_event(uint32_t key, int32_t diff)
{
    if (!ui_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Обработка навигации энкодером
    if (screens[UI_SCREEN_MAIN][0].is_visible) {
        if (key == LV_KEY_ENTER) {
            ui_show_screen(UI_SCREEN_SENSOR_DETAIL, current_focus);
        } else if (diff != 0) {
            current_focus = (current_focus + diff + SENSOR_COUNT) % SENSOR_COUNT;
            // Здесь можно добавить визуальное выделение фокуса
        }
    } else {
        if (key == LV_KEY_ESC) {
            ui_show_main_screen();
        }
    }

    return ESP_OK;
}

const char* ui_get_sensor_name(sensor_type_t sensor_type)
{
    if (sensor_type >= SENSOR_COUNT) {
        return "Unknown";
    }
    return sensor_metadata[sensor_type].name;
}

const char* ui_get_sensor_unit(sensor_type_t sensor_type)
{
    if (sensor_type >= SENSOR_COUNT) {
        return "";
    }
    return sensor_metadata[sensor_type].unit;
}

sensor_type_t ui_get_sensor_count(void)
{
    return SENSOR_COUNT;
}
