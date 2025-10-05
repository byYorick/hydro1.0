#include "lvgl_main.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
#include "encoder.h"
#include "sensor_screens.h"
#include <inttypes.h>
LV_FONT_DECLARE(lv_font_montserrat_14)

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "LVGL_MAIN";

/* =============================
 *  COLOR PALETTE
 * ============================= */
#define COLOR_BG            lv_color_white()
#define COLOR_SURFACE       lv_color_hex(0xF5F5F5)
#define COLOR_CARD          lv_color_hex(0xFFFFFF)
#define COLOR_ACCENT        lv_color_hex(0x1976D2)
#define COLOR_ACCENT_SOFT   lv_color_hex(0x64B5F6)
#define COLOR_NORMAL        lv_color_hex(0x2E7D32)
#define COLOR_WARNING       lv_color_hex(0xFF8F00)
#define COLOR_DANGER        lv_color_hex(0xD32F2F)
#define COLOR_TEXT          lv_color_hex(0x212121)
#define COLOR_TEXT_MUTED    lv_color_hex(0x616161)
#define COLOR_FOCUS         lv_color_hex(0x1976D2)
#define COLOR_SHADOW        lv_color_hex(0xCFD8DC)

/* =============================
 *  SENSOR META DATA
 * ============================= */
#define SENSOR_COUNT           6
#define HISTORY_POINTS         60
#define SENSOR_DATA_QUEUE_SIZE 10

/* =============================
 *  UI SCREEN MANAGEMENT
 * ============================= */
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_DETAIL_PH,
    SCREEN_DETAIL_EC,
    SCREEN_DETAIL_TEMP,
    SCREEN_DETAIL_HUMIDITY,
    SCREEN_DETAIL_LUX,
    SCREEN_DETAIL_CO2,
    SCREEN_SETTINGS_PH,
    SCREEN_SETTINGS_EC,
    SCREEN_SETTINGS_TEMP,
    SCREEN_SETTINGS_HUMIDITY,
    SCREEN_SETTINGS_LUX,
    SCREEN_SETTINGS_CO2,
    SCREEN_COUNT
} screen_type_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *chart;
    lv_obj_t *current_value_label;
    lv_obj_t *set_value_label;
    lv_obj_t *settings_btn;
    lv_obj_t *back_btn;
    uint8_t sensor_index;
} detail_screen_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *back_btn;
    lv_obj_t *settings_list;
    uint8_t sensor_index;
} settings_screen_t;

/* =============================
 *  GLOBAL VARIABLES
 * ============================= */
static screen_type_t current_screen = SCREEN_MAIN;
static detail_screen_t detail_screens[SENSOR_COUNT];
static settings_screen_t settings_screens[SENSOR_COUNT];
static lv_obj_t *main_screen;
static lv_obj_t *sensor_cards[SENSOR_COUNT];

/* =============================
 *  ENCODER NAVIGATION
 * ============================= */
static int selected_card_index = 0;  // Индекс выбранной карточки на главном экране
static int selected_settings_item = 0;  // Индекс выбранного пункта настроек
static bool encoder_navigation_enabled = true; // Включаем обратно
int32_t last_encoder_diff = 0;  // Последняя разность энкодера

typedef struct {
    const char *title;
    const char *unit;
    const char *description;
    float chart_min;
    float chart_max;
    float warn_low;
    float warn_high;
    float danger_low;
    float danger_high;
    float chart_scale;
    uint8_t decimals;
} sensor_meta_t;

static const sensor_meta_t SENSOR_META[SENSOR_COUNT] = {
    {
        .title = "pH",
        .unit = "",
        .description = "Keep the nutrient solution balanced for optimal uptake.",
        .chart_min = 4.5f,
        .chart_max = 8.0f,
        .warn_low = 6.0f,
        .warn_high = 7.0f,
        .danger_low = 5.5f,
        .danger_high = 7.5f,
        .chart_scale = 100.0f,
        .decimals = 2,
    },
    {
        .title = "EC",
        .unit = "mS/cm",
        .description = "Electrical conductivity shows nutrient strength. Stay in range!",
        .chart_min = 0.0f,
        .chart_max = 3.0f,
        .warn_low = 1.2f,
        .warn_high = 2.0f,
        .danger_low = 0.8f,
        .danger_high = 2.4f,
        .chart_scale = 100.0f,
        .decimals = 2,
    },
    {
        .title = "Temperature",
        .unit = "degC",
        .description = "Keep solution and air temperature comfortable for the crop.",
        .chart_min = 10.0f,
        .chart_max = 40.0f,
        .warn_low = 20.0f,
        .warn_high = 28.0f,
        .danger_low = 15.0f,
        .danger_high = 32.0f,
        .chart_scale = 10.0f,
        .decimals = 1,
    },
    {
        .title = "Humidity",
        .unit = "%",
        .description = "Stable humidity reduces stress and supports steady growth.",
        .chart_min = 20.0f,
        .chart_max = 100.0f,
        .warn_low = 45.0f,
        .warn_high = 75.0f,
        .danger_low = 35.0f,
        .danger_high = 85.0f,
        .chart_scale = 10.0f,
        .decimals = 1,
    },
    {
        .title = "Light",
        .unit = "lux",
        .description = "Monitor light levels to maintain healthy photosynthesis.",
        .chart_min = 0.0f,
        .chart_max = 2500.0f,
        .warn_low = 400.0f,
        .warn_high = 1500.0f,
        .danger_low = 200.0f,
        .danger_high = 2000.0f,
        .chart_scale = 1.0f,
        .decimals = 0,
    },
    {
        .title = "CO2",
        .unit = "ppm",
        .description = "Avoid excessive CO2 to keep plants and people comfortable.",
        .chart_min = 0.0f,
        .chart_max = 2000.0f,
        .warn_low = NAN,
        .warn_high = 800.0f,
        .danger_low = NAN,
        .danger_high = 1200.0f,
        .chart_scale = 1.0f,
        .decimals = 0,
    },
};

/* =============================
 *  LVGL OBJECTS & STATE
 * ============================= */
static lv_obj_t *screen_main;
static lv_obj_t *screen_detail;
static lv_obj_t *status_bar;
static lv_obj_t *status_time_label;
static lv_obj_t *status_settings_btn;
static lv_timer_t *status_timer = NULL;

static lv_obj_t *sensor_containers[SENSOR_COUNT];
static lv_obj_t *value_labels[SENSOR_COUNT];
static lv_obj_t *status_labels[SENSOR_COUNT];

static lv_obj_t *detail_value_label = NULL;
static lv_obj_t *detail_status_label = NULL;
static lv_obj_t *detail_chart = NULL;
static lv_chart_series_t *detail_series = NULL;
static int detail_current_index = -1;

static lv_style_t style_bg;
static lv_style_t style_title;
static lv_style_t style_label;
static lv_style_t style_value;
static lv_style_t style_value_large;
static lv_style_t style_value_small;
static lv_style_t style_unit;
static lv_style_t style_focus;
static lv_style_t style_card;
static lv_style_t style_status_bar;
static lv_style_t style_badge;
static bool styles_initialized = false;

static lv_group_t *encoder_group = NULL;
static lv_group_t *detail_screen_groups[SENSOR_COUNT] = {NULL};
static lv_group_t *settings_screen_groups[SENSOR_COUNT] = {NULL};
static QueueHandle_t sensor_data_queue = NULL;
static int current_focus_index = -1;
static bool display_task_started = false;

