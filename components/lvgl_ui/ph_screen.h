#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Структура параметров pH
typedef struct {
    float current_value;              // Текущее значение pH
    float target_value;               // Установленное (целевое) значение pH
    
    // Пределы срабатывания уведомлений
    float notification_high;          // Верхний предел для уведомления
    float notification_low;           // Нижний предел для уведомления
    
    // Пределы срабатывания насосов коррекции
    float pump_high;                  // Верхний предел для включения насоса pH-
    float pump_low;                   // Нижний предел для включения насоса pH+
    
    // Калибровочные данные (3 точки)
    float cal_point1_ref;             // Эталонное значение точки 1 (обычно 4.0)
    float cal_point1_raw;             // Измеренное значение точки 1
    float cal_point2_ref;             // Эталонное значение точки 2 (обычно 7.0)
    float cal_point2_raw;             // Измеренное значение точки 2
    float cal_point3_ref;             // Эталонное значение точки 3 (обычно 10.0)
    float cal_point3_raw;             // Измеренное значение точки 3
    bool calibration_valid;           // Флаг валидности калибровки
} ph_params_t;

// Инициализация экранов pH
esp_err_t ph_screen_init(void);

// Получение/установка параметров
esp_err_t ph_get_params(ph_params_t *params);
esp_err_t ph_set_params(const ph_params_t *params);

// Обновление текущего значения
esp_err_t ph_update_current_value(float value);

// Сохранение/загрузка из NVS
esp_err_t ph_save_to_nvs(void);
esp_err_t ph_load_from_nvs(void);

// Callback для возврата на главный экран
typedef void (*ph_close_callback_t)(void);

// Управление экранами
esp_err_t ph_show_detail_screen(void);
esp_err_t ph_show_settings_screen(void);
esp_err_t ph_show_calibration_screen(void);
esp_err_t ph_close_screen(void);
void ph_set_close_callback(ph_close_callback_t callback);

// Калибровка
esp_err_t ph_calibration_start(void);
esp_err_t ph_calibration_set_point(uint8_t point_num, float reference_value);
esp_err_t ph_calibration_finish(void);
esp_err_t ph_calibration_cancel(void);

#ifdef __cplusplus
}
#endif

