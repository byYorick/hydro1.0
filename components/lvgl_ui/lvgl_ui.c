#include "lvgl_ui.h"
#include "lvgl.h"
#include "lcd_ili9341.h"
#include "encoder.h"
#include <inttypes.h>

// Screen Manager System
#include "screen_manager/screen_manager.h"
#include "screen_manager/screen_init.h"
#include "screens/main_screen.h"
#include "screens/notification_screen.h"
#include "lvgl_styles.h"
#include "../error_handler/error_handler.h"
// Используем только встроенные шрифты LVGL
#include "montserrat14_ru.h"

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
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

    SCREEN_COUNT
} screen_type_t;

/* =============================
 *  ENCODER NAVIGATION
 * ============================= */
static bool encoder_navigation_enabled = true;
int32_t last_encoder_diff = 0;

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
// Глобальные стили (экспортированы в lvgl_styles.h)
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

// Стили для PID экранов
lv_style_t style_pid_card;
lv_style_t style_pid_active;
lv_style_t style_pid_idle;
lv_style_t style_pid_learning;
lv_style_t style_pid_predicting;
lv_style_t style_pid_tuning;
lv_style_t style_pid_target;
lv_style_t style_pid_error;
lv_style_t style_param_normal;
lv_style_t style_param_focused;
lv_style_t style_param_editing;
lv_style_t style_progress_bg;
lv_style_t style_progress_indicator;
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
lv_style_t style_detail_value_big;
lv_style_t style_pump_widget;
static bool styles_initialized = false;

static QueueHandle_t sensor_data_queue = NULL;
static bool display_task_started = false;
static sensor_data_t last_sensor_data = {0};
static lv_coord_t sensor_history[SENSOR_COUNT][HISTORY_POINTS];
static uint16_t sensor_history_pos[SENSOR_COUNT];
static bool sensor_history_full[SENSOR_COUNT];
static bool sensor_snapshot_valid = false;

/* =============================
 *  FORWARD DECLARATIONS
 * ============================= */