static sensor_data_t last_sensor_data = {0};
static lv_coord_t sensor_history[SENSOR_COUNT][HISTORY_POINTS];
static uint16_t sensor_history_pos[SENSOR_COUNT];
static bool sensor_history_full[SENSOR_COUNT];
static bool sensor_snapshot_valid = false;

/* =============================
 *  FORWARD DECLARATIONS
 * ============================= */
static void init_styles(void);
static void create_main_ui(void);
static void create_detail_ui(int index);
static void create_status_bar(lv_obj_t *parent, const char *title);
static void status_timer_cb(lv_timer_t *timer);
static float get_sensor_value_by_index(const sensor_data_t *data, int index);
static void record_sensor_value(int index, float value);
static void configure_chart_axes(lv_obj_t *chart, int index);
// Удалено: populate_chart_with_history (графики удалены)
static void update_detail_view(int index);
static void update_sensor_display(sensor_data_t *data);
static void display_update_task(void *pvParameters);
static lv_obj_t *create_sensor_card(lv_obj_t *parent, int index);
static void sensor_card_event_cb(lv_event_t *e);
static void create_detail_screen(uint8_t sensor_index);
static void create_settings_screen(uint8_t sensor_index);
static void show_screen(screen_type_t screen);
static void back_button_event_cb(lv_event_t *e);
static void settings_button_event_cb(lv_event_t *e);
static void encoder_task(void *pvParameters);
static void handle_encoder_event(encoder_event_t *event);
static void update_card_selection(void);
static void update_settings_selection(void);
static void encoder_event_cb(lv_event_t *e);

/* =============================
 *  PUBLIC HELPERS
 * ============================= */
void* lvgl_get_sensor_data_queue(void)
{
    return (void*)sensor_data_queue;
}

/* =============================
 *  INTERNAL UTILITIES
 * ============================= */
static inline bool threshold_defined(float value)
{
    return !isnan(value);
}

static void init_styles(void)
{
    if (styles_initialized) {
        return;
    }
    
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, COLOR_BG);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);

    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, COLOR_TEXT);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_title, LV_OPA_COVER);

    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_label, LV_OPA_COVER);

    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, COLOR_TEXT);
    lv_style_set_text_font(&style_value, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value, LV_OPA_COVER);

    lv_style_init(&style_value_large);
    lv_style_set_text_color(&style_value_large, COLOR_TEXT);
    lv_style_set_text_font(&style_value_large, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_large, LV_OPA_COVER);

    lv_style_init(&style_value_small);
    lv_style_set_text_color(&style_value_small, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_value_small, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_small, LV_OPA_COVER);

    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_unit, LV_OPA_COVER);

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 16);
    lv_style_set_pad_row(&style_card, 12);
    lv_style_set_pad_column(&style_card, 8);
    lv_style_set_shadow_color(&style_card, COLOR_SHADOW);
    lv_style_set_shadow_width(&style_card, 8);
    lv_style_set_shadow_spread(&style_card, 2);

    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_radius(&style_status_bar, 8);
    lv_style_set_pad_all(&style_status_bar, 2);

    lv_style_init(&style_badge);
    lv_style_set_bg_color(&style_badge, COLOR_ACCENT_SOFT);
    lv_style_set_bg_opa(&style_badge, LV_OPA_COVER);
    lv_style_set_radius(&style_badge, 10);
    lv_style_set_pad_all(&style_badge, 4);
    lv_style_set_text_color(&style_badge, COLOR_TEXT);
    lv_style_set_text_font(&style_badge, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_badge, LV_OPA_COVER);

    lv_style_init(&style_focus);
    lv_style_set_outline_width(&style_focus, 2);
    lv_style_set_outline_color(&style_focus, COLOR_FOCUS);
    lv_style_set_outline_opa(&style_focus, LV_OPA_COVER);
    lv_style_set_shadow_color(&style_focus, COLOR_ACCENT_SOFT);
    lv_style_set_shadow_width(&style_focus, 12);
    lv_style_set_shadow_spread(&style_focus, 4);

    styles_initialized = true;
}

