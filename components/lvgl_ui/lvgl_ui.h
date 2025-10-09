#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "system_config.h"

// Примечание: этот файл был переименован из lvgl_main.h в lvgl_ui.h
// в рамках рефакторинга компонента lvgl

/**
 * @brief Инициализация пользовательского интерфейса LVGL
 * 
 * Эта функция инициализирует основной пользовательский интерфейс для отображения
 * данных сенсоров на дисплее. Она создает все необходимые элементы интерфейса
 * и запускает задачу обновления отображения.
 */
void lvgl_main_init(void);

/**
 * @brief Обновление значений датчиков на дисплее
 * 
 * Эта функция отправляет новые значения датчиков в очередь для обновления
 * отображения на экране. Значения будут отображены в соответствующих полях
 * пользовательского интерфейса.
 * 
 * @param ph Значение pH (например, 6.5)
 * @param ec Значение EC (например, 1.2)
 * @param temp Значение температуры в градусах Цельсия (например, 24.5)
 * @param hum Значение влажности в процентах (например, 65.0)
 * @param lux Значение освещенности в люксах (например, 1200.0)
 * @param co2 Значение CO2 в ppm (например, 450.0)
 */
void lvgl_update_sensor_values(float ph, float ec, float temp, float hum, float lux, float co2);

// Глобальная переменная для отслеживания вращения энкодера
extern int32_t last_encoder_diff;

// Структура данных датчиков определена в system_config.h

/**
 * @brief Обновление значений датчиков на дисплее из структуры данных
 * 
 * Эта функция обновляет отображение значений датчиков на экране, используя
 * структуру с данными датчиков.
 * 
 * @param data Указатель на структуру с данными датчиков
 */
void lvgl_update_sensor_values_from_queue(sensor_data_t *data);

/**
 * @brief Получение дескриптора очереди данных датчиков
 * 
 * Эта функция возвращает дескриптор очереди, в которую помещаются данные датчиков
 * для обновления отображения на экране.
 * 
 * @return Дескриптор очереди данных датчиков
 */
void* lvgl_get_sensor_data_queue(void);

// LEGACY FUNCTIONS REMOVED: lvgl_set_focus(), lvgl_get_focus_index(), 
// lvgl_get_total_focus_items(), lvgl_clear_focus_group()
// Фокус теперь управляется автоматически через Screen Manager
// Используйте API Screen Manager для управления навигацией

// LEGACY FUNCTIONS REMOVED: lvgl_open_detail_screen(), lvgl_close_detail_screen(), lvgl_is_detail_screen_open()
// Детальные экраны теперь управляются через Screen Manager API:
// - screen_show("detail_ph", NULL) для открытия
// - screen_go_back() для закрытия
// - screen_get_current() для проверки состояния