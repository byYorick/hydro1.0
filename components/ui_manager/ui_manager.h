#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Типы экранов
typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_SENSOR_DETAIL,
    UI_SCREEN_SENSOR_SETTINGS,
    UI_SCREEN_COUNT
} ui_screen_type_t;

// Типы датчиков
typedef enum {
    SENSOR_PH = 0,
    SENSOR_EC,
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_LUX,
    SENSOR_CO2,
    SENSOR_COUNT
} sensor_type_t;

// Структура данных датчика
typedef struct {
    float current_value;
    float target_value;
    float min_value;
    float max_value;
    bool alarm_enabled;
    float alarm_low;
    float alarm_high;
    const char *unit;
    const char *name;
    const char *description;
    uint8_t decimals;
} sensor_data_t;

// Структура экрана
typedef struct {
    lv_obj_t *screen;
    ui_screen_type_t type;
    sensor_type_t sensor_type;
    bool is_initialized;
    bool is_visible;
} ui_screen_t;

// Структура конфигурации UI
typedef struct {
    lv_color_t bg_color;
    lv_color_t card_color;
    lv_color_t accent_color;
    lv_color_t text_color;
    lv_color_t text_muted_color;
    lv_color_t danger_color;
    lv_color_t warning_color;
    lv_color_t normal_color;
} ui_theme_t;

// Инициализация UI менеджера
esp_err_t ui_manager_init(void);

// Управление экранами
esp_err_t ui_show_screen(ui_screen_type_t screen_type, sensor_type_t sensor_type);
esp_err_t ui_hide_screen(ui_screen_type_t screen_type, sensor_type_t sensor_type);
esp_err_t ui_show_main_screen(void);
bool ui_is_screen_visible(ui_screen_type_t screen_type, sensor_type_t sensor_type);

// Управление данными датчиков
esp_err_t ui_update_sensor_data(sensor_type_t sensor_type, const sensor_data_t *data);
esp_err_t ui_get_sensor_data(sensor_type_t sensor_type, sensor_data_t *data);

// Управление навигацией
esp_err_t ui_set_focus(sensor_type_t sensor_type);
sensor_type_t ui_get_focus(void);
esp_err_t ui_handle_encoder_event(uint32_t key, int32_t diff);

// Управление темой
esp_err_t ui_set_theme(const ui_theme_t *theme);
esp_err_t ui_get_theme(ui_theme_t *theme);

// Утилиты
const char* ui_get_sensor_name(sensor_type_t sensor_type);
const char* ui_get_sensor_unit(sensor_type_t sensor_type);
sensor_type_t ui_get_sensor_count(void);

#ifdef __cplusplus
}
#endif