static float get_sensor_value_by_index(const sensor_data_t *data, int index);
static void record_sensor_value(int index, float value);
static void update_sensor_display(sensor_data_t *data);
static void display_update_task(void *pvParameters);
static void encoder_task(void *pvParameters);
static void handle_encoder_event(encoder_event_t *event);
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

    // Стиль заголовка - компактный темный с акцентом
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_header, 4);  // Компактные вертикальные отступы
    lv_style_set_pad_hor(&style_header, 8);  // Компактные горизонтальные отступы
    lv_style_set_radius(&style_header, 0);

    // Стиль основного заголовка - компактный
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, COLOR_TEXT);
    lv_style_set_text_font(&style_title, &montserrat_ru);
    lv_style_set_text_opa(&style_title, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_title, 2);  // Минимальные отступы

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
    lv_style_set_text_font(&style_value_large, &montserrat_ru);
    lv_style_set_text_opa(&style_value_large, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_value_large, 4);

    // Стиль единиц измерения - компактный
    lv_style_init(&style_unit);
    lv_style_set_text_color(&style_unit, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_unit, &montserrat_ru); // Русский шрифт с кириллицей
    lv_style_set_text_opa(&style_unit, LV_OPA_COVER);

    // Стиль названий датчиков - читаемый шрифт
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, COLOR_TEXT);
    lv_style_set_text_font(&style_label, &montserrat_ru); // Русский шрифт с кириллицей
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

    // Основной стиль кнопок - компактный бирюзовый акцент
    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_border_width(&style_button, 0);
    lv_style_set_radius(&style_button, 6);  // Меньший радиус
    lv_style_set_pad_ver(&style_button, 6);  // Компактные отступы
    lv_style_set_pad_hor(&style_button, 10);  // Компактные отступы
    lv_style_set_shadow_color(&style_button, COLOR_ACCENT);
    lv_style_set_shadow_width(&style_button, 1);  // Меньшая тень
    lv_style_set_shadow_opa(&style_button, LV_OPA_20);  // Более прозрачная тень
    lv_style_set_shadow_ofs_y(&style_button, 1);  // Меньшее смещение

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
    lv_style_set_text_font(&style_detail_title, &montserrat_ru);
    lv_style_set_text_opa(&style_detail_title, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_title, 8);

    // Стиль значения в детализации - очень крупный
    lv_style_init(&style_detail_value);
    lv_style_set_text_color(&style_detail_value, COLOR_ACCENT_SOFT);
    lv_style_set_text_font(&style_detail_value, &montserrat_ru);
    lv_style_set_text_opa(&style_detail_value, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_value, 8);

    // Стиль очень крупного значения в детализации (алиас для style_detail_value)
    lv_style_init(&style_detail_value_big);
    lv_style_set_text_color(&style_detail_value_big, COLOR_ACCENT_SOFT);
    lv_style_set_text_font(&style_detail_value_big, &montserrat_ru);  // Русский шрифт с кириллицей
    lv_style_set_text_opa(&style_detail_value_big, LV_OPA_COVER);
    lv_style_set_pad_ver(&style_detail_value_big, 12);

    // Стиль дополнительной информации
    lv_style_init(&style_detail_info);
    lv_style_set_text_color(&style_detail_info, COLOR_TEXT_MUTED);
    lv_style_set_text_font(&style_detail_info, &montserrat_ru);
    lv_style_set_text_opa(&style_detail_info, LV_OPA_COVER);

    // =============================================
    // СТИЛЬ ФОКУСА - РАМКА ВОКРУГ ЭЛЕМЕНТА
    // =============================================
    // Компактный стиль фокуса
    lv_style_init(&style_focus);
    lv_style_set_border_color(&style_focus, COLOR_ACCENT);       // Бирюзовая рамка
    lv_style_set_border_width(&style_focus, 2);                  // Компактная рамка 2px
    lv_style_set_border_opa(&style_focus, LV_OPA_COVER);         // Полная непрозрачность
    lv_style_set_outline_color(&style_focus, COLOR_ACCENT);      // Внешняя обводка
    lv_style_set_outline_width(&style_focus, 1);                 // Тонкая обводка 1px
    lv_style_set_outline_pad(&style_focus, 1);                   // Минимальный отступ
    lv_style_set_outline_opa(&style_focus, LV_OPA_40);           // Более прозрачная обводка

    // =============================================
    // СТИЛИ ДЛЯ ВИДЖЕТОВ КАЛИБРОВКИ
    // =============================================
    
    // Стиль контейнера виджета калибровки насоса (ОПТИМИЗАЦИЯ!)
    lv_style_init(&style_pump_widget);
    lv_style_set_bg_color(&style_pump_widget, lv_color_hex(0x2a2a2a));
    lv_style_set_bg_opa(&style_pump_widget, LV_OPA_COVER);
    lv_style_set_border_color(&style_pump_widget, lv_color_hex(0x444444));
    lv_style_set_border_width(&style_pump_widget, 1);
    lv_style_set_radius(&style_pump_widget, 8);
    lv_style_set_pad_all(&style_pump_widget, 6);
    
    // =============================================
    // УСТАНОВКА ДЕФОЛТНЫХ ШРИФТОВ
    // =============================================
    // Используем montserrat_ru как дефолтный шрифт с fallback на встроенный шрифт для иконок
    // Это обеспечит поддержку кириллицы и символов LVGL (LV_SYMBOL_*)
    lv_theme_t *theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_color_hex(0x00D4AA),  // COLOR_ACCENT
        lv_color_hex(0x0F1419),  // COLOR_BG
        true,                     // dark theme
        &montserrat_ru           // дефолтный шрифт
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);
    
    ESP_LOGI(TAG, "Default font set to montserrat_ru with fallback for icons");
    
    // Инициализация PID стилей
    init_pid_styles();

    styles_initialized = true;
    ESP_LOGI(TAG, "UI styles initialized with improved color scheme for 240x320 display");
}

/**
 * @brief Инициализация стилей для PID экранов
 */
