#include "sensor_screens.h"
#include "lvgl_main.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SENSOR_SCREENS";

// Глобальные переменные экранов
sensor_screen_t ph_detail_screen = {0};
sensor_screen_t ec_detail_screen = {0};
sensor_screen_t temp_detail_screen = {0};
sensor_screen_t humidity_detail_screen = {0};
sensor_screen_t lux_detail_screen = {0};
sensor_screen_t co2_detail_screen = {0};

sensor_screen_t ph_settings_screen = {0};
sensor_screen_t ec_settings_screen = {0};
sensor_screen_t temp_settings_screen = {0};
sensor_screen_t humidity_settings_screen = {0};
sensor_screen_t lux_settings_screen = {0};
sensor_screen_t co2_settings_screen = {0};

// Стили для экранов
static lv_style_t style_screen_bg;
static lv_style_t style_header;
static lv_style_t style_title;
static lv_style_t style_value_large;
static lv_style_t style_value_small;
static lv_style_t style_button;
static lv_style_t style_chart;

// Функции обратного вызова
static void back_button_event_cb(lv_event_t *e);
static void settings_button_event_cb(lv_event_t *e);

// Вспомогательные функции
static void init_styles(void);
static void create_detail_screen_ui(sensor_screen_t *screen, const char *title, const char *unit, const char *description);
static void create_settings_screen_ui(sensor_screen_t *screen, const char *title);
static sensor_screen_t* get_screen_by_type(sensor_screen_type_t screen_type);

void init_styles(void)
{
    // Стиль фона экрана
    lv_style_init(&style_screen_bg);
    lv_style_set_bg_color(&style_screen_bg, lv_color_hex(0x1a1a1a));
    lv_style_set_bg_opa(&style_screen_bg, LV_OPA_COVER);

    // Стиль заголовка
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, lv_color_hex(0x2d2d2d));
    lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
    lv_style_set_pad_all(&style_header, 10);
    lv_style_set_radius(&style_header, 0);

    // Стиль заголовка
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_hex(0xffffff));
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_title, LV_OPA_COVER);

    // Стиль больших значений
    lv_style_init(&style_value_large);
    lv_style_set_text_color(&style_value_large, lv_color_hex(0x00ff88));
    lv_style_set_text_font(&style_value_large, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_large, LV_OPA_COVER);

    // Стиль малых значений
    lv_style_init(&style_value_small);
    lv_style_set_text_color(&style_value_small, lv_color_hex(0xcccccc));
    lv_style_set_text_font(&style_value_small, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_small, LV_OPA_COVER);

    // Стиль кнопок
    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, lv_color_hex(0x404040));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_border_color(&style_button, lv_color_hex(0x606060));
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_radius(&style_button, 5);
    lv_style_set_pad_all(&style_button, 10);

    // Стиль графика
    lv_style_init(&style_chart);
    lv_style_set_bg_color(&style_chart, lv_color_hex(0x2a2a2a));
    lv_style_set_bg_opa(&style_chart, LV_OPA_COVER);
    lv_style_set_border_color(&style_chart, lv_color_hex(0x404040));
    lv_style_set_border_width(&style_chart, 1);
    lv_style_set_radius(&style_chart, 5);
}

