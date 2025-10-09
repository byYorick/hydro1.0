#include "lvgl_ui.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
#include "encoder.h"
// УДАЛЕНО: #include "ph_screen.h" - все pH экраны через Screen Manager
#include <inttypes.h>

// Screen Manager System
#include "screen_manager/screen_manager.h"
#include "screen_manager/screen_init.h"
#include "screens/main_screen.h"
#include "lvgl_styles.h"
// Используем только встроенные шрифты LVGL

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "data_logger.h"

static const char *TAG = "LVGL_MAIN";

/* =============================
 *  УЛУЧШЕННАЯ ЦВЕТОВАЯ ПАЛИТРА ДЛЯ ГИДРОПОНИКИ
 * ============================= */
#define COLOR_BG            lv_color_hex(0x0F1419)        // Темный фон (как ночное небо)
#define COLOR_SURFACE       lv_color_hex(0x1A2332)        // Поверхности (темно-синий)
#define COLOR_CARD          lv_color_hex(0x2D3E50)        // Карточки (темно-серый)
#define COLOR_ACCENT        lv_color_hex(0x00D4AA)        // Акцент (бирюзовый - цвет растений)
#define COLOR_ACCENT_SOFT   lv_color_hex(0x26E5B3)        // Мягкий акцент (светло-бирюзовый)
#define COLOR_NORMAL        lv_color_hex(0x4CAF50)        // Нормальные значения (зеленый)
#define COLOR_WARNING       lv_color_hex(0xFF9800)        // Предупреждения (оранжевый)
#define COLOR_DANGER        lv_color_hex(0xF44336)        // Опасность (красный)
#define COLOR_TEXT          lv_color_hex(0xFFFFFF)        // Основной текст (белый)
#define COLOR_TEXT_MUTED    lv_color_hex(0xB0BEC5)        // Приглушенный текст (серый)
#define COLOR_FOCUS         lv_color_hex(0x00D4AA)        // Фокус (бирюзовый)
#define COLOR_SHADOW        lv_color_hex(0x000000)        // Тени (черный)
#define COLOR_GRADIENT_START lv_color_hex(0x1A2332)       // Градиент начало
#define COLOR_GRADIENT_END  lv_color_hex(0x0F1419)        // Градиент конец

/* =============================
 *  SENSOR META DATA
 * ============================= */
#define SENSOR_COUNT           6
#define HISTORY_POINTS         60
#define SENSOR_DATA_QUEUE_SIZE 10

/* =============================
 *  UI SCREEN MANAGEMENT
 * ============================= */
/**
 * @brief Перечисление типов экранов пользовательского интерфейса
 * Улучшенная навигация с поддержкой мобильного приложения и сетевых функций
 */