static void create_status_bar(lv_obj_t *parent, const char *title)
{
    status_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(status_bar);
    lv_obj_add_style(status_bar, &style_status_bar, 0);
    lv_obj_set_width(status_bar, LV_PCT(100));
    lv_obj_set_height(status_bar, 20);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(status_bar, 4, 0);
    lv_obj_set_style_pad_right(status_bar, 4, 0);
    lv_obj_set_style_pad_top(status_bar, 2, 0);
    lv_obj_set_style_pad_bottom(status_bar, 2, 0);

    lv_obj_t *title_label = lv_label_create(status_bar);
    lv_obj_add_style(title_label, &style_title, 0);
    lv_label_set_text(title_label, title);
    lv_obj_set_flex_grow(title_label, 1);

    lv_obj_t *right_box = lv_obj_create(status_bar);
    lv_obj_remove_style_all(right_box);
    lv_obj_set_flex_flow(right_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(right_box, 6, 0);
    lv_obj_set_style_pad_all(right_box, 0, 0);
    lv_obj_set_flex_align(right_box,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    status_time_label = lv_label_create(right_box);
    lv_obj_add_style(status_time_label, &style_label, 0);
    lv_label_set_text(status_time_label, "--:--");

    status_settings_btn = lv_btn_create(right_box);
    lv_obj_remove_style_all(status_settings_btn);
    lv_obj_set_style_bg_opa(status_settings_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(status_settings_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(status_settings_btn, 0, 0);
    lv_obj_set_size(status_settings_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t *icon = lv_label_create(status_settings_btn);
    lv_obj_add_style(icon, &style_label, 0);
    lv_label_set_text(icon, "SET");

    if (status_timer == NULL) {
        status_timer = lv_timer_create(status_timer_cb, 1000, NULL);
    }
}

static void status_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!status_time_label) {
        return;
    }
    int64_t seconds = esp_timer_get_time() / 1000000LL;
    int hours = (seconds / 3600) % 24;
    int minutes = (seconds / 60) % 60;
    lv_label_set_text_fmt(status_time_label, "%02d:%02d", hours, minutes);
}

static float get_sensor_value_by_index(const sensor_data_t *data, int index)
{
    switch (index) {
        case 0: return data->ph;
        case 1: return data->ec;
        case 2: return data->temperature;  // Используем основное поле
        case 3: return data->humidity;     // Используем основное поле
        case 4: return data->lux;
        case 5: return data->co2;
        default: return 0.0f;
    }
}

// Удалено: configure_chart_axes (графики удалены)

static void record_sensor_value(int index, float value)
{
    const sensor_meta_t *meta = &SENSOR_META[index];
    lv_coord_t scaled = (lv_coord_t)lroundf(value * meta->chart_scale);
    sensor_history[index][sensor_history_pos[index]] = scaled;
    sensor_history_pos[index] = (sensor_history_pos[index] + 1) % HISTORY_POINTS;
    if (sensor_history_pos[index] == 0) {
        sensor_history_full[index] = true;
    }

    // УДАЛЕНО: старая система обновления графиков detail_chart
    // Теперь графики обновляются в update_sensor_display() через detail_screens[].chart
}

static void update_status_badge(int index, float value)
{
    if (!status_labels[index]) {
        return;
    }
    
    const sensor_meta_t *meta = &SENSOR_META[index];
    lv_obj_t *label = status_labels[index];
    lv_color_t bg = COLOR_ACCENT_SOFT;
    lv_color_t text = COLOR_TEXT;
    const char *text_str = "Normal";

    bool danger = (threshold_defined(meta->danger_low) && value < meta->danger_low) ||
                  (threshold_defined(meta->danger_high) && value > meta->danger_high);
    bool warning = (threshold_defined(meta->warn_low) && value < meta->warn_low) ||
                   (threshold_defined(meta->warn_high) && value > meta->warn_high);

    if (danger) {
        bg = COLOR_DANGER;
        text_str = "Critical";
        text = COLOR_BG;
    } else if (warning) {
        bg = COLOR_WARNING;
        text_str = "Warning";
        text = COLOR_BG;
    }

    lv_obj_set_style_bg_color(label, bg, 0);
    lv_obj_set_style_text_color(label, text, 0);
    lv_label_set_text(label, text_str);

    if (value_labels[index]) {
        lv_color_t value_color = COLOR_TEXT;
        if (danger) {
            value_color = COLOR_DANGER;
        } else if (warning) {
            value_color = COLOR_WARNING;
        }
        lv_obj_set_style_text_color(value_labels[index], value_color, 0);
    }
}

static void update_detail_view(int index)
{
    if (!lvgl_is_detail_screen_open() || detail_current_index != index) {
        return;
    }

    const sensor_meta_t *meta = &SENSOR_META[index];
    float value = get_sensor_value_by_index(&last_sensor_data, index);

    if (detail_value_label) {
        char buffer[32];
        char format[8];
        snprintf(format, sizeof(format), "%%.%df", meta->decimals);
        snprintf(buffer, sizeof(buffer), format, value);
        lv_label_set_text(detail_value_label, buffer);
    }

    if (detail_status_label) {
        bool danger = (threshold_defined(meta->danger_low) && value < meta->danger_low) ||
                      (threshold_defined(meta->danger_high) && value > meta->danger_high);
        bool warning = (threshold_defined(meta->warn_low) && value < meta->warn_low) ||
                       (threshold_defined(meta->warn_high) && value > meta->warn_high);

        const char *status_text = "Normal";
        lv_color_t status_color = COLOR_ACCENT_SOFT;
        if (danger) {
            status_text = "Critical";
            status_color = COLOR_DANGER;
        } else if (warning) {
            status_text = "Warning";
            status_color = COLOR_WARNING;
        }

        lv_obj_set_style_bg_color(detail_status_label, status_color, 0);
        lv_color_t text_color = (danger || warning) ? COLOR_BG : COLOR_TEXT;
        lv_obj_set_style_text_color(detail_status_label, text_color, 0);
        lv_label_set_text(detail_status_label, status_text);
    }

    // УДАЛЕНО: старая система обновления графиков detail_chart
    // Теперь графики обновляются в update_sensor_display() через detail_screens[].chart
}

static lv_obj_t *create_sensor_card(lv_obj_t *parent, int index)
{
    const sensor_meta_t *meta = &SENSOR_META[index];

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_width(card, LV_PCT(48));
    lv_obj_set_height(card, 90);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_add_style(title_label, &style_label, 0);
    lv_label_set_text(title_label, meta->title);

    lv_obj_t *value = lv_label_create(card);
    lv_obj_add_style(value, &style_value, 0);
    lv_label_set_text(value, "--");
    value_labels[index] = value;

    lv_obj_t *unit = lv_label_create(card);
    lv_obj_add_style(unit, &style_unit, 0);
    lv_label_set_text(unit, meta->unit);

    lv_obj_t *badge = lv_label_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_add_style(badge, &style_badge, 0);
    lv_label_set_text(badge, "Normal");
    status_labels[index] = badge;

    // Добавляем обработчик событий для перехода к детализации
    ESP_LOGI(TAG, "Adding click handler to card %d", index);
    lv_obj_add_event_cb(card, sensor_card_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    ESP_LOGI(TAG, "Card %d is now clickable", index);
    
    // Сохраняем ссылку на карточку для навигации энкодером
    sensor_cards[index] = card;

    return card;
}

/* =============================
 *  CORE UI BUILDERS
 * ============================= */
static void create_main_ui(void)
{
    init_styles();

    // Если главный экран уже создан, просто возвращаемся
    if (main_screen != NULL) {
        ESP_LOGI(TAG, "Main screen already created, skipping recreation");
        return;
    }

    main_screen = lv_scr_act();
    screen_main = main_screen; // Для совместимости
    lv_obj_add_style(main_screen, &style_bg, 0);
    lv_obj_set_style_pad_top(screen_main, 4, 0);
    lv_obj_set_style_pad_bottom(screen_main, 16, 0);
    lv_obj_set_style_pad_left(screen_main, 16, 0);
    lv_obj_set_style_pad_right(screen_main, 16, 0);
    lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(screen_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_main,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    create_status_bar(screen_main, "Hydroponics");

    lv_obj_t *content = lv_obj_create(screen_main);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_style_pad_column(content, 8, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    int focus_to_restore = (current_focus_index >= 0) ? current_focus_index : 0;

    if (encoder_group == NULL) {
        encoder_group = lv_group_create();
        lv_group_set_wrap(encoder_group, true);
        
        // Устанавливаем группу для энкодера
        lv_indev_t *encoder_indev = lcd_ili9341_get_encoder_indev();
        if (encoder_indev) {
            lv_indev_set_group(encoder_indev, encoder_group);
            ESP_LOGI(TAG, "Encoder group set for main screen");
        }
    }
    lvgl_clear_focus_group();
    
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        sensor_containers[i] = create_sensor_card(content, i);
        if (encoder_group) {
            lv_group_add_obj(encoder_group, sensor_containers[i]);
        }
    }

    lvgl_set_focus(focus_to_restore);

    if (sensor_snapshot_valid) {
        update_sensor_display(&last_sensor_data);
    }

    if (sensor_data_queue == NULL) {
        sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
    }

    if (!display_task_started) {
        xTaskCreate(display_update_task, "display_update", 4096, NULL, 6, NULL);
        display_task_started = true;
    }
}

static void create_detail_ui(int index)
{
    const sensor_meta_t *meta = &SENSOR_META[index];

    screen_detail = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_detail);
    lv_obj_add_style(screen_detail, &style_bg, 0);
    lv_obj_set_style_pad_all(screen_detail, 20, 0);
    lv_obj_set_flex_flow(screen_detail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_detail,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    create_status_bar(screen_detail, meta->title);

    lv_obj_t *body = lv_obj_create(screen_detail);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_flex_grow(body, 1);

    lv_obj_t *value_box = lv_obj_create(body);
    lv_obj_remove_style_all(value_box);
    lv_obj_set_width(value_box, LV_PCT(100));
    lv_obj_set_flex_flow(value_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_box,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    detail_value_label = lv_label_create(value_box);
    lv_obj_add_style(detail_value_label, &style_value_large, 0);
    lv_label_set_text(detail_value_label, "--");

    detail_status_label = lv_label_create(value_box);
    lv_obj_remove_style_all(detail_status_label);
    lv_obj_add_style(detail_status_label, &style_badge, 0);
    lv_label_set_text(detail_status_label, "Normal");

    // Информация о диапазонах
    lv_obj_t *info_container = lv_obj_create(body);
    lv_obj_remove_style_all(info_container);
    lv_obj_add_style(info_container, &style_card, 0);
    lv_obj_set_width(info_container, LV_PCT(100));
    lv_obj_set_height(info_container, 100);
    lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(info_container, 16, 0);

    lv_obj_t *range_label = lv_label_create(body);
    lv_obj_add_style(range_label, &style_label, 0);
    float range_low = threshold_defined(meta->warn_low) ? meta->warn_low : meta->chart_min;
    float range_high = threshold_defined(meta->warn_high) ? meta->warn_high : meta->chart_max;
    
    // Форматируем строку с правильными типами данных
    char range_text[128];
    snprintf(range_text, sizeof(range_text), "Target: %.*f - %.*f %s",
             (int)meta->decimals, (double)range_low,
             (int)meta->decimals, (double)range_high,
             meta->unit ? meta->unit : "");
    lv_label_set_text(range_label, range_text);

    lv_obj_t *desc_label = lv_label_create(body);
    lv_obj_add_style(desc_label, &style_label, 0);
    lv_label_set_text(desc_label, meta->description ? meta->description : "");
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc_label, LV_PCT(100));

    lv_obj_t *hint = lv_label_create(body);
    lv_obj_add_style(hint, &style_label, 0);
    lv_label_set_text(hint, "Press the encoder button to go back");

    detail_current_index = index;
    update_detail_view(index);
}

/* =============================
 *  DETAIL SCREEN CONTROL
 * ============================= */
bool lvgl_is_detail_screen_open(void)
{
    return screen_detail != NULL && lv_scr_act() == screen_detail;
}

void lvgl_open_detail_screen(int index)
{
    if (!lv_is_initialized()) {
        return;
    }

    int focus_before = current_focus_index;

    if (lvgl_is_detail_screen_open()) {
        lvgl_close_detail_screen();
    }

    if (!lvgl_lock(1000)) {
        return;
    }

    create_detail_ui(index);
    lv_screen_load_anim(screen_detail, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    lvgl_unlock();

    current_focus_index = focus_before;
}

void lvgl_close_detail_screen(void)
{
    if (!lv_is_initialized()) {
        return;
    }
    if (!lvgl_lock(1000)) {
        return;
    }
    
    if (screen_detail) {
        lv_obj_del_async(screen_detail);
        screen_detail = NULL;
        detail_value_label = NULL;
        detail_status_label = NULL;
        detail_chart = NULL;
        detail_series = NULL;
        detail_current_index = -1;
    }

    // Просто загружаем существующий главный экран, не пересоздавая его
    if (main_screen) {
        lv_screen_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
    }
    
    lvgl_unlock();
}

/* =============================
 *  FOCUS MANAGEMENT
 * ============================= */
void lvgl_set_focus(int index)
{
    if (index < 0 || index >= SENSOR_COUNT) {
        ESP_LOGW(TAG, "Invalid focus index: %d", index);
        return;
    }
    
    if (current_focus_index >= 0 && current_focus_index < SENSOR_COUNT) {
        if (sensor_containers[current_focus_index]) {
            lv_obj_remove_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
        }
    }

    current_focus_index = index;
    if (sensor_containers[index]) {
        lv_obj_add_style(sensor_containers[index], &style_focus, LV_PART_MAIN);
        lv_obj_scroll_to_view_recursive(sensor_containers[index], LV_ANIM_OFF);
        if (encoder_group) {
            lv_group_focus_obj(sensor_containers[index]);
        }
    }

    if (lvgl_is_detail_screen_open()) {
        update_detail_view(index);
    }
}

int lvgl_get_focus_index(void)
{
    return current_focus_index;
}

int lvgl_get_total_focus_items(void)
{
    return SENSOR_COUNT;
}

void lvgl_clear_focus_group(void)
{
    if (encoder_group) {
        lv_group_remove_all_objs(encoder_group);
    }
    current_focus_index = -1;
}

static void set_encoder_group(lv_group_t *group)
{
    lv_indev_t *encoder_indev = lcd_ili9341_get_encoder_indev();
    if (encoder_indev && group) {
        lv_indev_set_group(encoder_indev, group);
        ESP_LOGI(TAG, "Encoder group switched");
    }
}

// Ленивая инициализация экранов датчиков
static void ensure_screen_initialized(int sensor_index, bool is_settings)
{
    if (sensor_index < 0 || sensor_index >= SENSOR_COUNT) return;
    
    if (!is_settings) {
        // Инициализируем экран детализации если еще не инициализирован
        sensor_screen_t *screen = NULL;
        switch (sensor_index) {
            case 0: screen = &ph_detail_screen; if (!screen->is_initialized) ph_detail_screen_init(); break;
            case 1: screen = &ec_detail_screen; if (!screen->is_initialized) ec_detail_screen_init(); break;
            case 2: screen = &temp_detail_screen; if (!screen->is_initialized) temp_detail_screen_init(); break;
            case 3: screen = &humidity_detail_screen; if (!screen->is_initialized) humidity_detail_screen_init(); break;
            case 4: screen = &lux_detail_screen; if (!screen->is_initialized) lux_detail_screen_init(); break;
            case 5: screen = &co2_detail_screen; if (!screen->is_initialized) co2_detail_screen_init(); break;
        }
    } else {
        // Инициализируем экран настроек если еще не инициализирован
        sensor_screen_t *screen = NULL;
        switch (sensor_index) {
            case 0: screen = &ph_settings_screen; if (!screen->is_initialized) ph_settings_screen_init(); break;
            case 1: screen = &ec_settings_screen; if (!screen->is_initialized) ec_settings_screen_init(); break;
            case 2: screen = &temp_settings_screen; if (!screen->is_initialized) temp_settings_screen_init(); break;
            case 3: screen = &humidity_settings_screen; if (!screen->is_initialized) humidity_settings_screen_init(); break;
            case 4: screen = &lux_settings_screen; if (!screen->is_initialized) lux_settings_screen_init(); break;
            case 5: screen = &co2_settings_screen; if (!screen->is_initialized) co2_settings_screen_init(); break;
        }
    }
}

static void switch_to_screen(lv_obj_t *screen, screen_type_t screen_type, lv_group_t *group)
{
    if (screen) {
        // Если уходим с экрана детализации, очищаем указатели на старые графики
        // чтобы предотвратить обновление скрытых графиков
        if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
            if (screen_type < SCREEN_DETAIL_PH || screen_type > SCREEN_DETAIL_CO2) {
                // Уходим с экрана детализации на не-детализацию
                detail_chart = NULL;
                detail_series = NULL;
                detail_current_index = -1;
                ESP_LOGI(TAG, "Cleared detail screen chart references");
            }
        }
        
        lv_screen_load(screen);
        current_screen = screen_type;
        if (group) {
            set_encoder_group(group);
        }
    }
}

/* =============================
 *  SENSOR DATA HANDLING
 * ============================= */
static void update_sensor_display(sensor_data_t *data)
{
    ESP_LOGI(TAG, "=== UPDATE_SENSOR_DISPLAY CALLED ===");
    ESP_LOGI(TAG, "Data: pH=%.2f, EC=%.2f, Temp=%.1f, Hum=%.1f, Lux=%.0f, CO2=%.0f",
             data->ph, data->ec, data->temperature, data->humidity, data->lux, data->co2);
    
    last_sensor_data = *data;
    sensor_snapshot_valid = true;

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (!value_labels[i]) {
            ESP_LOGW(TAG, "value_labels[%d] is NULL!", i);
            continue;
        }

        const sensor_meta_t *meta = &SENSOR_META[i];
        float value = get_sensor_value_by_index(data, i);

        char buffer[32];
        char format[8];
        snprintf(format, sizeof(format), "%%.%df", meta->decimals);
        snprintf(buffer, sizeof(buffer), format, value);
        ESP_LOGI(TAG, "Updating label %d (%s): %s", i, meta->title, buffer);
        lv_label_set_text(value_labels[i], buffer);

        update_status_badge(i, value);
        record_sensor_value(i, value);
    }

    if (lvgl_is_detail_screen_open()) {
        update_detail_view(detail_current_index);
    }
    
    // Обновляем экраны детализации
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (detail_screens[i].screen && !lv_obj_has_flag(detail_screens[i].screen, LV_OBJ_FLAG_HIDDEN)) {
            const sensor_meta_t *meta = &SENSOR_META[i];
            float value = get_sensor_value_by_index(data, i);
            
            // Обновляем текущее значение
            char buffer[32];
            char format[8];
            snprintf(format, sizeof(format), "%%.%df %s", meta->decimals, meta->unit);
            snprintf(buffer, sizeof(buffer), format, value);
            lv_label_set_text(detail_screens[i].current_value_label, buffer);
            
            // Графики удалены для оптимизации памяти
        }
    }
}

static void display_update_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);

    ESP_LOGI(TAG, "=== DISPLAY_UPDATE_TASK STARTED ===");
    
    sensor_data_t sensor_data;
    uint32_t receive_count = 0;
    while (1) {
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            receive_count++;
            ESP_LOGI(TAG, "Received data from queue (count: %lu)", (unsigned long)receive_count);
            
            if (!lvgl_lock(100)) {
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            if (lv_is_initialized()) {
                update_sensor_display(&sensor_data);
            } else {
                ESP_LOGW(TAG, "LVGL not initialized yet!");
            }
            lvgl_unlock();
        } else {
            ESP_LOGD(TAG, "No data in queue (timeout)");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =============================
 *  PUBLIC API
 * ============================= */
void lvgl_main_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    if (lvgl_lock(1000)) {
        create_main_ui();
        lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for UI initialization");
    }
    
    // Создаем группы для экранов детализации и настроек
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (!detail_screen_groups[i]) {
            detail_screen_groups[i] = lv_group_create();
            lv_group_set_wrap(detail_screen_groups[i], true);
        }
        if (!settings_screen_groups[i]) {
            settings_screen_groups[i] = lv_group_create();
            lv_group_set_wrap(settings_screen_groups[i], true);
        }
    }
    
    // Инициализируем экраны датчиков (будет выполнено асинхронно)
    // Примечание: инициализация будет происходить при первом открытии экрана
    // чтобы избежать блокировки watchdog
    ESP_LOGI(TAG, "Sensor screen groups created");
    
    // Создаем задачу обработки энкодера
    // Используем собственную обработку вместо стандартной LVGL для кастомной навигации
    xTaskCreate(encoder_task, "encoder_task", 4096, NULL, 5, NULL);
    
    // Добавляем обработчик событий энкодера
    lv_obj_add_event_cb(main_screen, encoder_event_cb, LV_EVENT_ALL, NULL);
    ESP_LOGI(TAG, "Encoder navigation and sensor screens initialized");
}

void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    ESP_LOGI(TAG, "=== LVGL_UPDATE_SENSOR_VALUES ===");
    ESP_LOGI(TAG, "Values: pH=%.2f, EC=%.2f, Temp=%.1f, Hum=%.1f, Lux=%.0f, CO2=%.0f",
             ph, ec, temp, hum, lux, co2);
    
    if (sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "sensor_data_queue is NULL!");
        return;
    }
    
    sensor_data_t sensor_data = {
        .ph = ph,
        .ec = ec,
        .temperature = temp,  // Заполняем основное поле
        .humidity = hum,      // Заполняем основное поле
        .temp = temp,         // Заполняем алиас
        .hum = hum,           // Заполняем алиас
        .lux = lux,
        .co2 = co2,
    };

    if (xQueueSend(sensor_data_queue, &sensor_data, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, replacing oldest data");
        sensor_data_t oldest;
        xQueueReceive(sensor_data_queue, &oldest, 0);
        xQueueSend(sensor_data_queue, &sensor_data, 0);
    } else {
        ESP_LOGI(TAG, "Data sent to queue successfully");
    }
    
    // Обновляем данные в экранах датчиков
    ph_update_data(ph, 6.8f);  // Целевое значение pH
    ec_update_data(ec, 1.5f);  // Целевое значение EC
    temp_update_data(temp, 24.0f);  // Целевое значение температуры
    humidity_update_data(hum, 70.0f);  // Целевое значение влажности
    lux_update_data(lux, 500.0f);  // Целевое значение освещенности
    co2_update_data(co2, 450.0f);  // Целевое значение CO2
}

void lvgl_update_sensor_values_from_queue(sensor_data_t *data)
{
    if (sensor_data_queue == NULL) {
        return;
    }
    if (xQueueSend(sensor_data_queue, data, 0) != pdTRUE) {
        sensor_data_t oldest;
        xQueueReceive(sensor_data_queue, &oldest, 0);
        xQueueSend(sensor_data_queue, data, 0);
    }
}

/* =============================
 *  UI NAVIGATION FUNCTIONS
 * ============================= */

// Обработчик события клика по карточке сенсора
static void sensor_card_event_cb(lv_event_t *e)
{
    uint8_t sensor_index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "=== SENSOR CARD CLICKED: %d ===", sensor_index);
    ESP_LOGI(TAG, "Current screen: %d", current_screen);
    ESP_LOGI(TAG, "Encoder navigation enabled: %s", encoder_navigation_enabled ? "true" : "false");
    
    // Создаем экран детализации если еще не создан
    if (detail_screens[sensor_index].screen == NULL) {
        ESP_LOGI(TAG, "Creating detail screen for sensor %d", sensor_index);
        create_detail_screen(sensor_index);
    } else {
        ESP_LOGI(TAG, "Detail screen for sensor %d already exists", sensor_index);
    }
    
    // Переключаемся на экран детализации
    screen_type_t detail_screen = SCREEN_DETAIL_PH + sensor_index;
    ESP_LOGI(TAG, "Switching to detail screen: %d", detail_screen);
    show_screen(detail_screen);
    ESP_LOGI(TAG, "Screen switch completed");
}