void init_pid_styles(void) {
    // Стиль карточки PID - базовый
    lv_style_init(&style_pid_card);
    lv_style_set_bg_color(&style_pid_card, lv_color_hex(0x2a2a2a));
    lv_style_set_bg_opa(&style_pid_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_pid_card, 2);
    lv_style_set_border_color(&style_pid_card, lv_color_hex(0x3a3a3a));
    lv_style_set_radius(&style_pid_card, 8);
    lv_style_set_pad_all(&style_pid_card, 8);
    
    // PID активен - желтая рамка
    lv_style_init(&style_pid_active);
    lv_style_set_border_color(&style_pid_active, lv_color_hex(0xFFC107)); // Желтый
    lv_style_set_border_width(&style_pid_active, 3);
    
    // PID неактивен - серый
    lv_style_init(&style_pid_idle);
    lv_style_set_border_color(&style_pid_idle, lv_color_hex(0x5a5a5a)); // Серый
    lv_style_set_border_width(&style_pid_idle, 2);
    
    // Режим обучения - синяя рамка
    lv_style_init(&style_pid_learning);
    lv_style_set_border_color(&style_pid_learning, lv_color_hex(0x2196F3)); // Синий
    lv_style_set_border_width(&style_pid_learning, 3);
    
    // Упреждающая коррекция - фиолетовая рамка
    lv_style_init(&style_pid_predicting);
    lv_style_set_border_color(&style_pid_predicting, lv_color_hex(0x9C27B0)); // Фиолетовый
    lv_style_set_border_width(&style_pid_predicting, 3);
    
    // Автонастройка - оранжевая рамка
    lv_style_init(&style_pid_tuning);
    lv_style_set_border_color(&style_pid_tuning, lv_color_hex(0xFF9800)); // Оранжевый
    lv_style_set_border_width(&style_pid_tuning, 3);
    
    // Цель достигнута - зеленая рамка
    lv_style_init(&style_pid_target);
    lv_style_set_border_color(&style_pid_target, lv_color_hex(0x4CAF50)); // Зеленый
    lv_style_set_border_width(&style_pid_target, 3);
    
    // Ошибка - красная рамка
    lv_style_init(&style_pid_error);
    lv_style_set_border_color(&style_pid_error, lv_color_hex(0xF44336)); // Красный
    lv_style_set_border_width(&style_pid_error, 3);
    
    // Параметр в нормальном режиме
    lv_style_init(&style_param_normal);
    lv_style_set_bg_color(&style_param_normal, lv_color_hex(0x3a3a3a));
    lv_style_set_bg_opa(&style_param_normal, LV_OPA_COVER);
    lv_style_set_radius(&style_param_normal, 4);
    lv_style_set_pad_all(&style_param_normal, 6);
    lv_style_set_text_color(&style_param_normal, lv_color_white());
    
    // Параметр в фокусе - голубая рамка
    lv_style_init(&style_param_focused);
    lv_style_set_border_color(&style_param_focused, lv_color_hex(0x00D4AA));
    lv_style_set_border_width(&style_param_focused, 2);
    
    // Параметр редактируется - оранжевый фон
    lv_style_init(&style_param_editing);
    lv_style_set_bg_color(&style_param_editing, lv_color_hex(0xFF6B35)); // Оранжевый
    lv_style_set_text_color(&style_param_editing, lv_color_white());
    
    // Прогресс-бар фон
    lv_style_init(&style_progress_bg);
    lv_style_set_bg_color(&style_progress_bg, lv_color_hex(0x3a3a3a));
    lv_style_set_bg_opa(&style_progress_bg, LV_OPA_COVER);
    lv_style_set_radius(&style_progress_bg, 4);
    
    // Прогресс-бар индикатор
    lv_style_init(&style_progress_indicator);
    lv_style_set_bg_color(&style_progress_indicator, lv_color_hex(0x00D4AA));
    lv_style_set_bg_opa(&style_progress_indicator, LV_OPA_COVER);
    
    ESP_LOGI(TAG, "PID styles initialized");
}

static float get_sensor_value_by_index(const sensor_data_t *data, int index)
{
    switch (index) {
        case 0: return data->ph;
        case 1: return data->ec;
        case 2: return data->temperature;
        case 3: return data->humidity;
        case 4: return data->lux;
        case 5: return data->co2;
        default: return 0.0f;
    }
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

}

/* =============================
 *  SENSOR DATA HANDLING
 * ============================= */
static void update_sensor_display(sensor_data_t *data)
{
    last_sensor_data = *data;
    sensor_snapshot_valid = true;

    static int update_count = 0;
    update_count++;
    
    if (update_count % 10 == 0) {
        ESP_LOGI(TAG, "Updating sensors #%d: pH=%.2f EC=%.2f T=%.1f", 
                 update_count, data->ph, data->ec, data->temperature);
    }

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        float value = get_sensor_value_by_index(data, i);

        extern esp_err_t main_screen_update_sensor(uint8_t sensor_index, float value);
        esp_err_t ret = main_screen_update_sensor(i, value);
        
        if (ret != ESP_OK && update_count % 10 == 0) {
            ESP_LOGW(TAG, "Failed to update sensor %d: %s", i, esp_err_to_name(ret));
        }
        
        record_sensor_value(i, value);
    }
}