typedef enum {
    SCREEN_MAIN = 0,                    // Главный экран с карточками датчиков
    SCREEN_DETAIL_PH,                   // Детали pH датчика
    SCREEN_DETAIL_EC,                   // Детали EC датчика
    SCREEN_DETAIL_TEMP,                 // Детали температуры
    SCREEN_DETAIL_HUMIDITY,             // Детали влажности
    SCREEN_DETAIL_LUX,                  // Детали освещенности
    SCREEN_DETAIL_CO2,                  // Детали CO2 датчика
    SCREEN_SETTINGS_PH,                 // Настройки pH
    SCREEN_SETTINGS_EC,                 // Настройки EC
    SCREEN_SETTINGS_TEMP,               // Настройки температуры
    SCREEN_SETTINGS_HUMIDITY,           // Настройки влажности
    SCREEN_SETTINGS_LUX,                // Настройки освещенности
    SCREEN_SETTINGS_CO2,                // Настройки CO2

    // Новые экраны для улучшенной функциональности
    SCREEN_SYSTEM_STATUS,               // Общие настройки системы (главное меню настроек)
    SCREEN_AUTO_CONTROL,                // Настройки автоматического управления
    SCREEN_WIFI_SETTINGS,               // Настройки WiFi
    SCREEN_DISPLAY_SETTINGS,            // Настройки дисплея
    SCREEN_DATA_LOGGER_SETTINGS,        // Настройки логирования
    SCREEN_SYSTEM_INFO,                 // Информация о системе
    SCREEN_RESET_CONFIRM,               // Подтверждение сброса настроек
    SCREEN_NETWORK_SETTINGS,            // Настройки сети (WiFi, Bluetooth)
    SCREEN_MOBILE_CONNECT,              // Подключение к мобильному приложению
    SCREEN_OTA_UPDATE,                  // OTA обновления прошивки
    SCREEN_CALIBRATION,                 // Калибровка датчиков (зарезервировано)
    SCREEN_DATA_EXPORT,                 // Экспорт данных
    SCREEN_ABOUT,                       // О программе и системе

    SCREEN_COUNT                        // Общее количество экранов
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
static lv_obj_t *system_settings_screen = NULL;
static lv_group_t *system_settings_group = NULL;

// Экраны подменю системных настроек
static lv_obj_t *auto_control_screen = NULL;
static lv_obj_t *wifi_settings_screen = NULL;
static lv_obj_t *display_settings_screen = NULL;
static lv_obj_t *data_logger_screen = NULL;
static lv_obj_t *system_info_screen = NULL;
static lv_obj_t *reset_confirm_screen = NULL;

// Группы для навигации
static lv_group_t *auto_control_group = NULL;
static lv_group_t *wifi_settings_group = NULL;
static lv_group_t *display_settings_group = NULL;
static lv_group_t *data_logger_group = NULL;
static lv_group_t *system_info_group = NULL;
static lv_group_t *reset_confirm_group = NULL;

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

// ===== ГЛОБАЛЬНЫЕ СТИЛИ (экспортированы в lvgl_styles.h) =====
lv_style_t style_bg;
lv_style_t style_header;
lv_style_t style_title;
lv_style_t style_label;
lv_style_t style_value;
lv_style_t style_value_large;
lv_style_t style_value_small;
lv_style_t style_unit;
lv_style_t style_focus;
lv_style_t style_card;
lv_style_t style_card_focused;
lv_style_t style_status_bar;
lv_style_t style_status_normal;
lv_style_t style_status_warning;
lv_style_t style_status_danger;
lv_style_t style_badge;
lv_style_t style_button;
lv_style_t style_button_pressed;
lv_style_t style_button_secondary;
lv_style_t style_detail_bg;
lv_style_t style_detail_container;
lv_style_t style_detail_title;
lv_style_t style_detail_value;
lv_style_t style_detail_info;
lv_style_t style_detail_value_big;  // Alias для style_detail_value (для совместимости)
static bool styles_initialized = false;

static lv_group_t *encoder_group = NULL;
static lv_group_t *detail_screen_groups[SENSOR_COUNT] = {NULL};
static lv_group_t *settings_screen_groups[SENSOR_COUNT] = {NULL};
static QueueHandle_t sensor_data_queue = NULL;
static int current_focus_index = -1;
static bool display_task_started = false;

// Управление видимостью фокуса
static lv_timer_t *focus_hide_timer = NULL;
static bool focus_visible = true;
#define FOCUS_HIDE_TIMEOUT_MS 30000  // 30 секунд

static sensor_data_t last_sensor_data = {0};
static lv_coord_t sensor_history[SENSOR_COUNT][HISTORY_POINTS];
static uint16_t sensor_history_pos[SENSOR_COUNT];
static bool sensor_history_full[SENSOR_COUNT];
static bool sensor_snapshot_valid = false;

/* =============================
 *  FORWARD DECLARATIONS
 * ============================= */
// init_styles() объявлен в lvgl_styles.h
// LEGACY УДАЛЕНО
static void create_detail_ui(int index);
static void create_status_bar(lv_obj_t *parent, const char *title);
static void status_timer_cb(lv_timer_t *timer);
static float get_sensor_value_by_index(const sensor_data_t *data, int index);
static void record_sensor_value(int index, float value);
// static void configure_chart_axes(lv_obj_t *chart, int index); // Удалено - не используется
// Удалено: populate_chart_with_history (графики удалены)
static void update_detail_view(int index);
static void update_sensor_display(sensor_data_t *data);
static void display_update_task(void *pvParameters);
static lv_obj_t *create_sensor_card(lv_obj_t *parent, int index);
static void sensor_card_event_cb(lv_event_t *e);
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
static void focus_hide_timer_cb(lv_timer_t *timer);
static void show_focus(void);
static void hide_focus(void);
static void reset_focus_timer(void);
static void back_button_event_cb(lv_event_t *e);
static void settings_button_event_cb(lv_event_t *e);
static void system_settings_button_event_cb(lv_event_t *e);
static void system_menu_item_event_cb(lv_event_t *e);
static void encoder_task(void *pvParameters);
static void handle_encoder_event(encoder_event_t *event);
static void update_card_selection(void);
// LEGACY УДАЛЕНО
// УДАЛЕНО: static void encoder_event_cb(lv_event_t *e);
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
// LEGACY УДАЛЕНО
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

/**
 * @brief Инициализация стилей с улучшенной цветовой схемой для гидропоники
 * Использует принципы Material Design с адаптацией под гидропонную тематику
 * Все элементы рассчитаны для дисплея 240x320 с правильными отступами
 */
void init_styles(void)  // Глобальная функция - объявлена в lvgl_styles.h
{
    if (styles_initialized) {
        return;
    }

    // =============================================
    // СТИЛИ ОСНОВНЫХ ЭЛЕМЕНТОВ
    // =============================================

    // Стиль фона главного экрана - темный для комфорта глаз
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, COLOR_BG);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_pad_all(&style_bg, 0);

    // Стиль заголовка - темный с акцентом
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_header, 12);
    lv_style_set_pad_hor(&style_header, 16);
    lv_style_set_radius(&style_header, 0);

    // Стиль основного заголовка - крупный и четкий
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, COLOR_TEXT);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_title, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_title, 8);

    // =============================================
    // СТИЛИ КАРТОЧЕК ДАТЧИКОВ
    // =============================================

    // Стиль карточки датчика - современный дизайн с правильными отступами
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, COLOR_ACCENT_SOFT);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_opa(&style_card, LV_OPA_30);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 16);              // Основной отступ 16px
    lv_style_set_pad_row(&style_card, 12);              // Отступ между строками 12px
    lv_style_set_pad_column(&style_card, 8);            // Отступ между колонками 8px
    lv_style_set_shadow_color(&style_card, COLOR_SHADOW);
    lv_style_set_shadow_width(&style_card, 4);
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);
    lv_style_set_shadow_ofs_x(&style_card, 2);
    lv_style_set_shadow_ofs_y(&style_card, 2);

    // Стиль активной карточки - выделение бирюзовым
    lv_style_init(&style_card_focused);
    lv_style_set_bg_color(&style_card_focused, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_card_focused, LV_OPA_20);
    lv_style_set_border_color(&style_card_focused, COLOR_ACCENT);
    lv_style_set_border_width(&style_card_focused, 2);

    // Стиль больших значений датчиков - яркий и читаемый
    lv_style_init(&style_value_large);
    lv_style_set_text_color(&style_value_large, COLOR_ACCENT_SOFT);
    lv_style_set_text_font(&style_value_large, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_value_large, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_value_large, 4);

    // Стиль единиц измерения - компактный
    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_14); // Используем доступный шрифт
    lv_style_set_text_opa(&style_unit, LV_OPA_COVER);

    // Стиль названий датчиков - читаемый шрифт
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, COLOR_TEXT);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_14); // Используется из LVGL
    lv_style_set_text_opa(&style_label, LV_OPA_COVER);

    // Стиль статусных индикаторов
    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_status_bar, 8);
    lv_style_set_pad_hor(&style_status_bar, 16);

    // =============================================
    // СТИЛИ КНОПОК И ЭЛЕМЕНТОВ УПРАВЛЕНИЯ
    // =============================================

    // Основной стиль кнопок - бирюзовый акцент
    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_border_width(&style_button, 0);
    lv_style_set_radius(&style_button, 8);
    lv_style_set_pad_ver(&style_button, 12);
    lv_style_set_pad_hor(&style_button, 16);
    lv_style_set_shadow_color(&style_button, COLOR_ACCENT);
    lv_style_set_shadow_width(&style_button, 2);
    lv_style_set_shadow_opa(&style_button, LV_OPA_30);
    lv_style_set_shadow_ofs_y(&style_button, 2);

    // Стиль нажатой кнопки
    lv_style_init(&style_button_pressed);
    lv_style_set_bg_color(&style_button_pressed, COLOR_ACCENT_SOFT);
    lv_style_set_bg_opa(&style_button_pressed, LV_OPA_COVER);

    // Стиль кнопки назад - вторичный стиль
    lv_style_init(&style_button_secondary);
    lv_style_set_bg_color(&style_button_secondary, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_button_secondary, LV_OPA_COVER);
    lv_style_set_border_color(&style_button_secondary, COLOR_TEXT_MUTED);
    lv_style_set_border_width(&style_button_secondary, 1);
    lv_style_set_border_opa(&style_button_secondary, LV_OPA_50);

    // =============================================
    // СТИЛИ СТАТУСНЫХ ИНДИКАТОРОВ
    // =============================================

    // Нормальное состояние - зеленый
    lv_style_init(&style_status_normal);
    lv_style_set_bg_color(&style_status_normal, COLOR_NORMAL);
    lv_style_set_bg_opa(&style_status_normal, LV_OPA_COVER);
    lv_style_set_radius(&style_status_normal, 4);
    lv_style_set_size(&style_status_normal, 8, 8);

    // Предупреждение - оранжевый
    lv_style_init(&style_status_warning);
    lv_style_set_bg_color(&style_status_warning, COLOR_WARNING);
    lv_style_set_bg_opa(&style_status_warning, LV_OPA_COVER);
    lv_style_set_radius(&style_status_warning, 4);
    lv_style_set_size(&style_status_warning, 8, 8);

    // Опасность - красный
    lv_style_init(&style_status_danger);
    lv_style_set_bg_color(&style_status_danger, COLOR_DANGER);
    lv_style_set_bg_opa(&style_status_danger, LV_OPA_COVER);
    lv_style_set_radius(&style_status_danger, 4);
    lv_style_set_size(&style_status_danger, 8, 8);

    // =============================================
    // СТИЛИ ЭКРАНОВ ДЕТАЛИЗАЦИИ
    // =============================================

    // Фон экрана детализации
    lv_style_init(&style_detail_bg);
    lv_style_set_bg_color(&style_detail_bg, COLOR_BG);
    lv_style_set_bg_opa(&style_detail_bg, LV_OPA_COVER);
    lv_style_set_pad_all(&style_detail_bg, 16);

    // Контейнер для контента детализации
    lv_style_init(&style_detail_container);
    lv_style_set_bg_color(&style_detail_container, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_detail_container, LV_OPA_COVER);
    lv_style_set_border_color(&style_detail_container, COLOR_ACCENT);
    lv_style_set_border_width(&style_detail_container, 1);
    lv_style_set_border_opa(&style_detail_container, LV_OPA_30);
    lv_style_set_radius(&style_detail_container, 12);
    lv_style_set_pad_all(&style_detail_container, 16);

    // Стиль заголовка детализации
    lv_style_init(&style_detail_title);
    lv_style_set_text_color(&style_detail_title, COLOR_TEXT);
    lv_style_set_text_font(&style_detail_title, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_detail_title, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_title, 8);

    // Стиль значения в детализации - очень крупный
    lv_style_init(&style_detail_value);
    lv_style_set_text_color(&style_detail_value, COLOR_ACCENT_SOFT);
    lv_style_set_text_font(&style_detail_value, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_detail_value, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_value, 8);

    // Стиль очень крупного значения в детализации (алиас для style_detail_value)
    lv_style_init(&style_detail_value_big);
    lv_style_set_text_color(&style_detail_value_big, COLOR_ACCENT_SOFT);
    lv_style_set_text_font(&style_detail_value_big, &lv_font_montserrat_14);  // Используем доступный шрифт (14)
    lv_style_set_text_opa(&style_detail_value_big, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_value_big, 12);

    // Стиль дополнительной информации
    lv_style_init(&style_detail_info);
    lv_style_set_text_color(&style_detail_info, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_detail_info, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_detail_info, LV_OPA_COVER);

    // =============================================
    // СТИЛЬ ФОКУСА - РАМКА ВОКРУГ ЭЛЕМЕНТА
    // =============================================
    lv_style_init(&style_focus);
    lv_style_set_border_color(&style_focus, COLOR_ACCENT);       // Бирюзовая рамка
    lv_style_set_border_width(&style_focus, 3);                  // Толщина рамки 3px
    lv_style_set_border_opa(&style_focus, LV_OPA_COVER);         // Полная непрозрачность
    lv_style_set_outline_color(&style_focus, COLOR_ACCENT);      // Внешняя обводка
    lv_style_set_outline_width(&style_focus, 2);                 // Толщина обводки 2px
    lv_style_set_outline_pad(&style_focus, 2);                   // Отступ обводки 2px
    lv_style_set_outline_opa(&style_focus, LV_OPA_50);           // Полупрозрачная обводка

    styles_initialized = true;
    ESP_LOGI(TAG, "Стили интерфейса инициализированы с улучшенной цветовой схемой для дисплея 240x320");
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
    lv_obj_set_style_pad_all(status_settings_btn, 4, 0);
    lv_obj_set_size(status_settings_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    
    // Стиль при фокусе - добавляем рамку
    lv_obj_set_style_border_color(status_settings_btn, COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(status_settings_btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(status_settings_btn, LV_OPA_COVER, LV_STATE_FOCUSED);

    lv_obj_t *icon = lv_label_create(status_settings_btn);
    lv_obj_add_style(icon, &style_label, 0);
    lv_label_set_text(icon, "SET");
    
    // Добавляем обработчик события для кнопки SET
    lv_obj_add_event_cb(status_settings_btn, system_settings_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(status_settings_btn, LV_OBJ_FLAG_CLICKABLE);
    
    // Делаем кнопку видимой и фокусируемой
    lv_obj_clear_flag(status_settings_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_settings_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

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

/**
 * @brief Создание улучшенной карточки датчика с правильными размерами для дисплея 240x320
 * Карточка адаптирована под гидропонную тематику с бирюзовыми акцентами
 */
static lv_obj_t *create_sensor_card(lv_obj_t *parent, int index)
{
    const sensor_meta_t *meta = &SENSOR_META[index];

    // =============================================
    // СОЗДАНИЕ ОСНОВНОЙ КАРТОЧКИ
    // =============================================

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_card, 0);

    // Фиксированные размеры для дисплея 240x320 (2 колонки с отступами)
    lv_coord_t card_width = (240 - 16 - 8 - 8) / 2;  // (экран - левый отступ - правый отступ - промежуток) / 2
    lv_coord_t card_height = 85;                      // Оптимальная высота для контента

    lv_obj_set_size(card, card_width, card_height);
    lv_obj_set_style_min_width(card, card_width, 0);
    lv_obj_set_style_max_width(card, card_width, 0);

    // Flexbox компоновка для вертикального размещения элементов
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // =============================================
    // СОЗДАНИЕ ЭЛЕМЕНТОВ КАРТОЧКИ
    // =============================================

    // Название датчика - верхняя часть карточки
    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_add_style(title_label, &style_label, 0);
    lv_label_set_text(title_label, meta->title);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);

    // Основное значение датчика - крупное и яркое
    lv_obj_t *value = lv_label_create(card);
    lv_obj_add_style(value, &style_value_large, 0);
    lv_label_set_text(value, "--");
    value_labels[index] = value;
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);

    // Единица измерения - компактная
    lv_obj_t *unit = lv_label_create(card);
    lv_obj_add_style(unit, &style_unit, 0);
    lv_label_set_text(unit, meta->unit);
    lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_RIGHT, 0);

    // Статусный индикатор - цветовой индикатор состояния
    lv_obj_t *status_dot = lv_obj_create(card);
    lv_obj_remove_style_all(status_dot);
    lv_obj_add_style(status_dot, &style_status_normal, 0);
    lv_obj_set_size(status_dot, 8, 8);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);

    // Статусный текст - описание состояния
    lv_obj_t *badge = lv_label_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_add_style(badge, &style_detail_info, 0);
    lv_label_set_text(badge, "Норма");
    lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);
    status_labels[index] = badge;

    // =============================================
    // НАСТРОЙКА ОБРАБОТЧИКОВ СОБЫТИЙ
    // =============================================

    // Добавляем обработчик событий для перехода к детализации
    lv_obj_add_event_cb(card, sensor_card_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    // Сохраняем ссылку на карточку для навигации энкодером
    sensor_cards[index] = card;

    // =============================================
    // НАСТРОЙКА ФОКУСА И НАВИГАЦИИ
    // =============================================

    // Добавляем карточку в группу энкодера для навигации
    if (encoder_group) {
        lv_group_add_obj(encoder_group, card);
    }

    // Сохраняем ссылки на элементы для обновления
    sensor_containers[index] = card;

    return card;
}