// Создание экрана детализации для сенсора
static void create_detail_screen(uint8_t sensor_index)
{
    const sensor_meta_t *meta = &SENSOR_META[sensor_index];
    detail_screen_t *detail = &detail_screens[sensor_index];
    
    // Создаем экран
    detail->screen = lv_obj_create(NULL);
    detail->sensor_index = sensor_index;
    lv_obj_clean(detail->screen);
    lv_obj_add_style(detail->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(detail->screen, 16, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(detail->screen);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text_fmt(title, "%s Details", meta->title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Кнопка назад
    detail->back_btn = lv_btn_create(detail->screen);
    lv_obj_add_style(detail->back_btn, &style_card, 0);
    lv_obj_set_size(detail->back_btn, 60, 30);
    lv_obj_align(detail->back_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(detail->back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(detail->back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_center(back_label);
    
    // Информационная панель (график удален для оптимизации памяти)
    lv_obj_t *info_panel = lv_obj_create(detail->screen);
    lv_obj_add_style(info_panel, &style_card, 0);
    lv_obj_set_size(info_panel, 280, 120);
    lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, 40);
    
    lv_obj_t *info_text = lv_label_create(info_panel);
    lv_obj_add_style(info_text, &style_value_small, 0);
    lv_label_set_text(info_text, meta->description);
    lv_label_set_long_mode(info_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_text, 250);
    lv_obj_center(info_text);
    
    // Текущее значение
    lv_obj_t *current_label = lv_label_create(detail->screen);
    lv_obj_add_style(current_label, &style_label, 0);
    lv_label_set_text(current_label, "Current:");
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 0, 170);
    
    detail->current_value_label = lv_label_create(detail->screen);
    lv_obj_add_style(detail->current_value_label, &style_value_large, 0);
    lv_label_set_text(detail->current_value_label, "--");
    lv_obj_align(detail->current_value_label, LV_ALIGN_TOP_LEFT, 80, 170);
    
    // Установленное значение
    lv_obj_t *set_label = lv_label_create(detail->screen);
    lv_obj_add_style(set_label, &style_label, 0);
    lv_label_set_text(set_label, "Set:");
    lv_obj_align(set_label, LV_ALIGN_TOP_LEFT, 0, 200);
    
    detail->set_value_label = lv_label_create(detail->screen);
    lv_obj_add_style(detail->set_value_label, &style_value, 0);
    
    // Форматируем строку с правильными типами данных
    char set_value_text[64];
    snprintf(set_value_text, sizeof(set_value_text), "%.*f %s", 
             (int)meta->decimals, 
             (double)((meta->warn_low + meta->warn_high) / 2.0f), 
             meta->unit ? meta->unit : "");
    lv_label_set_text(detail->set_value_label, set_value_text);
    lv_obj_align(detail->set_value_label, LV_ALIGN_TOP_LEFT, 80, 200);
    
    // Кнопка настроек
    detail->settings_btn = lv_btn_create(detail->screen);
    lv_obj_add_style(detail->settings_btn, &style_card, 0);
    lv_obj_set_size(detail->settings_btn, 120, 40);
    lv_obj_align(detail->settings_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(detail->settings_btn, settings_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)sensor_index);
    
    lv_obj_t *settings_label = lv_label_create(detail->settings_btn);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);
    
    ESP_LOGI(TAG, "Detail screen created for sensor %d", sensor_index);
}

// Создание экрана настроек для сенсора
static void create_settings_screen(uint8_t sensor_index)
{
    const sensor_meta_t *meta = &SENSOR_META[sensor_index];
    settings_screen_t *settings = &settings_screens[sensor_index];
    
    // Создаем экран
    settings->screen = lv_obj_create(NULL);
    settings->sensor_index = sensor_index;
    lv_obj_clean(settings->screen);
    lv_obj_add_style(settings->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(settings->screen, 16, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(settings->screen);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text_fmt(title, "%s Settings", meta->title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Кнопка назад
    settings->back_btn = lv_btn_create(settings->screen);
    lv_obj_add_style(settings->back_btn, &style_card, 0);
    lv_obj_set_size(settings->back_btn, 60, 30);
    lv_obj_align(settings->back_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(settings->back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(settings->back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_center(back_label);
    
    // Список настроек (заглушки)
    settings->settings_list = lv_obj_create(settings->screen);
    lv_obj_remove_style_all(settings->settings_list);
    lv_obj_add_style(settings->settings_list, &style_card, 0);
    lv_obj_set_size(settings->settings_list, 280, 200);
    lv_obj_align(settings->settings_list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(settings->settings_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings->settings_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(settings->settings_list, 16, 0);
    lv_obj_set_style_pad_row(settings->settings_list, 8, 0);
    
    // Заглушки настроек
    const char* settings_items[] = {
        "Calibration",
        "Alarm Thresholds", 
        "Update Interval",
        "Display Units",
        "Data Logging"
    };
    
    for (int i = 0; i < 5; i++) {
        lv_obj_t *item = lv_btn_create(settings->settings_list);
        lv_obj_add_style(item, &style_card, 0);
        lv_obj_set_size(item, 240, 30);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_center(item_label);
        
        // Добавляем индикатор "заглушка"
        lv_obj_t *placeholder = lv_label_create(item);
        lv_obj_add_style(placeholder, &style_unit, 0);
        lv_label_set_text(placeholder, "→");
        lv_obj_align(placeholder, LV_ALIGN_RIGHT_MID, -10, 0);
    }
    
    ESP_LOGI(TAG, "Settings screen created for sensor %d", sensor_index);
}

// Переключение экранов
static void show_screen(screen_type_t screen)
{
    ESP_LOGI(TAG, "=== SHOW_SCREEN: %d ===", screen);
    
    // Если уходим с экрана детализации, очищаем указатели на старые графики
    // чтобы предотвратить обновление скрытых графиков
    if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
        if (screen < SCREEN_DETAIL_PH || screen > SCREEN_DETAIL_CO2) {
            // Уходим с экрана детализации на не-детализацию
            detail_chart = NULL;
            detail_series = NULL;
            detail_current_index = -1;
            ESP_LOGI(TAG, "Cleared detail screen chart references (old path)");
        }
    }
    
    current_screen = screen;
    
    // Скрываем все экраны
    ESP_LOGI(TAG, "Hiding all screens");
    lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (detail_screens[i].screen) {
            lv_obj_add_flag(detail_screens[i].screen, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Hidden detail screen %d", i);
        }
        if (settings_screens[i].screen) {
            lv_obj_add_flag(settings_screens[i].screen, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Hidden settings screen %d", i);
        }
    }
    
    // Показываем нужный экран
    switch (screen) {
        case SCREEN_MAIN:
            ESP_LOGI(TAG, "Showing main screen");
            lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
            lv_screen_load(main_screen);
            break;
            
        case SCREEN_DETAIL_PH:
        case SCREEN_DETAIL_EC:
        case SCREEN_DETAIL_TEMP:
        case SCREEN_DETAIL_HUMIDITY:
        case SCREEN_DETAIL_LUX:
        case SCREEN_DETAIL_CO2: {
            uint8_t sensor_index = screen - SCREEN_DETAIL_PH;
            ESP_LOGI(TAG, "Showing detail screen for sensor %d", sensor_index);
            if (detail_screens[sensor_index].screen) {
                lv_obj_clear_flag(detail_screens[sensor_index].screen, LV_OBJ_FLAG_HIDDEN);
                lv_screen_load(detail_screens[sensor_index].screen);
                ESP_LOGI(TAG, "Detail screen %d loaded successfully", sensor_index);
            } else {
                ESP_LOGE(TAG, "Detail screen %d is NULL!", sensor_index);
            }
            break;
        }
        
        case SCREEN_SETTINGS_PH:
        case SCREEN_SETTINGS_EC:
        case SCREEN_SETTINGS_TEMP:
        case SCREEN_SETTINGS_HUMIDITY:
        case SCREEN_SETTINGS_LUX:
        case SCREEN_SETTINGS_CO2: {
            uint8_t sensor_index = screen - SCREEN_SETTINGS_PH;
            if (settings_screens[sensor_index].screen) {
                lv_obj_clear_flag(settings_screens[sensor_index].screen, LV_OBJ_FLAG_HIDDEN);
                lv_screen_load(settings_screens[sensor_index].screen);
            }
            break;
        }
        
        default:
            break;
    }
    
    ESP_LOGI(TAG, "Switched to screen %d", screen);
    
    // Обновляем выделение в зависимости от экрана
    if (screen == SCREEN_MAIN) {
        update_card_selection();
    } else if (screen >= SCREEN_SETTINGS_PH && screen <= SCREEN_SETTINGS_CO2) {
        update_settings_selection();
    }
}

// Обработчик кнопки "Назад"
static void back_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e); // Не используем e напрямую
    
    switch (current_screen) {
        case SCREEN_MAIN:
            // Уже на главном экране
            break;
            
        case SCREEN_DETAIL_PH:
        case SCREEN_DETAIL_EC:
        case SCREEN_DETAIL_TEMP:
        case SCREEN_DETAIL_HUMIDITY:
        case SCREEN_DETAIL_LUX:
        case SCREEN_DETAIL_CO2:
            // Возвращаемся на главный экран
            show_screen(SCREEN_MAIN);
            break;
            
        case SCREEN_SETTINGS_PH:
        case SCREEN_SETTINGS_EC:
        case SCREEN_SETTINGS_TEMP:
        case SCREEN_SETTINGS_HUMIDITY:
        case SCREEN_SETTINGS_LUX:
        case SCREEN_SETTINGS_CO2: {
            // Возвращаемся к экрану детализации
            uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
            screen_type_t detail_screen = SCREEN_DETAIL_PH + sensor_index;
            show_screen(detail_screen);
            break;
        }
        
        default:
            break;
    }
}

// Обработчик кнопки "Настройки"
static void settings_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e); // Не используем e напрямую, только user_data
    uint8_t sensor_index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "Settings button clicked for sensor %d", sensor_index);
    
    // Создаем экран настроек если еще не создан
    if (settings_screens[sensor_index].screen == NULL) {
        create_settings_screen(sensor_index);
    }
    
    // Переключаемся на экран настроек
    screen_type_t settings_screen = SCREEN_SETTINGS_PH + sensor_index;
    show_screen(settings_screen);
}

/* =============================
 *  ENCODER NAVIGATION FUNCTIONS
 * ============================= */

// Задача обработки энкодера
static void encoder_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);
    
    QueueHandle_t encoder_queue = encoder_get_event_queue();
    if (encoder_queue == NULL) {
        ESP_LOGE(TAG, "Encoder queue not available");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Encoder task started, waiting for events...");
    
    encoder_event_t event;
    while (1) {
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "⚡ Encoder event received: type=%d, value=%d", event.type, event.value);
            
            if (!lvgl_lock(100)) {
                ESP_LOGW(TAG, "Failed to acquire LVGL lock for encoder event");
                continue;
            }
            
            if (lv_is_initialized()) {
                ESP_LOGI(TAG, "📍 Current screen: %d, nav_enabled: %d", current_screen, encoder_navigation_enabled);
                handle_encoder_event(&event);
            }
            lvgl_unlock();
        }
    }
}

// Обработка событий энкодера
static void handle_encoder_event(encoder_event_t *event)
{
    if (!encoder_navigation_enabled) {
        return;
    }
    
    switch (event->type) {
        case ENCODER_EVENT_ROTATE_CW:
            ESP_LOGI(TAG, "Encoder CW rotation");
            if (current_screen == SCREEN_MAIN) {
                selected_card_index = (selected_card_index + 1) % SENSOR_COUNT;
                update_card_selection();
            } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                selected_settings_item = (selected_settings_item + 1) % 5; // 5 пунктов настроек
                update_settings_selection();
            }
            break;
            
        case ENCODER_EVENT_ROTATE_CCW:
            ESP_LOGI(TAG, "Encoder CCW rotation");
            if (current_screen == SCREEN_MAIN) {
                selected_card_index = (selected_card_index - 1 + SENSOR_COUNT) % SENSOR_COUNT;
                update_card_selection();
            } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                selected_settings_item = (selected_settings_item - 1 + 5) % 5;
                update_settings_selection();
            }
            break;
            
        case ENCODER_EVENT_BUTTON_PRESS:
            ESP_LOGI(TAG, "Encoder button press");
            if (current_screen == SCREEN_MAIN) {
                // Переходим к экрану детализации выбранной карточки
                if (detail_screens[selected_card_index].screen == NULL) {
                    create_detail_screen(selected_card_index);
                }
                screen_type_t detail_screen = SCREEN_DETAIL_PH + selected_card_index;
                show_screen(detail_screen);
            } else if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
                // На экране детализации - переходим к настройкам
                uint8_t sensor_index = current_screen - SCREEN_DETAIL_PH;
                if (settings_screens[sensor_index].screen == NULL) {
                    create_settings_screen(sensor_index);
                }
                screen_type_t settings_screen = SCREEN_SETTINGS_PH + sensor_index;
                show_screen(settings_screen);
            }
            break;
            
        case ENCODER_EVENT_BUTTON_LONG_PRESS:
            ESP_LOGI(TAG, "Encoder button long press - going back");
            // Длинное нажатие - возврат назад
            if (current_screen == SCREEN_MAIN) {
                // Уже на главном экране
            } else if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
                show_screen(SCREEN_MAIN);
            } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
                screen_type_t detail_screen = SCREEN_DETAIL_PH + sensor_index;
                show_screen(detail_screen);
            }
            break;
            
        case ENCODER_EVENT_BUTTON_RELEASE:
            // Обработка отпускания кнопки (если нужно)
            break;
            
        default:
            break;
    }
}