static void display_update_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);
    
    ESP_LOGI(TAG, "Display update task started, waiting for sensor data...");
    
    sensor_data_t sensor_data;
    int cycle_count = 0;
    
    while (1) {
        bool data_processed = false;
        while (xQueueReceive(sensor_data_queue, &sensor_data, 0) == pdTRUE) {
            data_processed = true;
        }
        
        // КРИТИЧНО: Блокируем LVGL для обработки очередей даже БЕЗ данных датчиков
        if (!lvgl_lock(100)) {
            ESP_LOGW(TAG, "Failed to get LVGL lock in display task");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        if (lv_is_initialized()) {
            // Обновляем датчики если есть данные
            if (data_processed) {
                cycle_count++;
                update_sensor_display(&sensor_data);
            }
            
            // КРИТИЧНО: Обрабатываем очередь уведомлений ВСЕГДА (независимо от датчиков)
            notification_screen_process_queue();
        }
        lvgl_unlock();
        
        if (!data_processed && cycle_count == 0 && (esp_timer_get_time() / 1000000LL) % 30 == 0) {
            ESP_LOGD(TAG, "Display task alive, waiting for sensor data...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* =============================
 *  PUBLIC API
 * ============================= */
void lvgl_main_init(void)
{
    ESP_LOGI(TAG, "=======================================================");
    ESP_LOGI(TAG, "   Initializing UI with Screen Manager System     ");
    ESP_LOGI(TAG, "=======================================================");
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Initializing Screen Manager System...");
    if (lvgl_lock(1000)) {
        init_styles();
        
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
    
    if (sensor_data_queue == NULL) {
        sensor_data_queue = xQueueCreate(SENSOR_DATA_QUEUE_SIZE, sizeof(sensor_data_t));
        if (sensor_data_queue == NULL) {
            ESP_LOGE(TAG, "FAILED to create sensor data queue!");
        } else {
            ESP_LOGI(TAG, "Sensor data queue created successfully");
        }
    }
    
    // Получаем конфигурацию UI из config_manager
    const system_config_t *sys_config = config_manager_get_cached();
    uint32_t display_stack = sys_config ? sys_config->ui_config.display_task_stack_size : 16384;
    uint32_t encoder_stack = sys_config ? sys_config->ui_config.encoder_task_stack_size : 16384;
    uint8_t display_priority = sys_config ? sys_config->ui_config.display_task_priority : 6;
    uint8_t encoder_priority = sys_config ? sys_config->ui_config.encoder_task_priority : 5;
    
    ESP_LOGI(TAG, "UI Task configuration: Display=%lu bytes (prio=%d), Encoder=%lu bytes (prio=%d)",
             (unsigned long)display_stack, display_priority, (unsigned long)encoder_stack, encoder_priority);
    
    if (!display_task_started) {
        TaskHandle_t display_task_handle = NULL;
        BaseType_t task_created = xTaskCreate(display_update_task, "display_update", display_stack, NULL, display_priority, &display_task_handle);
        if (task_created == pdPASS && display_task_handle != NULL) {
            display_task_started = true;
            ESP_LOGI(TAG, "Display update task created successfully");
        } else {
            ESP_LOGE(TAG, "FAILED to create display update task!");
        }
    }
    
    TaskHandle_t encoder_task_handle = NULL;
    BaseType_t encoder_task_created = xTaskCreate(encoder_task, "lvgl_encoder", encoder_stack, NULL, encoder_priority, &encoder_task_handle);
    if (encoder_task_created == pdPASS && encoder_task_handle != NULL) {
        ESP_LOGI(TAG, "Encoder task created successfully");
    } else {
        ESP_LOGE(TAG, "FAILED to create encoder task!");
    }
    
    ESP_LOGI(TAG, "UI initialization complete with Screen Manager");
    ESP_LOGI(TAG, "  - Sensor queue: %s", sensor_data_queue ? "OK" : "FAILED");
    ESP_LOGI(TAG, "  - Display task: %s", display_task_started ? "OK" : "FAILED");
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
 *  ENCODER NAVIGATION FUNCTIONS
 * ============================= */
static void encoder_task(void *pvParameters)
{
    LV_UNUSED(pvParameters);
    
    QueueHandle_t encoder_queue = NULL;
    ESP_LOGI(TAG, "Encoder task started, waiting for encoder initialization...");
    
    // КРИТИЧНО: Подписываем задачу на watchdog
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "Encoder task subscribed to watchdog");
    
    while (encoder_queue == NULL) {
        encoder_queue = encoder_get_event_queue();
        if (encoder_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Feed watchdog во время инициализации
        esp_task_wdt_reset();
    }
    
    ESP_LOGI(TAG, "Encoder queue ready, starting event processing...");
    
    encoder_event_t event;
    while (1) {
        // КРИТИЧНО: Feed watchdog в начале каждого цикла
        esp_task_wdt_reset();
        
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Feed watchdog перед обработкой события
            esp_task_wdt_reset();
            
            // Увеличенный timeout для lazy loading экранов (до 2 секунд для сложных экранов)
            if (!lvgl_lock(2000)) {
                ESP_LOGW(TAG, "Failed to acquire LVGL lock - DROPPING event (avoid queue overflow)");
                // КРИТИЧНО: НЕ возвращаем событие в очередь!
                // Это может вызвать переполнение и deadlock.
                // Просто пропускаем это событие.
                
                // Feed watchdog после ошибки
                esp_task_wdt_reset();
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            
            if (lv_is_initialized()) {
                handle_encoder_event(&event);
            }
            lvgl_unlock();
            
            // Feed watchdog после обработки события
            esp_task_wdt_reset();
        }
        
        // Задержка для предотвращения watchdog timeout
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void handle_encoder_event(encoder_event_t *event)
{
    if (!encoder_navigation_enabled) {
        return;
    }
    
    screen_instance_t *current = screen_get_current();
    if (!current || !current->encoder_group) {
        ESP_LOGW(TAG, "No active screen with encoder group");
        return;
    }
    
    // Периодическая очистка скрытых элементов
    static uint32_t cleanup_counter = 0;
    if (++cleanup_counter >= 100) {
        cleanup_counter = 0;
        int removed = screen_cleanup_hidden_elements(NULL);
        if (removed > 0) {
            ESP_LOGD(TAG, "Periodic cleanup: removed %d hidden elements from encoder group", removed);
        }
    }
    
    // Обработка событий энкодера через Screen Manager
    switch (event->type) {
        case ENCODER_EVENT_ROTATE_CW:
            // КРИТИЧНО: Проверяем количество элементов перед переключением
            if (lv_group_get_obj_count(current->encoder_group) > 1) {
                lv_group_focus_next(current->encoder_group);
                ESP_LOGD(TAG, "Screen Manager: focus next");
            } else {
                ESP_LOGD(TAG, "Screen Manager: only 1 element, ignoring rotate");
            }
            break;
            
        case ENCODER_EVENT_ROTATE_CCW:
            // КРИТИЧНО: Проверяем количество элементов перед переключением
            if (lv_group_get_obj_count(current->encoder_group) > 1) {
                lv_group_focus_prev(current->encoder_group);
                ESP_LOGD(TAG, "Screen Manager: focus prev");
            } else {
                ESP_LOGD(TAG, "Screen Manager: only 1 element, ignoring rotate");
            }
            break;
            
        case ENCODER_EVENT_BUTTON_PRESS:
            // Получаем сфокусированный объект
            {
                uint32_t obj_count = lv_group_get_obj_count(current->encoder_group);
                lv_obj_t *focused = lv_group_get_focused(current->encoder_group);
                
                ESP_LOGI(TAG, ">>> ENCODER PRESS: screen=%s, group=%p, obj_count=%lu, focused=%p",
                         current->config ? current->config->id : "unknown",
                         current->encoder_group,
                         (unsigned long)obj_count,
                         focused);
                
                // КРИТИЧНО: Проверяем, что группа валидна и имеет объекты
                if (obj_count == 0) {
                    ESP_LOGW(TAG, "[FAIL] Encoder group is empty, ignoring press");
                    break;
                }
                
                if (focused) {
                    // КРИТИЧНО: Проверяем, что объект все еще валиден
                    if (!lv_obj_is_valid(focused)) {
                        ESP_LOGW(TAG, "[FAIL] Focused object is invalid, ignoring press");
                        break;
                    }
                    
                    // КРИТИЧНО: Отправляем CLICKED напрямую - самый безопасный вариант
                    // LVGL автоматически обработает как нажатие кнопки
                    lv_obj_send_event(focused, LV_EVENT_CLICKED, NULL);
                    ESP_LOGI(TAG, "[OK] Sent CLICKED event to focused object");
                } else {
                    ESP_LOGW(TAG, "[FAIL] No focused object in group (obj_count=%lu)", (unsigned long)obj_count);
                }
            }
            break;
            
        case ENCODER_EVENT_BUTTON_LONG_PRESS:
            ESP_LOGI(TAG, "Encoder button long press detected (disabled)");
            break;
            
        case ENCODER_EVENT_BUTTON_RELEASE:
            // Игнорируем - событие уже обработано при нажатии
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown encoder event type: %d", event->type);
            break;
    }
}