/* =============================
 *  CORE UI BUILDERS
 * ============================= */
/**
 * @brief Создание улучшенного главного экрана с правильной компоновкой для дисплея 240x320
 * Карточки расположены в 2 колонки с правильными отступами и размерами
 */
static void __attribute__((unused)) create_main_ui(void)
{
    init_styles();

    // Если главный экран уже создан, просто возвращаемся
    if (main_screen != NULL) {
        ESP_LOGI(TAG, "Main screen already created, skipping recreation");
        return;
    }

    main_screen = lv_scr_act();
    screen_main = main_screen; // Для совместимости

    // =============================================
    // НАСТРОЙКА ОСНОВНОГО ЭКРАНА
    // =============================================

    lv_obj_remove_style_all(main_screen);
    lv_obj_add_style(main_screen, &style_bg, 0);

    // Устанавливаем отступы для всего экрана (адаптировано для 240x320)
    lv_obj_set_style_pad_top(main_screen, 4, 0);
    lv_obj_set_style_pad_bottom(main_screen, 8, 0);
    lv_obj_set_style_pad_left(main_screen, 8, 0);
    lv_obj_set_style_pad_right(main_screen, 8, 0);

    // Основная компоновка - вертикальная
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(main_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_screen,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    // =============================================
    // СОЗДАНИЕ СТАТУС-БАРА
    // =============================================

    create_status_bar(main_screen, "🌱 Hydroponics Monitor v3.0");

    // =============================================
    // СОЗДАНИЕ КОНТЕЙНЕРА ДЛЯ КАРТОЧЕК ДАТЧИКОВ
    // =============================================

    lv_obj_t *content = lv_obj_create(main_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, 240 - 16);  // Ширина 240px минус отступы
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    // Правильные отступы между карточками для дисплея 240x320
    lv_obj_set_style_pad_row(content, 12, 0);    // Вертикальный отступ между рядами 12px
    lv_obj_set_style_pad_column(content, 8, 0);  // Горизонтальный отступ между колонками 8px
    lv_obj_set_style_pad_all(content, 0, 0);     // Убираем внутренние отступы контейнера
    lv_obj_set_flex_grow(content, 1);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // =============================================
    // НАСТРОЙКА ЭНКОДЕРА И ФОКУСА
    // =============================================

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
    
    // Добавляем кнопку SET в группу энкодера (в конце, после всех карточек)
    if (encoder_group && status_settings_btn) {
        lv_group_add_obj(encoder_group, status_settings_btn);
        ESP_LOGI(TAG, "SET button added to encoder group");
        
        // Проверяем количество объектов в группе
        uint32_t obj_count = lv_group_get_obj_count(encoder_group);
        ESP_LOGI(TAG, "Total objects in encoder group: %lu (6 sensors + 1 SET button)", (unsigned long)obj_count);
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
    
    // Инициализируем таймер автоскрытия фокуса
    if (focus_hide_timer == NULL) {
        reset_focus_timer();
        ESP_LOGI(TAG, "Focus hide timer initialized");
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
    lv_obj_set_height(info_container, 80);  // Уменьшили с 100 до 80
    lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(info_container, 12, 0);  // Уменьшили отступы с 16 до 12

    lv_obj_t *range_label = lv_label_create(info_container);
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

    lv_obj_t *desc_label = lv_label_create(info_container);
    lv_obj_add_style(desc_label, &style_label, 0);
    lv_label_set_text(desc_label, meta->description ? meta->description : "");
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc_label, LV_PCT(100));

    // Добавляем кнопку Settings
    lv_obj_t *settings_btn = lv_btn_create(body);
    lv_obj_set_width(settings_btn, LV_PCT(90));
    lv_obj_set_height(settings_btn, 35);  // Уменьшили с 40 до 35
    lv_obj_set_style_bg_color(settings_btn, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(settings_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(settings_btn, 8, 0);
    
    lv_obj_t *settings_label = lv_label_create(settings_btn);
    lv_obj_set_style_text_color(settings_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_14, 0);  // Используем доступный шрифт
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);
    
    // Добавляем обработчик нажатия на кнопку Settings
    lv_obj_add_event_cb(settings_btn, settings_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
    lv_obj_add_flag(settings_btn, LV_OBJ_FLAG_CLICKABLE);

    // Компактная подсказка
    lv_obj_t *hint = lv_label_create(body);
    lv_obj_add_style(hint, &style_label, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);  // Используем доступный шрифт
    lv_label_set_text(hint, "Press: back | Long: home");

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
    
    // Убираем фокус с предыдущего элемента (карточка или кнопка SET)
    if (current_focus_index >= 0 && current_focus_index < SENSOR_COUNT) {
        // Убираем визуальный стиль фокуса с карточки датчика
        if (sensor_containers[current_focus_index] && focus_visible) {
            lv_obj_remove_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
        }
    } else if (current_focus_index == SENSOR_COUNT) {
        // Убираем фокус с кнопки SET (индекс 6)
        if (status_settings_btn) {
            lv_obj_clear_state(status_settings_btn, LV_STATE_FOCUSED);
            // Также убираем визуальный стиль фокуса если есть
            lv_obj_remove_style(status_settings_btn, &style_focus, LV_PART_MAIN);
        }
    }

    // Обновляем индекс фокуса
    current_focus_index = index;
    selected_card_index = index;  // Синхронизируем индексы
    
    // Устанавливаем фокус на новом элементе (только если фокус видим)
    if (sensor_containers[index]) {
        if (focus_visible) {
            lv_obj_add_style(sensor_containers[index], &style_focus, LV_PART_MAIN);
        }
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
    return SENSOR_COUNT + 1;  // 6 датчиков + 1 кнопка SET
}

void lvgl_clear_focus_group(void)
{
    if (encoder_group) {
        lv_group_remove_all_objs(encoder_group);
    }
    current_focus_index = -1;
}

/* =============================
 *  FOCUS VISIBILITY CONTROL
 * ============================= */

/**
 * @brief Callback таймера для автоскрытия фокуса
 */
static void focus_hide_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    hide_focus();
    ESP_LOGI(TAG, "Focus hidden after inactivity timeout");
}

/**
 * @brief Показать фокус на текущем элементе
 */
static void show_focus(void)
{
    if (focus_visible) {
        return;  // Фокус уже видим
    }
    
    focus_visible = true;
    
    // Применяем стиль фокуса к текущему элементу
    if (current_focus_index >= 0 && current_focus_index < SENSOR_COUNT) {
        if (sensor_containers[current_focus_index]) {
            lv_obj_add_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
        }
    }
    
    ESP_LOGD(TAG, "Focus shown");
}

/**
 * @brief Скрыть фокус
 */
static void hide_focus(void)
{
    if (!focus_visible) {
        return;  // Фокус уже скрыт
    }
    
    focus_visible = false;
    
    // Убираем стиль фокуса с текущего элемента
    if (current_focus_index >= 0 && current_focus_index < SENSOR_COUNT) {
        if (sensor_containers[current_focus_index]) {
            lv_obj_remove_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
        }
    }
    
    ESP_LOGD(TAG, "Focus hidden");
}

/**
 * @brief Сбросить таймер автоскрытия фокуса
 * Вызывается при любой активности энкодера
 */
static void reset_focus_timer(void)
{
    // Показываем фокус если он был скрыт
    if (!focus_visible) {
        show_focus();
    }
    
    // Перезапускаем таймер
    if (focus_hide_timer == NULL) {
        focus_hide_timer = lv_timer_create(focus_hide_timer_cb, FOCUS_HIDE_TIMEOUT_MS, NULL);
        ESP_LOGI(TAG, "Focus hide timer created (%d ms)", FOCUS_HIDE_TIMEOUT_MS);
    } else {
        lv_timer_reset(focus_hide_timer);
        ESP_LOGD(TAG, "Focus hide timer reset");
    }
}

static void set_encoder_group(lv_group_t *group)
{
    lv_indev_t *encoder_indev = lcd_ili9341_get_encoder_indev();
    if (encoder_indev && group) {
        lv_indev_set_group(encoder_indev, group);
        // Устанавливаем фокус на первый объект в группе
        if (lv_group_get_obj_count(group) > 0) {
            lv_group_focus_next(group);
        }
    }
}

// Ленивая инициализация экранов датчиков убрана - используем detail_screens[] и settings_screens[]

static void switch_to_screen(lv_obj_t *screen, screen_type_t screen_type, lv_group_t *group)
{
    if (screen) {
        // Если уходим с экрана детализации, очищаем указатели на старые графики
        if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
            if (screen_type < SCREEN_DETAIL_PH || screen_type > SCREEN_DETAIL_CO2) {
                detail_chart = NULL;
                detail_series = NULL;
                detail_current_index = -1;
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
    
    sensor_data_t sensor_data;
    while (1) {
        // Обрабатываем все данные из очереди за один цикл
        bool data_processed = false;
        while (xQueueReceive(sensor_data_queue, &sensor_data, 0) == pdTRUE) {
            data_processed = true;
            // Берем только последнее значение из очереди, игнорируя промежуточные
        }
        
        if (data_processed) {
            if (!lvgl_lock(100)) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            if (lv_is_initialized()) {
                update_sensor_display(&sensor_data);
            }
            lvgl_unlock();
        }
        
        // Обновляем дисплей каждые 200мс
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* =============================
 *  CALLBACK ДЛЯ pH ЭКРАНОВ
 * ============================= */

// LEGACY: Callback удалён - используется screen_go_back() из Screen Manager

/* =============================
 *  PUBLIC API
 * ============================= */
void lvgl_main_init(void)
{
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   Initializing UI with Screen Manager System     ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════╝");
    
    // Инициализация старых компонентов (для совместимости)
    // Инициализируем экраны pH (пока оставляем)
    // LEGACY REMOVED: pH screens migrated to Screen Manager
    // ph_screen_init();
    // ph_set_close_callback(ph_return_to_main);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // ===== НОВАЯ СИСТЕМА: Screen Manager =====
    ESP_LOGI(TAG, "Initializing Screen Manager System...");
    if (lvgl_lock(1000)) {
        // Инициализация стилей (требуется для виджетов)
        init_styles();
        
        // Инициализируем Screen Manager и регистрируем все экраны
        esp_err_t ret = screen_system_init_all();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Screen Manager: %s", esp_err_to_name(ret));
            lvgl_unlock();
            return;
        }
        
        lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for UI initialization");
        return;
    }
    
    // Создаем задачу обработки энкодера
    TaskHandle_t encoder_task_handle = NULL;
    BaseType_t task_created = xTaskCreate(encoder_task, "lvgl_encoder", 4096, NULL, 5, &encoder_task_handle);
    if (task_created == pdPASS && encoder_task_handle != NULL) {
        ESP_LOGI(TAG, "Encoder task created successfully");
    } else {
        ESP_LOGE(TAG, "FAILED to create encoder task!");
    }
    
    ESP_LOGI(TAG, "UI initialization complete with Screen Manager");
}

void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2)
{
    if (sensor_data_queue == NULL) {
        return;
    }
    
    sensor_data_t sensor_data = {
        .ph = ph,
        .ec = ec,
        .temperature = temp,
        .humidity = hum,
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

/* =============================
 *  UI NAVIGATION FUNCTIONS
 * ============================= */

// Обработчик события клика по карточке сенсора (ОБНОВЛЕНО для Screen Manager)
static void sensor_card_event_cb(lv_event_t *e)
{
    uint8_t sensor_index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    
    // Screen Manager: прямые вызовы screen_show()
    const char *detail_screens_ids[] = {
        "detail_ph", "detail_ec", "detail_temp",
        "detail_humidity", "detail_lux", "detail_co2"
    };
    
    if (sensor_index < SENSOR_COUNT) {
        ESP_LOGI(TAG, "Opening detail screen for sensor %d via Screen Manager", sensor_index);
        screen_show(detail_screens_ids[sensor_index], NULL);
    }
}

// Создание экрана детализации для сенсора
static void __attribute__((unused)) create_detail_screen(uint8_t sensor_index)
{
    // Для pH используем специальный детализированный экран
    // LEGACY: pH detail screen migrated to Screen Manager
    // sensor_index 0 (pH) handled by Screen Manager automatically
    if (sensor_index == 0) {
        ESP_LOGW(TAG, "LEGACY call to pH detail - ignoring");
        return;
    }
    
    const sensor_meta_t *meta = &SENSOR_META[sensor_index];
    detail_screen_t *detail = &detail_screens[sensor_index];
    lv_group_t *detail_group = detail_screen_groups[sensor_index];
    if (detail_group == NULL) {
        detail_group = lv_group_create();
        lv_group_set_wrap(detail_group, true);
        detail_screen_groups[sensor_index] = detail_group;
    } else {
        lv_group_remove_all_objs(detail_group);
    }
    
    // Создаем экран
    detail->screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(detail->screen);
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
    lv_group_add_obj(detail_group, detail->back_btn);
    
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
    lv_group_add_obj(detail_group, detail->settings_btn);
    
    lv_obj_t *settings_label = lv_label_create(detail->settings_btn);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_center(settings_label);
}

// Создание экрана настроек для сенсора
static void __attribute__((unused)) create_settings_screen(uint8_t sensor_index)
{
    const sensor_meta_t *meta = &SENSOR_META[sensor_index];
    settings_screen_t *settings = &settings_screens[sensor_index];
    
    // Создаем экран
    settings->screen = lv_obj_create(NULL);
    settings->sensor_index = sensor_index;
    lv_obj_clean(settings->screen);
    lv_obj_add_style(settings->screen, &style_bg, 0);
    lv_obj_set_style_pad_all(settings->screen, 16, 0);
    
    // Получаем или создаем группу для этого экрана настроек
    lv_group_t *settings_group = settings_screen_groups[sensor_index];
    if (settings_group == NULL) {
        settings_group = lv_group_create();
        lv_group_set_wrap(settings_group, true);
        settings_screen_groups[sensor_index] = settings_group;
    } else {
        lv_group_remove_all_objs(settings_group);
    }
    
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
    // Добавляем кнопку назад в группу энкодера
    lv_group_add_obj(settings_group, settings->back_btn);
    
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
        // Добавляем каждую кнопку в группу энкодера
        lv_group_add_obj(settings_group, item);
        
        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_center(item_label);
        
        // Добавляем индикатор "заглушка"
        lv_obj_t *placeholder = lv_label_create(item);
        lv_obj_add_style(placeholder, &style_unit, 0);
        lv_label_set_text(placeholder, "→");
        lv_obj_align(placeholder, LV_ALIGN_RIGHT_MID, -10, 0);
    }
    
    ESP_LOGI(TAG, "Settings screen created with %d objects in encoder group", lv_group_get_obj_count(settings_group));
}

// Переключение экранов
static void __attribute__((unused)) show_screen(screen_type_t screen)
{
    if (current_screen >= SCREEN_DETAIL_PH && current_screen <= SCREEN_DETAIL_CO2) {
        if (screen < SCREEN_DETAIL_PH || screen > SCREEN_DETAIL_CO2) {
            detail_chart = NULL;
            detail_series = NULL;
            detail_current_index = -1;
        }
    }
 
    current_screen = screen;
    lv_group_t *target_group = NULL;
    lv_obj_t *target_screen_obj = NULL;

    switch (screen) {
        case SCREEN_MAIN:
            target_screen_obj = main_screen;
            target_group = encoder_group;
            break;
        case SCREEN_DETAIL_PH:
            // LEGACY: Use Screen Manager instead
            ESP_LOGW(TAG, "LEGACY pH detail - use screen_show(\"detail_ph\", NULL)");
            target_group = NULL;
            target_screen_obj = NULL;
            break;
        case SCREEN_DETAIL_EC:
        case SCREEN_DETAIL_TEMP:
        case SCREEN_DETAIL_HUMIDITY:
        case SCREEN_DETAIL_LUX:
        case SCREEN_DETAIL_CO2: {
            uint8_t sensor_index = screen - SCREEN_DETAIL_PH;
            detail_screen_t *detail = &detail_screens[sensor_index];
            if (detail->screen == NULL) {
                create_detail_screen(sensor_index);
            }
            target_screen_obj = detail->screen;
            target_group = detail_screen_groups[sensor_index];
            break;
        }
        case SCREEN_SETTINGS_PH:
            // LEGACY: Use Screen Manager instead
            ESP_LOGW(TAG, "LEGACY pH settings - use screen_show(\"settings_ph\", NULL)");
            target_group = NULL;
            target_screen_obj = NULL;
            break;
        case SCREEN_SETTINGS_EC:
        case SCREEN_SETTINGS_TEMP:
        case SCREEN_SETTINGS_HUMIDITY:
        case SCREEN_SETTINGS_LUX:
        case SCREEN_SETTINGS_CO2: {
            uint8_t sensor_index = screen - SCREEN_SETTINGS_PH;
            settings_screen_t *settings = &settings_screens[sensor_index];
            if (settings->screen == NULL) {
                create_settings_screen(sensor_index);
            }
            target_screen_obj = settings->screen;
            target_group = settings_screen_groups[sensor_index];
            break;
        }
        case SCREEN_CALIBRATION:
            // LEGACY: Use Screen Manager instead
            ESP_LOGW(TAG, "LEGACY pH calibration - use Screen Manager");
            target_group = NULL;
            target_screen_obj = NULL;
            break;
        case SCREEN_SYSTEM_STATUS:
        case SCREEN_AUTO_CONTROL:
        case SCREEN_WIFI_SETTINGS:
        case SCREEN_DISPLAY_SETTINGS:
        case SCREEN_DATA_LOGGER_SETTINGS:
        case SCREEN_SYSTEM_INFO:
        case SCREEN_RESET_CONFIRM:
            // LEGACY: Эти экраны мигрированы на Screen Manager
            ESP_LOGW(TAG, "LEGACY system screen access! Use screen_show() instead.");
            target_screen_obj = NULL;
            target_group = NULL;
            break;
        default:
            break;
    }

    if (target_screen_obj) {
        switch_to_screen(target_screen_obj, screen, target_group);
    }

    if (screen == SCREEN_MAIN) {
        update_card_selection();
    }
    // update_settings_selection() больше не используется - навигация управляется группой LVGL
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
        
        case SCREEN_SYSTEM_STATUS:
            // С главного меню настроек возвращаемся на главный экран
            show_screen(SCREEN_MAIN);
            break;
        
        case SCREEN_AUTO_CONTROL:
        case SCREEN_WIFI_SETTINGS:
        case SCREEN_DISPLAY_SETTINGS:
        case SCREEN_DATA_LOGGER_SETTINGS:
        case SCREEN_SYSTEM_INFO:
        case SCREEN_RESET_CONFIRM:
            // Из подменю настроек возвращаемся в меню системных настроек
            show_screen(SCREEN_SYSTEM_STATUS);
            break;
        
        case SCREEN_NETWORK_SETTINGS:
        case SCREEN_MOBILE_CONNECT:
        case SCREEN_OTA_UPDATE:
        case SCREEN_DATA_EXPORT:
        case SCREEN_ABOUT:
            // Возвращаемся на главный экран
            show_screen(SCREEN_MAIN);
            break;
        
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

// Обработчик кнопки "SET" в строке состояния (ОБНОВЛЕНО для Screen Manager)
static void system_settings_button_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    ESP_LOGI(TAG, "System settings button clicked - opening via Screen Manager");
    
    // Screen Manager: прямой вызов
    screen_show("system_menu", NULL);
}

// Обработчик пунктов меню системных настроек (ОБНОВЛЕНО для Screen Manager)
static void system_menu_item_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем только клик или нажатие Enter
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key != LV_KEY_ENTER) {
            return;  // Игнорируем другие клавиши
        }
    } else if (code != LV_EVENT_CLICKED) {
        return;  // Игнорируем другие события
    }
    
    uint8_t item_index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "System menu item %d activated (code=%d) - Screen Manager", item_index, code);
    
    // Screen Manager: прямые вызовы экранов
    const char *system_screens[] = {
        "auto_control",      // 0
        "wifi_settings",     // 1
        "display_settings",  // 2
        "data_logger",       // 3
        "system_info",       // 4
        "reset_confirm"      // 5
    };
    
    if (item_index < 6) {
        screen_show(system_screens[item_index], NULL);
    }
}

/* =============================
 *  ENCODER NAVIGATION FUNCTIONS
 * ============================= */

// Задача обработки энкодера
static void encoder_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);
    
    // Ждем, пока энкодер не будет инициализирован и очередь не станет доступной
    QueueHandle_t encoder_queue = NULL;
    ESP_LOGI(TAG, "Encoder task started, waiting for encoder initialization...");
    
    while (encoder_queue == NULL) {
        encoder_queue = encoder_get_event_queue();
        if (encoder_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Ждем 100мс
        }
    }
    
    ESP_LOGI(TAG, "Encoder queue ready, starting event processing...");
    
    encoder_event_t event;
    while (1) {
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!lvgl_lock(100)) {
                ESP_LOGW(TAG, "Failed to acquire LVGL lock for encoder event");
                continue;
            }
            
            if (lv_is_initialized()) {
                handle_encoder_event(&event);
            }
            lvgl_unlock();
        }
    }
}