void create_detail_screen_ui(sensor_screen_t *screen, const char *title, const char *unit, const char *description)
{
    if (screen->is_initialized) return;

    // Создаем экран
    screen->screen = lv_obj_create(NULL);
    lv_obj_add_style(screen->screen, &style_screen_bg, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_add_style(header, &style_header, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    screen->back_button = lv_btn_create(header);
    lv_obj_add_style(screen->back_button, &style_button, 0);
    lv_obj_set_size(screen->back_button, 40, 40);
    lv_obj_align(screen->back_button, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(screen->back_button, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(screen->back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title_label = lv_label_create(header);
    lv_obj_add_style(title_label, &style_title, 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    // Основной контент
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 60);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, 20, 0);

    // Текущее значение
    lv_obj_t *current_container = lv_obj_create(content);
    lv_obj_set_size(current_container, LV_PCT(100), 80);
    lv_obj_align(current_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(current_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(current_container, LV_OPA_TRANSP, 0);

    lv_obj_t *current_label = lv_label_create(current_container);
    lv_obj_add_style(current_label, &style_value_small, 0);
    lv_label_set_text(current_label, "Current:");
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 0, 0);

    screen->current_value_label = lv_label_create(current_container);
    lv_obj_add_style(screen->current_value_label, &style_value_large, 0);
    lv_label_set_text(screen->current_value_label, "0.00");
    lv_obj_align(screen->current_value_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // Целевое значение
    lv_obj_t *target_label = lv_label_create(current_container);
    lv_obj_add_style(target_label, &style_value_small, 0);
    lv_label_set_text(target_label, "Target:");
    lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    screen->target_value_label = lv_label_create(current_container);
    lv_obj_add_style(screen->target_value_label, &style_value_large, 0);
    lv_label_set_text(screen->target_value_label, "0.00");
    lv_obj_align(screen->target_value_label, LV_ALIGN_TOP_RIGHT, 0, 25);

    // Дополнительная информация
    lv_obj_t *info_label = lv_label_create(content);
    lv_obj_add_style(info_label, &style_value_small, 0);
    lv_label_set_text(info_label, description);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, LV_PCT(90));
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 100);
    
    // Диапазон значений
    lv_obj_t *range_container = lv_obj_create(content);
    lv_obj_set_size(range_container, LV_PCT(100), 60);
    lv_obj_align(range_container, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_opa(range_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(range_container, LV_OPA_TRANSP, 0);
    
    lv_obj_t *min_label = lv_label_create(range_container);
    lv_obj_add_style(min_label, &style_value_small, 0);
    lv_label_set_text(min_label, "Min: 0.0");
    lv_obj_align(min_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *max_label = lv_label_create(range_container);
    lv_obj_add_style(max_label, &style_value_small, 0);
    lv_label_set_text(max_label, "Max: 100.0");
    lv_obj_align(max_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // Кнопка настроек
    screen->settings_button = lv_btn_create(content);
    lv_obj_add_style(screen->settings_button, &style_button, 0);
    lv_obj_set_size(screen->settings_button, 120, 40);
    lv_obj_align(screen->settings_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(screen->settings_button, settings_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_label = lv_label_create(screen->settings_button);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);

    screen->is_initialized = true;
    ESP_LOGI(TAG, "Detail screen UI created for %s", title);
}

void create_settings_screen_ui(sensor_screen_t *screen, const char *title)
{
    if (screen->is_initialized) return;

    // Создаем экран
    screen->screen = lv_obj_create(NULL);
    lv_obj_add_style(screen->screen, &style_screen_bg, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_add_style(header, &style_header, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    screen->back_button = lv_btn_create(header);
    lv_obj_add_style(screen->back_button, &style_button, 0);
    lv_obj_set_size(screen->back_button, 40, 40);
    lv_obj_align(screen->back_button, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(screen->back_button, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(screen->back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title_label = lv_label_create(header);
    lv_obj_add_style(title_label, &style_title, 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    // Основной контент
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 60);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, 20, 0);

    // Заглушки настроек
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

    screen->is_initialized = true;
    ESP_LOGI(TAG, "Settings screen UI created for %s", title);
}

sensor_screen_t* get_screen_by_type(sensor_screen_type_t screen_type)
{
    switch (screen_type) {
        case SCREEN_PH_DETAIL: return &ph_detail_screen;
        case SCREEN_EC_DETAIL: return &ec_detail_screen;
        case SCREEN_TEMP_DETAIL: return &temp_detail_screen;
        case SCREEN_HUMIDITY_DETAIL: return &humidity_detail_screen;
        case SCREEN_LUX_DETAIL: return &lux_detail_screen;
        case SCREEN_CO2_DETAIL: return &co2_detail_screen;
        case SCREEN_PH_SETTINGS: return &ph_settings_screen;
        case SCREEN_EC_SETTINGS: return &ec_settings_screen;
        case SCREEN_TEMP_SETTINGS: return &temp_settings_screen;
        case SCREEN_HUMIDITY_SETTINGS: return &humidity_settings_screen;
        case SCREEN_LUX_SETTINGS: return &lux_settings_screen;
        case SCREEN_CO2_SETTINGS: return &co2_settings_screen;
        default: return NULL;
    }
}

static void back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Back button clicked");
        // Здесь будет логика возврата к главному экрану
        // lv_scr_load(main_screen);
    }
}

static void settings_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Settings button clicked");
        // Здесь будет логика перехода к экрану настроек
    }
}

// Функции инициализации экранов теперь реализованы в отдельных файлах

// Реализация функций управления экранами
void show_sensor_screen(sensor_screen_type_t screen_type)
{
    sensor_screen_t *screen = get_screen_by_type(screen_type);
    if (screen && screen->is_initialized) {
        lv_screen_load(screen->screen);
        ESP_LOGI(TAG, "Showing screen type: %d", screen_type);
    }
}

void hide_sensor_screen(sensor_screen_type_t screen_type)
{
    sensor_screen_t *screen = get_screen_by_type(screen_type);
    if (screen && screen->is_initialized) {
        lv_obj_add_flag(screen->screen, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Hiding screen type: %d", screen_type);
    }
}

void update_sensor_screen_data(sensor_screen_type_t screen_type, float current_value, float target_value)
{
    sensor_screen_t *screen = get_screen_by_type(screen_type);
    if (screen && screen->is_initialized) {
        char current_text[32];
        char target_text[32];
        
        snprintf(current_text, sizeof(current_text), "%.2f", current_value);
        snprintf(target_text, sizeof(target_text), "%.2f", target_value);
        
        lv_label_set_text(screen->current_value_label, current_text);
        lv_label_set_text(screen->target_value_label, target_text);
    }
}

void destroy_sensor_screen(sensor_screen_type_t screen_type)
{
    sensor_screen_t *screen = get_screen_by_type(screen_type);
    if (screen && screen->is_initialized) {
        lv_obj_del(screen->screen);
        memset(screen, 0, sizeof(sensor_screen_t));
        ESP_LOGI(TAG, "Destroyed screen type: %d", screen_type);
    }
}
