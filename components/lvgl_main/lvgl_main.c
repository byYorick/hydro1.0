#include "lvgl_main.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
LV_FONT_DECLARE(lv_font_montserrat_14)
LV_FONT_DECLARE(lv_font_montserrat_18)
LV_FONT_DECLARE(lv_font_montserrat_20)

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
static lv_style_t style_unit;
static lv_style_t style_focus;
static lv_style_t style_card;
static lv_style_t style_status_bar;
static lv_style_t style_badge;
static bool styles_initialized = false;

static lv_group_t *encoder_group = NULL;
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
static lv_chart_series_t *populate_chart_with_history(lv_obj_t *chart, lv_chart_series_t *series, int index);
static void update_detail_view(int index);
static void update_sensor_display(sensor_data_t *data);
static void display_update_task(void *pvParameters);
static lv_obj_t *create_sensor_card(lv_obj_t *parent, int index);

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

    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14);

    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, COLOR_TEXT);
    lv_style_set_text_font(&style_value, &lv_font_montserrat_20);

    lv_style_init(&style_value_large);
    lv_style_set_text_color(&style_value_large, COLOR_TEXT);
    lv_style_set_text_font(&style_value_large, &lv_font_montserrat_20);

    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_14);

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
        case 2: return data->temp;
        case 3: return data->hum;
        case 4: return data->lux;
        case 5: return data->co2;
        default: return 0.0f;
    }
}

static void configure_chart_axes(lv_obj_t *chart, int index)
{
    const sensor_meta_t *meta = &SENSOR_META[index];
    lv_coord_t min = (lv_coord_t)(meta->chart_min * meta->chart_scale);
    lv_coord_t max = (lv_coord_t)(meta->chart_max * meta->chart_scale);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
    lv_chart_set_div_line_count(chart, 4, 5);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);
}

static lv_chart_series_t *populate_chart_with_history(lv_obj_t *chart, lv_chart_series_t *series, int index)
{
    if (!chart) {
        return series;
    }

    configure_chart_axes(chart, index);
    lv_chart_set_point_count(chart, HISTORY_POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_color_t color = COLOR_ACCENT;
    if (series) {
        color = series->color;
        lv_chart_remove_series(chart, series);
    }
    series = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);

    uint16_t count = sensor_history_full[index] ? HISTORY_POINTS : sensor_history_pos[index];
    if (count == 0) {
        return series;
    }

    uint16_t start = sensor_history_full[index] ? sensor_history_pos[index] : 0;
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t pos = (start + i) % HISTORY_POINTS;
        lv_chart_set_next_value(chart, series, sensor_history[index][pos]);
    }
    return series;
}

static void record_sensor_value(int index, float value)
{
    const sensor_meta_t *meta = &SENSOR_META[index];
    lv_coord_t scaled = (lv_coord_t)lroundf(value * meta->chart_scale);
    sensor_history[index][sensor_history_pos[index]] = scaled;
    sensor_history_pos[index] = (sensor_history_pos[index] + 1) % HISTORY_POINTS;
    if (sensor_history_pos[index] == 0) {
        sensor_history_full[index] = true;
    }

    if (detail_chart && detail_current_index == index) {
        detail_series = populate_chart_with_history(detail_chart, detail_series, index);
    }
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

    detail_series = populate_chart_with_history(detail_chart, detail_series, index);
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

    return card;
}

/* =============================
 *  CORE UI BUILDERS
 * ============================= */
static void create_main_ui(void)
{
    init_styles();

    screen_main = lv_scr_act();
    lv_obj_clean(screen_main);
    lv_obj_add_style(screen_main, &style_bg, 0);
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

    detail_chart = lv_chart_create(body);
    lv_obj_set_height(detail_chart, 150);
    lv_obj_add_style(detail_chart, &style_card, LV_PART_MAIN);
    lv_obj_set_style_pad_all(detail_chart, 16, LV_PART_MAIN);
    lv_chart_set_type(detail_chart, LV_CHART_TYPE_LINE);
    lv_obj_set_flex_grow(detail_chart, 1);
    lv_obj_set_style_line_width(detail_chart, 3, LV_PART_INDICATOR);
    detail_series = lv_chart_add_series(detail_chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_t *range_label = lv_label_create(body);
    lv_obj_add_style(range_label, &style_label, 0);
    float range_low = threshold_defined(meta->warn_low) ? meta->warn_low : meta->chart_min;
    float range_high = threshold_defined(meta->warn_high) ? meta->warn_high : meta->chart_max;
    lv_label_set_text_fmt(range_label, "Target: %.*f - %.*f %s",
                          meta->decimals, range_low,
                          meta->decimals, range_high,
                          meta->unit);

    lv_obj_t *desc_label = lv_label_create(body);
    lv_obj_add_style(desc_label, &style_label, 0);
    lv_label_set_text(desc_label, meta->description);
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
    lv_scr_load_anim(screen_detail, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
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

    create_main_ui();
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

/* =============================
 *  SENSOR DATA HANDLING
 * ============================= */
static void update_sensor_display(sensor_data_t *data)
{
    last_sensor_data = *data;
    sensor_snapshot_valid = true;

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (!value_labels[i]) {
            continue;
        }

        const sensor_meta_t *meta = &SENSOR_META[i];
        float value = get_sensor_value_by_index(data, i);

        char buffer[32];
        char format[8];
        snprintf(format, sizeof(format), "%%.%df", meta->decimals);
        snprintf(buffer, sizeof(buffer), format, value);
        lv_label_set_text(value_labels[i], buffer);

        update_status_badge(i, value);
        record_sensor_value(i, value);
    }

    if (lvgl_is_detail_screen_open()) {
        update_detail_view(detail_current_index);
    }
}

static void display_update_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);

    sensor_data_t sensor_data;
    while (1) {
        if (xQueueReceive(sensor_data_queue, &sensor_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (!lvgl_lock(100)) {
                ESP_LOGW(TAG, "Failed to acquire LVGL lock, skipping update");
                continue;
            }
            
            if (lv_is_initialized()) {
                update_sensor_display(&sensor_data);
            }
            lvgl_unlock();
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
}

void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    if (sensor_data_queue == NULL) {
        return;
    }
    
    sensor_data_t sensor_data = {
        .ph = ph,
        .ec = ec,
        .temp = temp,
        .hum = hum,
        .lux = lux,
        .co2 = co2,
    };

    if (xQueueSend(sensor_data_queue, &sensor_data, 0) != pdTRUE) {
        sensor_data_t oldest;
        xQueueReceive(sensor_data_queue, &oldest, 0);
        xQueueSend(sensor_data_queue, &sensor_data, 0);
    }
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