// Обработка событий энкодера (УПРОЩЕННАЯ для Screen Manager)
static void handle_encoder_event(encoder_event_t *event)
{
    if (!encoder_navigation_enabled) {
        return;
    }
    
    // Сбрасываем таймер скрытия фокуса
    reset_focus_timer();
    
    // ===== НОВАЯ СИСТЕМА: Делегируем Screen Manager =====
    screen_instance_t *current = screen_get_current();
    if (current && current->encoder_group) {
        // Навигация управляется группой LVGL автоматически
        switch (event->type) {
            case ENCODER_EVENT_ROTATE_CW:
                lv_group_focus_next(current->encoder_group);
                ESP_LOGD(TAG, "Screen Manager: focus next");
                return;  // Обработано новой системой
                
            case ENCODER_EVENT_ROTATE_CCW:
                lv_group_focus_prev(current->encoder_group);
                ESP_LOGD(TAG, "Screen Manager: focus prev");
                return;  // Обработано новой системой
                
            case ENCODER_EVENT_BUTTON_PRESS:
                // Отправляем ENTER в группу
                lv_group_send_data(current->encoder_group, LV_KEY_ENTER);
                // Также отправляем CLICKED напрямую
                lv_obj_t *focused = lv_group_get_focused(current->encoder_group);
                if (focused) {
                    lv_obj_send_event(focused, LV_EVENT_CLICKED, NULL);
                }
                ESP_LOGD(TAG, "Screen Manager: button pressed");
                return;  // Обработано новой системой
                
            default:
                break;
        }
    }
    
    // ===== LEGACY: Минимальная обработка для pH screen (если активен) =====
    // Screen Manager не активен, используем fallback
    ESP_LOGD(TAG, "Legacy encoder handling (Screen Manager not active)");
    
    switch (event->type) {
        case ENCODER_EVENT_ROTATE_CW:
            lcd_ili9341_set_encoder_diff(1);
            break;
            
        case ENCODER_EVENT_ROTATE_CCW:
            lcd_ili9341_set_encoder_diff(-1);
            break;
            
        case ENCODER_EVENT_BUTTON_PRESS:
            // Fallback для legacy экранов
            {
                lv_indev_t *indev = lcd_ili9341_get_encoder_indev();
                if (indev) {
                    lv_group_t *group = lv_indev_get_group(indev);
                    if (group) {
                        lv_group_send_data(group, LV_KEY_ENTER);
                    }
                }
            }
            break;
            
        case ENCODER_EVENT_BUTTON_LONG_PRESS:
            ESP_LOGI(TAG, "Encoder button long press detected (disabled)");
            // Заглушка - длинное нажатие пока отключено
            // TODO: можно использовать для быстрого возврата на главный экран
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
    if (current_screen != SCREEN_MAIN) {
        return;
    }
    
    // Для карточек датчиков (индексы 0-5) используем систему фокуса
    if (selected_card_index < SENSOR_COUNT) {
        lvgl_set_focus(selected_card_index);
    } else {
        // Для кнопки SET (индекс 6) устанавливаем фокус напрямую
        if (encoder_group && status_settings_btn) {
            lv_group_focus_obj(status_settings_btn);
            current_focus_index = SENSOR_COUNT;
        }
    }
}

// Обновление выделения пунктов настроек
static void __attribute__((unused)) update_settings_selection(void)
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
}

