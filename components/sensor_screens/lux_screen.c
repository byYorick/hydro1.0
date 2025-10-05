#include "sensor_screens.h"
#include "lvgl_main.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "LUX_SCREEN";

// Данные для освещенности
typedef struct {
    float current_value;
    float target_value;
    float min_value;
    float max_value;
    bool alarm_enabled;
    float alarm_low;
    float alarm_high;
} lux_data_t;

static lux_data_t lux_data = {
    .current_value = 450.0f,
    .target_value = 500.0f,
    .min_value = 0.0f,
    .max_value = 1000.0f,
    .alarm_enabled = true,
    .alarm_low = 200.0f,
    .alarm_high = 800.0f
};

// Функции обратного вызова
static void lux_back_button_event_cb(lv_event_t *e);
static void lux_settings_button_event_cb(lv_event_t *e);
static void lux_calibration_button_event_cb(lv_event_t *e);
static void lux_alarm_button_event_cb(lv_event_t *e);

void lux_detail_screen_init(void)
{
    ESP_LOGI(TAG, "Initializing Lux detail screen");
    
    sensor_screen_t *screen = &lux_detail_screen;
    if (screen->is_initialized) return;

    // Создаем экран
    screen->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen->screen, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(screen->screen, LV_OPA_COVER, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2d2d2d), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(header, 10, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    screen->back_button = lv_btn_create(header);
    lv_obj_set_style_bg_color(screen->back_button, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(screen->back_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen->back_button, 5, 0);
    lv_obj_set_size(screen->back_button, 40, 40);
    lv_obj_align(screen->back_button, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(screen->back_button, lux_back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(screen->back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title_label = lv_label_create(header);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(title_label, "Light Level");
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
    lv_obj_set_style_text_color(current_label, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(current_label, "Current Lux:");
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 0, 0);

    screen->current_value_label = lv_label_create(current_container);
    lv_obj_set_style_text_color(screen->current_value_label, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_font(screen->current_value_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(screen->current_value_label, "450 lux");
    lv_obj_align(screen->current_value_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // Целевое значение
    lv_obj_t *target_label = lv_label_create(current_container);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(target_label, "Target Lux:");
    lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    screen->target_value_label = lv_label_create(current_container);
    lv_obj_set_style_text_color(screen->target_value_label, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_font(screen->target_value_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(screen->target_value_label, "500 lux");
    lv_obj_align(screen->target_value_label, LV_ALIGN_TOP_RIGHT, 0, 25);

    // Мини-график
    screen->chart = lv_chart_create(content);
    lv_obj_set_style_bg_color(screen->chart, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_opa(screen->chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(screen->chart, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(screen->chart, 1, 0);
    lv_obj_set_style_radius(screen->chart, 5, 0);
    lv_obj_set_size(screen->chart, LV_PCT(100), 120);
    lv_obj_align(screen->chart, LV_ALIGN_TOP_MID, 0, 100);
    lv_chart_set_type(screen->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(screen->chart, 20);
    lv_chart_set_range(screen->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);

    // Серия данных
    lv_chart_series_t *series = lv_chart_add_series(screen->chart, lv_color_hex(0x00ff88), LV_CHART_AXIS_PRIMARY_Y);
    // lv_chart_set_zoom_x удалена в LVGL 9.x
    // lv_chart_set_zoom_x(screen->chart, 256);

    // Кнопка настроек
    screen->settings_button = lv_btn_create(content);
    lv_obj_set_style_bg_color(screen->settings_button, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(screen->settings_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen->settings_button, 5, 0);
    lv_obj_set_style_pad_all(screen->settings_button, 10, 0);
    lv_obj_set_size(screen->settings_button, 120, 40);
    lv_obj_align(screen->settings_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(screen->settings_button, lux_settings_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_label = lv_label_create(screen->settings_button);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);

    screen->is_initialized = true;
    ESP_LOGI(TAG, "Lux detail screen initialized");
}

void lux_settings_screen_init(void)
{
    ESP_LOGI(TAG, "Initializing Lux settings screen");
    
    sensor_screen_t *screen = &lux_settings_screen;
    if (screen->is_initialized) return;

    // Создаем экран
    screen->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen->screen, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(screen->screen, LV_OPA_COVER, 0);

    // Заголовок
    lv_obj_t *header = lv_obj_create(screen->screen);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2d2d2d), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(header, 10, 0);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Кнопка назад
    screen->back_button = lv_btn_create(header);
    lv_obj_set_style_bg_color(screen->back_button, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(screen->back_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen->back_button, 5, 0);
    lv_obj_set_size(screen->back_button, 40, 40);
    lv_obj_align(screen->back_button, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(screen->back_button, lux_back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(screen->back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Заголовок
    lv_obj_t *title_label = lv_label_create(header);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(title_label, "Light Settings");
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    // Основной контент
    lv_obj_t *content = lv_obj_create(screen->screen);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 60);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, 20, 0);

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
        lv_obj_set_style_bg_color(item, lv_color_hex(0x404040), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0x606060), 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_pad_all(item, 10, 0);
        lv_obj_set_size(item, LV_PCT(100), 40);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, i * 50 + 20);

        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_center(item_label);

        // Добавляем обработчики событий для разных пунктов
        switch (i) {
            case 0: // Calibration
                lv_obj_add_event_cb(item, lux_calibration_button_event_cb, LV_EVENT_CLICKED, NULL);
                break;
            case 1: // Alarm Thresholds
                lv_obj_add_event_cb(item, lux_alarm_button_event_cb, LV_EVENT_CLICKED, NULL);
                break;
            default:
                break;
        }
    }

    screen->is_initialized = true;
    ESP_LOGI(TAG, "Lux settings screen initialized");
}

// Функции обратного вызова
static void lux_back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Lux back button clicked");
        // Здесь будет логика возврата к главному экрану
    }
}

static void lux_settings_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Lux settings button clicked");
        // Переход к экрану настроек освещенности
        if (!lux_settings_screen.is_initialized) {
            lux_settings_screen_init();
        }
        lv_screen_load(lux_settings_screen.screen);
    }
}

static void lux_calibration_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Lux calibration button clicked");
        // Здесь будет логика калибровки освещенности
    }
}

static void lux_alarm_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Lux alarm button clicked");
        // Здесь будет логика настройки аварийных порогов освещенности
    }
}

// Функция обновления данных освещенности
void lux_update_data(float current_value, float target_value)
{
    lux_data.current_value = current_value;
    lux_data.target_value = target_value;
    
    if (lux_detail_screen.is_initialized) {
        char current_text[32];
        char target_text[32];
        
        snprintf(current_text, sizeof(current_text), "%.0f lux", current_value);
        snprintf(target_text, sizeof(target_text), "%.0f lux", target_value);
        
        lv_label_set_text(lux_detail_screen.current_value_label, current_text);
        lv_label_set_text(lux_detail_screen.target_value_label, target_text);
    }
}
