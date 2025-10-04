#pragma once

#include "lvgl.h"

// Типы экранов
typedef enum {
    SCREEN_PH_DETAIL,
    SCREEN_EC_DETAIL,
    SCREEN_TEMP_DETAIL,
    SCREEN_HUMIDITY_DETAIL,
    SCREEN_LUX_DETAIL,
    SCREEN_CO2_DETAIL,
    SCREEN_PH_SETTINGS,
    SCREEN_EC_SETTINGS,
    SCREEN_TEMP_SETTINGS,
    SCREEN_HUMIDITY_SETTINGS,
    SCREEN_LUX_SETTINGS,
    SCREEN_CO2_SETTINGS
} sensor_screen_type_t;

// Структура данных экрана
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *chart;
    lv_obj_t *current_value_label;
    lv_obj_t *target_value_label;
    lv_obj_t *settings_button;
    lv_obj_t *back_button;
    bool is_initialized;
} sensor_screen_t;

// Функции инициализации экранов
void ph_detail_screen_init(void);
void ec_detail_screen_init(void);
void temp_detail_screen_init(void);
void humidity_detail_screen_init(void);
void lux_detail_screen_init(void);
void co2_detail_screen_init(void);

void ph_settings_screen_init(void);
void ec_settings_screen_init(void);
void temp_settings_screen_init(void);
void humidity_settings_screen_init(void);
void lux_settings_screen_init(void);
void co2_settings_screen_init(void);

// Функции управления экранами
void show_sensor_screen(sensor_screen_type_t screen_type);
void hide_sensor_screen(sensor_screen_type_t screen_type);
void update_sensor_screen_data(sensor_screen_type_t screen_type, float current_value, float target_value);
void destroy_sensor_screen(sensor_screen_type_t screen_type);

// Функции обновления данных для каждого датчика
void ph_update_data(float current_value, float target_value);
void ec_update_data(float current_value, float target_value);
void temp_update_data(float current_value, float target_value);
void humidity_update_data(float current_value, float target_value);
void lux_update_data(float current_value, float target_value);
void co2_update_data(float current_value, float target_value);

// Глобальные переменные экранов
extern sensor_screen_t ph_detail_screen;
extern sensor_screen_t ec_detail_screen;
extern sensor_screen_t temp_detail_screen;
extern sensor_screen_t humidity_detail_screen;
extern sensor_screen_t lux_detail_screen;
extern sensor_screen_t co2_detail_screen;

extern sensor_screen_t ph_settings_screen;
extern sensor_screen_t ec_settings_screen;
extern sensor_screen_t temp_settings_screen;
extern sensor_screen_t humidity_settings_screen;
extern sensor_screen_t lux_settings_screen;
extern sensor_screen_t co2_settings_screen;