// LEGACY ФУНКЦИЯ УДАЛЕНА: encoder_event_cb() - используется handle_encoder_event()

// =============================================
// ЭКРАН СИСТЕМНЫХ НАСТРОЕК
// =============================================

/**
 * @brief Создание экрана общих настроек системы
 */
static void __attribute__((unused)) create_system_settings_screen(void)
{
    if (system_settings_screen != NULL) {
        ESP_LOGI(TAG, "System settings screen already exists");
        return;
    }

    system_settings_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(system_settings_screen);
    lv_obj_add_style(system_settings_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(system_settings_screen, 16, 0);

    // Статус-бар
    create_status_bar(system_settings_screen, "System Settings");

    // Основной контент
    lv_obj_t *content = lv_obj_create(system_settings_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // Создаем группу для навигации энкодером
    if (system_settings_group == NULL) {
        system_settings_group = lv_group_create();
        lv_group_set_wrap(system_settings_group, true);
    } else {
        lv_group_remove_all_objs(system_settings_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "General Settings");
    lv_obj_set_width(title, LV_PCT(100));

    // Пункты меню настроек
    const char* settings_items[] = {
        "Auto Control",
        "WiFi Settings",
        "Display Settings",
        "Data Logger",
        "System Info",
        "Reset to Defaults"
    };
    
    for (int i = 0; i < 6; i++) {
        lv_obj_t *item = lv_btn_create(content);
        lv_obj_add_style(item, &style_card, 0);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_height(item, 40);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        
        // Стиль фокуса - бирюзовая рамка
        lv_obj_set_style_border_color(item, COLOR_ACCENT, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(item, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(item, LV_OPA_COVER, LV_STATE_FOCUSED);
        
        lv_obj_t *item_label = lv_label_create(item);
        lv_obj_add_style(item_label, &style_label, 0);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_align(item_label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_t *arrow = lv_label_create(item);
        lv_obj_add_style(arrow, &style_unit, 0);
        lv_label_set_text(arrow, ">");
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);
        
        // Добавляем обработчики события с индексом пункта меню
        lv_obj_add_event_cb(item, system_menu_item_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(item, system_menu_item_event_cb, LV_EVENT_KEY, (void*)(intptr_t)i);
        
        // Добавляем в группу для навигации
        lv_group_add_obj(system_settings_group, item);
    }

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(system_settings_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "System settings screen created");
}

// =============================================
// ЭКРАН AUTO CONTROL
// =============================================

static void auto_control_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    // Загружаем конфигурацию
    system_config_t config;
    if (config_load(&config) == ESP_OK) {
        config.auto_control_enabled = enabled;
        config_save(&config);
        ESP_LOGI(TAG, "Auto control %s", enabled ? "enabled" : "disabled");
    }
}

static void __attribute__((unused)) create_auto_control_screen(void)
{
    if (auto_control_screen != NULL) {
        ESP_LOGI(TAG, "Auto control screen already exists");
        return;
    }

    auto_control_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(auto_control_screen);
    lv_obj_add_style(auto_control_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(auto_control_screen, 16, 0);

    // Статус-бар
    create_status_bar(auto_control_screen, "Auto Control");

    // Основной контент
    lv_obj_t *content = lv_obj_create(auto_control_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);

    // Создаем группу для навигации
    if (auto_control_group == NULL) {
        auto_control_group = lv_group_create();
        lv_group_set_wrap(auto_control_group, true);
    } else {
        lv_group_remove_all_objs(auto_control_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Automatic Control");
    lv_obj_set_width(title, LV_PCT(100));

    // Описание
    lv_obj_t *desc = lv_label_create(content);
    lv_obj_add_style(desc, &style_detail_info, 0);
    lv_label_set_text(desc, "Enable automatic pH and EC correction using pumps");
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, LV_PCT(100));

    // Главный переключатель Auto Control
    lv_obj_t *main_switch_cont = lv_obj_create(content);
    lv_obj_add_style(main_switch_cont, &style_card, 0);
    lv_obj_set_width(main_switch_cont, LV_PCT(100));
    lv_obj_set_height(main_switch_cont, 50);
    lv_obj_set_flex_flow(main_switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_switch_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(main_switch_cont, 12, 0);

    lv_obj_t *main_label = lv_label_create(main_switch_cont);
    lv_obj_add_style(main_label, &style_label, 0);
    lv_label_set_text(main_label, "Auto Control");

    lv_obj_t *main_switch = lv_switch_create(main_switch_cont);
    lv_obj_set_style_bg_color(main_switch, COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    
    // Загружаем текущее состояние из конфигурации
    system_config_t config;
    if (config_load(&config) == ESP_OK) {
        if (config.auto_control_enabled) {
            lv_obj_add_state(main_switch, LV_STATE_CHECKED);
        }
    }
    
    lv_obj_add_event_cb(main_switch, auto_control_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(auto_control_group, main_switch);

    // Информация о статусе
    lv_obj_t *status_cont = lv_obj_create(content);
    lv_obj_add_style(status_cont, &style_card, 0);
    lv_obj_set_width(status_cont, LV_PCT(100));
    lv_obj_set_height(status_cont, 80);
    lv_obj_set_style_pad_all(status_cont, 12, 0);

    lv_obj_t *status_label = lv_label_create(status_cont);
    lv_obj_add_style(status_label, &style_label, 0);
    lv_label_set_text(status_label, 
        "When enabled:\n"
        "- pH will be corrected automatically\n"
        "- EC will be adjusted as needed");
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_label, LV_PCT(100));

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(auto_control_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Auto control screen created");
}

// =============================================
// ЭКРАН WIFI SETTINGS
// =============================================

static void __attribute__((unused)) create_wifi_settings_screen(void)
{
    if (wifi_settings_screen != NULL) {
        ESP_LOGI(TAG, "WiFi settings screen already exists");
        return;
    }

    wifi_settings_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(wifi_settings_screen);
    lv_obj_add_style(wifi_settings_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(wifi_settings_screen, 16, 0);

    // Статус-бар
    create_status_bar(wifi_settings_screen, "WiFi Settings");

    // Основной контент
    lv_obj_t *content = lv_obj_create(wifi_settings_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // Создаем группу для навигации
    if (wifi_settings_group == NULL) {
        wifi_settings_group = lv_group_create();
        lv_group_set_wrap(wifi_settings_group, true);
    } else {
        lv_group_remove_all_objs(wifi_settings_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "WiFi Configuration");
    lv_obj_set_width(title, LV_PCT(100));

    // Информация о подключении
    lv_obj_t *info_cont = lv_obj_create(content);
    lv_obj_add_style(info_cont, &style_card, 0);
    lv_obj_set_width(info_cont, LV_PCT(100));
    lv_obj_set_height(info_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(info_cont, 12, 0);
    lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info_cont, 6, 0);

    lv_obj_t *mode_label = lv_label_create(info_cont);
    lv_obj_add_style(mode_label, &style_label, 0);
    lv_label_set_text(mode_label, "Mode: Access Point");
    
    lv_obj_t *ssid_label = lv_label_create(info_cont);
    lv_obj_add_style(ssid_label, &style_label, 0);
    lv_label_set_text(ssid_label, "SSID: HydroMonitor");
    
    lv_obj_t *ip_label = lv_label_create(info_cont);
    lv_obj_add_style(ip_label, &style_detail_info, 0);
    lv_label_set_text(ip_label, "IP: 192.168.4.1");

    // Кнопки управления
    lv_obj_t *btn_scan = lv_btn_create(content);
    lv_obj_add_style(btn_scan, &style_button, 0);
    lv_obj_set_width(btn_scan, LV_PCT(100));
    lv_obj_set_height(btn_scan, 40);
    lv_group_add_obj(wifi_settings_group, btn_scan);
    
    lv_obj_t *scan_label = lv_label_create(btn_scan);
    lv_label_set_text(scan_label, "Scan Networks");
    lv_obj_center(scan_label);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(wifi_settings_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "WiFi settings screen created");
}

// =============================================
// ЭКРАН DISPLAY SETTINGS
// =============================================

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    // Устанавливаем яркость дисплея
    lcd_ili9341_set_brightness((uint8_t)value);
    ESP_LOGI(TAG, "Display brightness set to %ld%%", value);
}

static void __attribute__((unused)) create_display_settings_screen(void)
{
    if (display_settings_screen != NULL) {
        ESP_LOGI(TAG, "Display settings screen already exists");
        return;
    }

    display_settings_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(display_settings_screen);
    lv_obj_add_style(display_settings_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(display_settings_screen, 16, 0);

    // Статус-бар
    create_status_bar(display_settings_screen, "Display Settings");

    // Основной контент
    lv_obj_t *content = lv_obj_create(display_settings_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);

    // Создаем группу для навигации
    if (display_settings_group == NULL) {
        display_settings_group = lv_group_create();
        lv_group_set_wrap(display_settings_group, true);
    } else {
        lv_group_remove_all_objs(display_settings_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Display Configuration");
    lv_obj_set_width(title, LV_PCT(100));

    // Контейнер яркости
    lv_obj_t *brightness_cont = lv_obj_create(content);
    lv_obj_add_style(brightness_cont, &style_card, 0);
    lv_obj_set_width(brightness_cont, LV_PCT(100));
    lv_obj_set_height(brightness_cont, 80);
    lv_obj_set_style_pad_all(brightness_cont, 12, 0);
    lv_obj_set_flex_flow(brightness_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(brightness_cont, 8, 0);

    lv_obj_t *brightness_label = lv_label_create(brightness_cont);
    lv_obj_add_style(brightness_label, &style_label, 0);
    lv_label_set_text(brightness_label, "Brightness");

    lv_obj_t *slider = lv_slider_create(brightness_cont);
    lv_obj_set_width(slider, LV_PCT(90));
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);  // Значение по умолчанию
    lv_obj_set_style_bg_color(slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(display_settings_group, slider);

    // Дополнительная информация
    lv_obj_t *info = lv_label_create(content);
    lv_obj_add_style(info, &style_detail_info, 0);
    lv_label_set_text(info, "Recommended: 60-80% for indoor use");
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info, LV_PCT(100));

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(display_settings_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Display settings screen created");
}

// =============================================
// ЭКРАН DATA LOGGER
// =============================================

static void data_logger_clear_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    if (data_logger_clear() == ESP_OK) {
        ESP_LOGI(TAG, "Data logger cleared successfully");
    }
}

static void data_logger_save_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    if (data_logger_save_to_nvs() == ESP_OK) {
        ESP_LOGI(TAG, "Data logger saved to NVS");
    }
}

static void __attribute__((unused)) create_data_logger_screen(void)
{
    if (data_logger_screen != NULL) {
        ESP_LOGI(TAG, "Data logger screen already exists");
        return;
    }

    data_logger_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(data_logger_screen);
    lv_obj_add_style(data_logger_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(data_logger_screen, 16, 0);

    // Статус-бар
    create_status_bar(data_logger_screen, "Data Logger");

    // Основной контент
    lv_obj_t *content = lv_obj_create(data_logger_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);

    // Создаем группу для навигации
    if (data_logger_group == NULL) {
        data_logger_group = lv_group_create();
        lv_group_set_wrap(data_logger_group, true);
    } else {
        lv_group_remove_all_objs(data_logger_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Data Logger");
    lv_obj_set_width(title, LV_PCT(100));

    // Статистика логов
    lv_obj_t *stats_cont = lv_obj_create(content);
    lv_obj_add_style(stats_cont, &style_card, 0);
    lv_obj_set_width(stats_cont, LV_PCT(100));
    lv_obj_set_height(stats_cont, 80);
    lv_obj_set_style_pad_all(stats_cont, 12, 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(stats_cont, 6, 0);

    lv_obj_t *count_label = lv_label_create(stats_cont);
    lv_obj_add_style(count_label, &style_label, 0);
    uint32_t log_count = data_logger_get_count();
    lv_label_set_text_fmt(count_label, "Total entries: %lu", (unsigned long)log_count);
    
    lv_obj_t *info_label = lv_label_create(stats_cont);
    lv_obj_add_style(info_label, &style_detail_info, 0);
    lv_label_set_text(info_label, "Logs are saved in NVS flash");

    // Кнопка сохранения
    lv_obj_t *save_btn = lv_btn_create(content);
    lv_obj_add_style(save_btn, &style_button, 0);
    lv_obj_set_width(save_btn, LV_PCT(100));
    lv_obj_set_height(save_btn, 40);
    lv_obj_add_event_cb(save_btn, data_logger_save_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(data_logger_group, save_btn);
    
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save to NVS");
    lv_obj_center(save_label);

    // Кнопка очистки
    lv_obj_t *clear_btn = lv_btn_create(content);
    lv_obj_add_style(clear_btn, &style_button, 0);
    lv_obj_set_style_bg_color(clear_btn, COLOR_DANGER, 0);
    lv_obj_set_width(clear_btn, LV_PCT(100));
    lv_obj_set_height(clear_btn, 40);
    lv_obj_add_event_cb(clear_btn, data_logger_clear_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(data_logger_group, clear_btn);
    
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear All Logs");
    lv_obj_center(clear_label);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(data_logger_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Data logger screen created");
}

// =============================================
// ЭКРАН SYSTEM INFO
// =============================================

static void __attribute__((unused)) create_system_info_screen(void)
{
    if (system_info_screen != NULL) {
        ESP_LOGI(TAG, "System info screen already exists");
        return;
    }

    system_info_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(system_info_screen);
    lv_obj_add_style(system_info_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(system_info_screen, 16, 0);

    // Статус-бар
    create_status_bar(system_info_screen, "System Info");

    // Основной контент
    lv_obj_t *content = lv_obj_create(system_info_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // Создаем группу для навигации
    if (system_info_group == NULL) {
        system_info_group = lv_group_create();
        lv_group_set_wrap(system_info_group, true);
    } else {
        lv_group_remove_all_objs(system_info_group);
    }

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "System Information");
    lv_obj_set_width(title, LV_PCT(100));

    // Информация о системе
    lv_obj_t *info_cont = lv_obj_create(content);
    lv_obj_add_style(info_cont, &style_card, 0);
    lv_obj_set_width(info_cont, LV_PCT(100));
    lv_obj_set_height(info_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(info_cont, 12, 0);
    lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info_cont, 6, 0);

    lv_obj_t *version_label = lv_label_create(info_cont);
    lv_obj_add_style(version_label, &style_label, 0);
    lv_label_set_text(version_label, "Version: v1.0");
    
    lv_obj_t *chip_label = lv_label_create(info_cont);
    lv_obj_add_style(chip_label, &style_label, 0);
    lv_label_set_text(chip_label, "Chip: ESP32-S3");
    
    lv_obj_t *cores_label = lv_label_create(info_cont);
    lv_obj_add_style(cores_label, &style_label, 0);
    lv_label_set_text(cores_label, "Cores: 2 (Dual Core)");

    // Память
    lv_obj_t *memory_cont = lv_obj_create(content);
    lv_obj_add_style(memory_cont, &style_card, 0);
    lv_obj_set_width(memory_cont, LV_PCT(100));
    lv_obj_set_height(memory_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(memory_cont, 12, 0);
    lv_obj_set_flex_flow(memory_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(memory_cont, 6, 0);

    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap = esp_get_minimum_free_heap_size();
    
    lv_obj_t *heap_label = lv_label_create(memory_cont);
    lv_obj_add_style(heap_label, &style_label, 0);
    lv_label_set_text_fmt(heap_label, "Free Heap: %lu KB", (unsigned long)(free_heap / 1024));
    
    lv_obj_t *min_heap_label = lv_label_create(memory_cont);
    lv_obj_add_style(min_heap_label, &style_detail_info, 0);
    lv_label_set_text_fmt(min_heap_label, "Min Free: %lu KB", (unsigned long)(min_heap / 1024));

    // Время работы
    uint64_t uptime = esp_timer_get_time() / 1000000ULL;  // секунды
    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    
    lv_obj_t *uptime_cont = lv_obj_create(content);
    lv_obj_add_style(uptime_cont, &style_card, 0);
    lv_obj_set_width(uptime_cont, LV_PCT(100));
    lv_obj_set_height(uptime_cont, 50);
    lv_obj_set_style_pad_all(uptime_cont, 12, 0);

    lv_obj_t *uptime_label = lv_label_create(uptime_cont);
    lv_obj_add_style(uptime_label, &style_label, 0);
    lv_label_set_text_fmt(uptime_label, "Uptime: %luh %lum", 
                          (unsigned long)hours, (unsigned long)minutes);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_width(back_btn, 120);
    lv_obj_set_height(back_btn, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(system_info_group, back_btn);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_obj_add_style(back_label, &style_label, 0);
    lv_label_set_text(back_label, "< Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "System info screen created");
}

// =============================================
// ЭКРАН RESET CONFIRM
// =============================================

static void reset_confirm_yes_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    ESP_LOGI(TAG, "Resetting system to defaults");
    
    system_config_t config;
    if (config_manager_reset_to_defaults(&config) == ESP_OK) {
        ESP_LOGI(TAG, "System reset successful, restarting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
}

static void reset_confirm_no_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    ESP_LOGI(TAG, "Reset cancelled");
    show_screen(SCREEN_SYSTEM_STATUS);
}

static void __attribute__((unused)) create_reset_confirm_screen(void)
{
    if (reset_confirm_screen != NULL) {
        ESP_LOGI(TAG, "Reset confirm screen already exists");
        return;
    }

    reset_confirm_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(reset_confirm_screen);
    lv_obj_add_style(reset_confirm_screen, &style_bg, 0);
    lv_obj_set_style_pad_all(reset_confirm_screen, 16, 0);

    // Статус-бар
    create_status_bar(reset_confirm_screen, "Reset Confirmation");

    // Основной контент
    lv_obj_t *content = lv_obj_create(reset_confirm_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 32, 320 - 60);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 16, 0);

    // Создаем группу для навигации
    if (reset_confirm_group == NULL) {
        reset_confirm_group = lv_group_create();
        lv_group_set_wrap(reset_confirm_group, true);
    } else {
        lv_group_remove_all_objs(reset_confirm_group);
    }

    // Иконка предупреждения
    lv_obj_t *warning_icon = lv_label_create(content);
    lv_obj_add_style(warning_icon, &style_detail_title, 0);
    lv_obj_set_style_text_color(warning_icon, COLOR_WARNING, 0);
    lv_label_set_text(warning_icon, LV_SYMBOL_WARNING);

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Reset to Defaults?");
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // Описание
    lv_obj_t *desc = lv_label_create(content);
    lv_obj_add_style(desc, &style_label, 0);
    lv_label_set_text(desc, 
        "This will reset all settings to factory defaults and restart the system.\n\n"
        "Are you sure?");
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, LV_PCT(100));
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);

    // Контейнер для кнопок
    lv_obj_t *btn_cont = lv_obj_create(content);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_width(btn_cont, LV_PCT(100));
    lv_obj_set_height(btn_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_cont, 16, 0);

    // Кнопка Нет
    lv_obj_t *no_btn = lv_btn_create(btn_cont);
    lv_obj_add_style(no_btn, &style_button, 0);
    lv_obj_set_width(no_btn, 90);
    lv_obj_set_height(no_btn, 45);
    lv_obj_add_event_cb(no_btn, reset_confirm_no_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(reset_confirm_group, no_btn);
    
    lv_obj_t *no_label = lv_label_create(no_btn);
    lv_label_set_text(no_label, "No");
    lv_obj_center(no_label);

    // Кнопка Да
    lv_obj_t *yes_btn = lv_btn_create(btn_cont);
    lv_obj_add_style(yes_btn, &style_button, 0);
    lv_obj_set_style_bg_color(yes_btn, COLOR_DANGER, 0);
    lv_obj_set_width(yes_btn, 90);
    lv_obj_set_height(yes_btn, 45);
    lv_obj_add_event_cb(yes_btn, reset_confirm_yes_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(reset_confirm_group, yes_btn);
    
    lv_obj_t *yes_label = lv_label_create(yes_btn);
    lv_label_set_text(yes_label, "Yes");
    lv_obj_center(yes_label);

    ESP_LOGI(TAG, "Reset confirm screen created");
}

// =============================================
// НОВЫЕ ЭКРАНЫ ДЛЯ МОБИЛЬНОГО ПРИЛОЖЕНИЯ
// =============================================

/**
 * @brief Создание экрана подключения к мобильному приложению
 */
static void __attribute__((unused)) create_mobile_connect_screen(void)
{
    static lv_obj_t *mobile_screen = NULL;

    if (mobile_screen != NULL) return;

    mobile_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(mobile_screen);
    lv_obj_add_style(mobile_screen, &style_bg, 0);

    // Статус-бар
    create_status_bar(mobile_screen, "📱 Мобильное приложение");

    // Основной контент
    lv_obj_t *content = lv_obj_create(mobile_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 16, 320 - 80);
    lv_obj_set_pos(content, 8, 48);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 16, 0);

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Подключение к мобильному приложению");
    lv_obj_set_width(title, 240 - 32);

    // Статус подключения
    lv_obj_t *status_label = lv_label_create(content);
    lv_obj_add_style(status_label, &style_label, 0);
    lv_label_set_text(status_label, "Статус: Поиск устройств...");
    lv_obj_set_width(status_label, 240 - 32);

    // Инструкции
    lv_obj_t *instructions = lv_label_create(content);
    lv_obj_add_style(instructions, &style_detail_info, 0);
    lv_label_set_text(instructions,
        "1. Убедитесь, что Bluetooth включен\n"
        "2. Откройте мобильное приложение\n"
        "3. Выберите устройство HydroMonitor\n"
        "4. Дождитесь подключения");
    lv_obj_set_width(instructions, 240 - 32);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_size(back_btn, 80, 35);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Экран мобильного приложения создан");
}

/**
 * @brief Создание экрана сетевых настроек
 */
static void __attribute__((unused)) create_network_settings_screen(void)
{
    static lv_obj_t *network_screen = NULL;

    if (network_screen != NULL) return;

    network_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(network_screen);
    lv_obj_add_style(network_screen, &style_bg, 0);

    // Статус-бар
    create_status_bar(network_screen, "🌐 Сетевые настройки");

    // Основной контент
    lv_obj_t *content = lv_obj_create(network_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 16, 320 - 80);
    lv_obj_set_pos(content, 8, 48);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 16, 0);

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Настройки сети");
    lv_obj_set_width(title, 240 - 32);

    // WiFi статус
    lv_obj_t *wifi_status = lv_label_create(content);
    lv_obj_add_style(wifi_status, &style_label, 0);
    lv_label_set_text(wifi_status, "WiFi: Подключено к HydroMonitor-AP");
    lv_obj_set_width(wifi_status, 240 - 32);

    // IP адрес
    lv_obj_t *ip_label = lv_label_create(content);
    lv_obj_add_style(ip_label, &style_detail_info, 0);
    lv_label_set_text(ip_label, "IP: 192.168.4.1");
    lv_obj_set_width(ip_label, 240 - 32);

    // Bluetooth статус
    lv_obj_t *bt_status = lv_label_create(content);
    lv_obj_add_style(bt_status, &style_label, 0);
    lv_label_set_text(bt_status, "Bluetooth: Активен");
    lv_obj_set_width(bt_status, 240 - 32);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_size(back_btn, 80, 35);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Экран сетевых настроек создан");
}

/**
 * @brief Создание экрана статуса системы
 */
static void __attribute__((unused)) create_system_status_screen(void)
{
    static lv_obj_t *status_screen = NULL;

    if (status_screen != NULL) return;

    status_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(status_screen);
    lv_obj_add_style(status_screen, &style_bg, 0);

    // Статус-бар
    create_status_bar(status_screen, "📊 Статус системы");

    // Основной контент
    lv_obj_t *content = lv_obj_create(status_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 16, 320 - 80);
    lv_obj_set_pos(content, 8, 48);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 16, 0);

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Системная информация");
    lv_obj_set_width(title, 240 - 32);

    // Информация о системе
    lv_obj_t *sys_info = lv_label_create(content);
    lv_obj_add_style(sys_info, &style_label, 0);
    lv_label_set_text(sys_info,
        "• ESP32-S3 Dual Core\n"
        "• RAM: 512KB + 8MB PSRAM\n"
        "• Flash: 4MB\n"
        "• Дисплей: ILI9341 240x320\n"
        "• FreeRTOS + LVGL");
    lv_obj_set_width(sys_info, 240 - 32);
    lv_label_set_long_mode(sys_info, LV_LABEL_LONG_WRAP);

    // Статистика памяти
    lv_obj_t *memory_info = lv_label_create(content);
    lv_obj_add_style(memory_info, &style_detail_info, 0);
    lv_label_set_text(memory_info, "Память: 85% свободно");
    lv_obj_set_width(memory_info, 240 - 32);

    // Время работы
    lv_obj_t *uptime_info = lv_label_create(content);
    lv_obj_add_style(uptime_info, &style_detail_info, 0);
    lv_label_set_text(uptime_info, "Время работы: 02:34:12");
    lv_obj_set_width(uptime_info, 240 - 32);

    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(content);
    lv_obj_add_style(back_btn, &style_button_secondary, 0);
    lv_obj_set_size(back_btn, 80, 35);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Назад");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Экран статуса системы создан");
}

/**
 * @brief Создание экрана OTA обновлений
 */
static void __attribute__((unused)) create_ota_update_screen(void)
{
    static lv_obj_t *ota_screen = NULL;

    if (ota_screen != NULL) return;

    ota_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(ota_screen);
    lv_obj_add_style(ota_screen, &style_bg, 0);

    // Статус-бар
    create_status_bar(ota_screen, "⬆️ OTA обновления");

    // Основной контент
    lv_obj_t *content = lv_obj_create(ota_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 240 - 16, 320 - 80);
    lv_obj_set_pos(content, 8, 48);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, 16, 0);

    // Заголовок
    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_detail_title, 0);
    lv_label_set_text(title, "Проверка обновлений");
    lv_obj_set_width(title, 240 - 32);

    // Текущая версия
    lv_obj_t *version_info = lv_label_create(content);
    lv_obj_add_style(version_info, &style_label, 0);
    lv_label_set_text(version_info, "Текущая версия: v3.0.0");
    lv_obj_set_width(version_info, 240 - 32);

    // Статус проверки
    lv_obj_t *check_status = lv_label_create(content);
    lv_obj_add_style(check_status, &style_detail_info, 0);
    lv_label_set_text(check_status, "Проверка обновлений...");
    lv_obj_set_width(check_status, 240 - 32);

    // Прогресс-бар для обновления
    lv_obj_t *progress_bar = lv_bar_create(content);
    lv_obj_set_size(progress_bar, 200, 20);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_center(progress_bar);

    // Кнопки управления
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_width(btn_container, 240 - 32);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *check_btn = lv_btn_create(btn_container);
    lv_obj_add_style(check_btn, &style_button, 0);
    lv_obj_set_size(check_btn, 80, 35);
    lv_obj_t *check_label = lv_label_create(check_btn);
    lv_label_set_text(check_label, "Проверить");
    lv_obj_center(check_label);

    lv_obj_t *update_btn = lv_btn_create(btn_container);
        lv_obj_add_style(update_btn, &style_button, 0);
        lv_obj_set_size(update_btn, 80, 35);
        lv_obj_t *update_label = lv_label_create(update_btn);
        lv_label_set_text(update_label, "Обновить");
        lv_obj_center(update_label);

        // Кнопка назад
        lv_obj_t *back_btn = lv_btn_create(content);
        lv_obj_add_style(back_btn, &style_button_secondary, 0);
        lv_obj_set_size(back_btn, 80, 35);
        lv_obj_t *back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, "Назад");
        lv_obj_center(back_label);

    ESP_LOGI(TAG, "Экран OTA обновлений создан");
}

