#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Инициализация дисплея ILI9341 с поддержкой LVGL
 * 
 * @return lv_disp_t* Указатель на драйвер дисплея LVGL
 */
lv_display_t* lcd_ili9341_init(void);

/**
 * @brief Установка яркости дисплея
 * 
 * @param brightness Уровень яркости (0-100)
 */
void lcd_ili9341_set_brightness(uint8_t brightness);

/**
 * @brief Обновление значений датчиков на дисплее (устаревшая функция)
 * 
 * @param ph Значение pH
 * @param ec Значение EC
 * @param temp Значение температуры
 * @param hum Значение влажности
 * @param lux Значение освещенности
 * @param co2 Значение CO2
 */
void lcd_ili9341_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2);

/**
 * @brief Блокировка мьютекса LVGL
 * 
 * @param timeout_ms Таймаут в миллисекундах (-1 для бесконечного ожидания)
 * @return true если блокировка получена, false в противном случае
 */
bool lvgl_lock(int timeout_ms);

/**
 * @brief Разблокировка мьютекса LVGL
 */
void lvgl_unlock(void);

/**
 * @brief Получить устройство ввода энкодера
 * 
 * @return lv_indev_t* Указатель на устройство ввода энкодера
 */
lv_indev_t* lcd_ili9341_get_encoder_indev(void);

#ifdef __cplusplus
}
#endif