// Обновление выделения карточек на главном экране
static void update_card_selection(void)
{
    ESP_LOGI(TAG, "🎯 update_card_selection called: selected=%d, current_screen=%d", selected_card_index, current_screen);
    
    if (current_screen != SCREEN_MAIN) {
        ESP_LOGW(TAG, "Not on main screen, skipping card selection update");
        return;
    }
    
    // Сбрасываем выделение всех карточек
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_cards[i]) {
            lv_obj_clear_state(sensor_cards[i], LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(sensor_cards[i], COLOR_CARD, 0);
            lv_obj_set_style_border_color(sensor_cards[i], COLOR_SHADOW, 0);
            lv_obj_set_style_border_width(sensor_cards[i], 1, 0);
            ESP_LOGI(TAG, "  Card %d: focus cleared", i);
        } else {
            ESP_LOGW(TAG, "  Card %d: NULL pointer!", i);
        }
    }
    
    // Выделяем выбранную карточку
    if (sensor_cards[selected_card_index]) {
        lv_obj_add_state(sensor_cards[selected_card_index], LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(sensor_cards[selected_card_index], COLOR_ACCENT_SOFT, 0);
        lv_obj_set_style_border_color(sensor_cards[selected_card_index], COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(sensor_cards[selected_card_index], 2, 0);
        ESP_LOGI(TAG, "✅ Card %d: FOCUSED and highlighted", selected_card_index);
    } else {
        ESP_LOGE(TAG, "❌ Selected card %d is NULL!", selected_card_index);
    }
}

// Обновление выделения пунктов настроек
static void update_settings_selection(void)
{
    if (current_screen < SCREEN_SETTINGS_PH || current_screen > SCREEN_SETTINGS_CO2) {
        return;
    }
    
    uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
    settings_screen_t *settings = &settings_screens[sensor_index];
    
    if (!settings->settings_list) {
        return;
    }
    
    // Сбрасываем выделение всех пунктов
    lv_obj_t *child = lv_obj_get_child(settings->settings_list, 0);
    int i = 0;
    while (child) {
        lv_obj_clear_state(child, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(child, COLOR_CARD, 0);
        lv_obj_set_style_border_color(child, COLOR_SHADOW, 0);
        lv_obj_set_style_border_width(child, 1, 0);
        
        child = lv_obj_get_child(settings->settings_list, ++i);
    }
    
    // Выделяем выбранный пункт
    child = lv_obj_get_child(settings->settings_list, selected_settings_item);
    if (child) {
        lv_obj_add_state(child, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(child, COLOR_ACCENT_SOFT, 0);
        lv_obj_set_style_border_color(child, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(child, 2, 0);
    }
    
    ESP_LOGI(TAG, "Selected settings item: %d", selected_settings_item);
}

// Обработчик событий энкодера
static void encoder_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = lv_event_get_key(e);
    
    ESP_LOGI(TAG, "Encoder event: code=%d, key=%"PRIu32, code, key);
    
    // Обрабатываем события кнопки энкодера
    if (code == LV_EVENT_KEY) {
        switch (key) {
            case LV_KEY_ENTER:
                ESP_LOGI(TAG, "ENTER key pressed");
                if (current_screen == SCREEN_MAIN) {
                    // Инициализируем экран детализации если нужно
                    ensure_screen_initialized(selected_card_index, false);
                    
                    // Переходим к экрану детализации выбранной карточки
                    sensor_screen_t *detail_screen = NULL;
                    screen_type_t new_screen_type = SCREEN_MAIN;
                    
                    switch (selected_card_index) {
                        case 0: detail_screen = &ph_detail_screen; new_screen_type = SCREEN_DETAIL_PH; break;
                        case 1: detail_screen = &ec_detail_screen; new_screen_type = SCREEN_DETAIL_EC; break;
                        case 2: detail_screen = &temp_detail_screen; new_screen_type = SCREEN_DETAIL_TEMP; break;
                        case 3: detail_screen = &humidity_detail_screen; new_screen_type = SCREEN_DETAIL_HUMIDITY; break;
                        case 4: detail_screen = &lux_detail_screen; new_screen_type = SCREEN_DETAIL_LUX; break;
                        case 5: detail_screen = &co2_detail_screen; new_screen_type = SCREEN_DETAIL_CO2; break;
                    }
                    
                    if (detail_screen && detail_screen->screen) {
                        switch_to_screen(detail_screen->screen, new_screen_type, detail_screen_groups[selected_card_index]);
                    }
                } else if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
                    // На экране детализации - переходим к настройкам
                    int sensor_index = current_screen - SCREEN_DETAIL_PH;
                    ensure_screen_initialized(sensor_index, true);
                    
                    sensor_screen_t *settings_screen = NULL;
                    screen_type_t new_screen_type = SCREEN_MAIN;
                    
                    switch (sensor_index) {
                        case 0: settings_screen = &ph_settings_screen; new_screen_type = SCREEN_SETTINGS_PH; break;
                        case 1: settings_screen = &ec_settings_screen; new_screen_type = SCREEN_SETTINGS_EC; break;
                        case 2: settings_screen = &temp_settings_screen; new_screen_type = SCREEN_SETTINGS_TEMP; break;
                        case 3: settings_screen = &humidity_settings_screen; new_screen_type = SCREEN_SETTINGS_HUMIDITY; break;
                        case 4: settings_screen = &lux_settings_screen; new_screen_type = SCREEN_SETTINGS_LUX; break;
                        case 5: settings_screen = &co2_settings_screen; new_screen_type = SCREEN_SETTINGS_CO2; break;
                    }
                    
                    if (settings_screen && settings_screen->screen) {
                        switch_to_screen(settings_screen->screen, new_screen_type, settings_screen_groups[sensor_index]);
                    }
                }
                break;
                
            case LV_KEY_ESC:
                ESP_LOGI(TAG, "ESC key pressed - going back");
                if (current_screen == SCREEN_MAIN) {
                    // Уже на главном экране
                } else if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
                    // Возвращаемся к главному экрану
                    switch_to_screen(main_screen, SCREEN_MAIN, encoder_group);
                } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                    // Возвращаемся к экрану детализации
                    int sensor_index = current_screen - SCREEN_SETTINGS_PH;
                    sensor_screen_t *detail_screen = NULL;
                    screen_type_t new_screen_type = SCREEN_MAIN;
                    
                    switch (sensor_index) {
                        case 0: detail_screen = &ph_detail_screen; new_screen_type = SCREEN_DETAIL_PH; break;
                        case 1: detail_screen = &ec_detail_screen; new_screen_type = SCREEN_DETAIL_EC; break;
                        case 2: detail_screen = &temp_detail_screen; new_screen_type = SCREEN_DETAIL_TEMP; break;
                        case 3: detail_screen = &humidity_detail_screen; new_screen_type = SCREEN_DETAIL_HUMIDITY; break;
                        case 4: detail_screen = &lux_detail_screen; new_screen_type = SCREEN_DETAIL_LUX; break;
                        case 5: detail_screen = &co2_detail_screen; new_screen_type = SCREEN_DETAIL_CO2; break;
                    }
                    
                    if (detail_screen && detail_screen->screen) {
                        switch_to_screen(detail_screen->screen, new_screen_type, detail_screen_groups[sensor_index]);
                    }
                }
                break;
        }
    }
    
    // Обрабатываем события вращения энкодера
    if (code == LV_EVENT_VALUE_CHANGED) {
        ESP_LOGI(TAG, "Encoder value changed event");
        
        // Используем глобальную переменную для отслеживания вращения
        if (last_encoder_diff > 0) {
            ESP_LOGI(TAG, "CW rotation");
            if (current_screen == SCREEN_MAIN) {
                selected_card_index = (selected_card_index + 1) % SENSOR_COUNT;
                update_card_selection();
            } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                selected_settings_item = (selected_settings_item + 1) % 5;
                update_settings_selection();
            }
            last_encoder_diff = 0; // Сбрасываем после обработки
        } else if (last_encoder_diff < 0) {
            ESP_LOGI(TAG, "CCW rotation");
            if (current_screen == SCREEN_MAIN) {
                selected_card_index = (selected_card_index - 1 + SENSOR_COUNT) % SENSOR_COUNT;
                update_card_selection();
            } else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
                selected_settings_item = (selected_settings_item - 1 + 5) % 5;
                update_settings_selection();
            }
            last_encoder_diff = 0; // Сбрасываем после обработки
        }
    }
}
