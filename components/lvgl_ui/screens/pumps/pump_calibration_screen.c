/**
 * @file pump_calibration_screen.c
 * @brief Реализация экрана калибровки насосов
 */

#include "pump_calibration_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "data_logger.h"
#include "ph_ec_controller.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PUMP_CALIB_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_pump_dropdown = NULL;
static lv_obj_t *g_current_rate_label = NULL;
static lv_obj_t *g_time_input = NULL;
static lv_obj_t *g_start_btn = NULL;
static lv_obj_t *g_countdown_label = NULL;
static lv_obj_t *g_volume_input_container = NULL;
static lv_obj_t *g_volume_input = NULL;
static lv_obj_t *g_result_label = NULL;
static lv_obj_t *g_save_btn = NULL;

// Состояние калибровки
static pump_index_t g_selected_pump = PUMP_INDEX_PH_UP;
static uint32_t g_calibration_time_ms = 10000;
static float g_old_flow_rate = 0;
static bool g_calibration_running = false;
static lv_timer_t *g_countdown_timer = NULL;
static int g_countdown_remaining = 0;

/* =============================
 *  ТАЙМЕР ОБРАТНОГО ОТСЧЕТА
 * ============================= */

static void countdown_timer_cb(lv_timer_t *timer)
{
    g_countdown_remaining -= 100; // Уменьшаем на 100 мс
    
    if (g_countdown_remaining <= 0) {
        // Калибровка завершена
        g_calibration_running = false;
        lv_label_set_text(g_countdown_label, "Калибровка завершена!");
        lv_obj_set_style_text_color(g_countdown_label, lv_color_hex(0x4CAF50), 0);
        
        // Показать контейнер ввода объема
        if (g_volume_input_container) {
            lv_obj_clear_flag(g_volume_input_container, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Остановить таймер
        if (g_countdown_timer) {
            lv_timer_del(g_countdown_timer);
            g_countdown_timer = NULL;
        }
        
        ESP_LOGI(TAG, "Калибровка завершена, ожидание ввода объема");
    } else {
        // Обновить отображение
        char text[64];
        snprintf(text, sizeof(text), "Осталось: %d мс", g_countdown_remaining);
        lv_label_set_text(g_countdown_label, text);
    }
}

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback выбора насоса
 */
static void on_pump_selected(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    g_selected_pump = (pump_index_t)selected;
    
    // Получить текущий flow_rate из конфигурации
    const system_config_t *config = config_manager_get_cached();
    if (config) {
        g_old_flow_rate = config->pump_config[g_selected_pump].flow_rate_ml_per_sec;
        
        char text[64];
        snprintf(text, sizeof(text), "Текущий расход: %.3f мл/сек", g_old_flow_rate);
        lv_label_set_text(g_current_rate_label, text);
    }
    
    ESP_LOGI(TAG, "Выбран насос %s", PUMP_NAMES[g_selected_pump]);
}

/**
 * @brief Запуск калибровки
 */
static void start_calibration(void)
{
    // Запуск калибровки
    g_calibration_running = true;
    g_countdown_remaining = g_calibration_time_ms;
    
    // Скрыть контейнер ввода объема
    if (g_volume_input_container) {
        lv_obj_add_flag(g_volume_input_container, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Показать countdown
    lv_obj_clear_flag(g_countdown_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(g_countdown_label, lv_color_hex(0xFFC107), 0);
    
    // Запустить насос напрямую
    ESP_LOGI(TAG, "Запуск калибровки насоса %s на %lu мс", 
             PUMP_NAMES[g_selected_pump], (unsigned long)g_calibration_time_ms);
    
    pump_manager_run_direct(g_selected_pump, g_calibration_time_ms);
    
    // Запустить таймер обратного отсчета
    g_countdown_timer = lv_timer_create(countdown_timer_cb, 100, NULL);
}

/**
 * @brief Callback для кнопки "Запустить калибровку"
 */
static void on_start_calibration_click(lv_event_t *e)
{
    // Получить время калибровки из поля ввода
    const char *time_text = lv_textarea_get_text(g_time_input);
    g_calibration_time_ms = (uint32_t)atoi(time_text);
    
    if (g_calibration_time_ms < 1000) g_calibration_time_ms = 1000;
    if (g_calibration_time_ms > 30000) g_calibration_time_ms = 30000;
    
    ESP_LOGI(TAG, "Запуск калибровки насоса %s на %lu мс", 
             PUMP_NAMES[g_selected_pump], (unsigned long)g_calibration_time_ms);
    
    // Запуск калибровки напрямую (без popup)
    start_calibration();
}

/**
 * @brief Callback для кнопки "Сохранить"
 */
static void on_save_calibration_click(lv_event_t *e)
{
    // Получить введенный объем
    const char *volume_text = lv_textarea_get_text(g_volume_input);
    float real_volume = atof(volume_text);
    
    if (real_volume <= 0) {
        ESP_LOGW(TAG, "Неверный объем: %.2f", real_volume);
        lv_label_set_text(g_result_label, "Ошибка: неверный объем!");
        lv_obj_set_style_text_color(g_result_label, lv_color_hex(0xF44336), 0);
        return;
    }
    
    // Расчет нового расхода
    float new_flow_rate = real_volume / (g_calibration_time_ms / 1000.0f);
    
    ESP_LOGI(TAG, "Калибровка: старый расход=%.3f, новый расход=%.3f мл/сек",
             g_old_flow_rate, new_flow_rate);
    
    // Сохранение в конфигурацию
    system_config_t config;
    esp_err_t err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_config[g_selected_pump].flow_rate_ml_per_sec = new_flow_rate;
        err = config_save(&config);
        
        if (err == ESP_OK) {
            // Применить конфигурацию
            ph_ec_controller_apply_config(&config);
            
            // Логирование
            data_logger_log_pump_calibration(g_selected_pump, g_old_flow_rate, new_flow_rate);
            
            // Обновить отображение текущего расхода
            g_old_flow_rate = new_flow_rate;
            char text[64];
            snprintf(text, sizeof(text), "Текущий расход: %.3f мл/сек", new_flow_rate);
            lv_label_set_text(g_current_rate_label, text);
            
            // Показать результат
            char result[128];
            snprintf(result, sizeof(result), 
                     "Новый расход: %.3f мл/сек\n(было: %.3f мл/сек)",
                     new_flow_rate, g_old_flow_rate);
            lv_label_set_text(g_result_label, result);
            lv_obj_set_style_text_color(g_result_label, lv_color_hex(0x4CAF50), 0);
            
            ESP_LOGI(TAG, "Калибровка сохранена успешно");
        }
    }
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pump_calibration_screen_create(void *context)
{
    ESP_LOGI(TAG, "Создание экрана калибровки");
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Калибровка");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Калибровка насосов");
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Dropdown выбора насоса
    lv_obj_t *pump_label = lv_label_create(screen);
    lv_label_set_text(pump_label, "Насос:");
    lv_obj_set_style_text_color(pump_label, lv_color_white(), 0);
    lv_obj_align(pump_label, LV_ALIGN_TOP_LEFT, 10, 65);
    
    g_pump_dropdown = lv_dropdown_create(screen);
    lv_dropdown_set_options(g_pump_dropdown, "pH UP\npH DOWN\nEC A\nEC B\nEC C\nWater");
    lv_obj_set_width(g_pump_dropdown, 150);
    lv_obj_align(g_pump_dropdown, LV_ALIGN_TOP_LEFT, 70, 60);
    lv_obj_add_event_cb(g_pump_dropdown, on_pump_selected, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Текущий расход
    g_current_rate_label = lv_label_create(screen);
    lv_label_set_text(g_current_rate_label, "Текущий расход: - мл/сек");
    lv_obj_set_style_text_color(g_current_rate_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(g_current_rate_label, LV_ALIGN_TOP_LEFT, 10, 95);
    
    // Поле ввода времени калибровки
    lv_obj_t *time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "Время (мс):");
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 10, 125);
    
    g_time_input = lv_textarea_create(screen);
    lv_obj_set_size(g_time_input, 100, 35);
    lv_textarea_set_text(g_time_input, "10000");
    lv_textarea_set_one_line(g_time_input, true);
    lv_textarea_set_max_length(g_time_input, 5);
    lv_obj_align(g_time_input, LV_ALIGN_TOP_LEFT, 110, 120);
    
    // Кнопка "Запустить калибровку"
    g_start_btn = lv_btn_create(screen);
    lv_obj_set_size(g_start_btn, 200, 40);
    lv_obj_align(g_start_btn, LV_ALIGN_TOP_MID, 0, 165);
    lv_obj_set_style_bg_color(g_start_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(g_start_btn, on_start_calibration_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *start_label = lv_label_create(g_start_btn);
    lv_label_set_text(start_label, "ЗАПУСТИТЬ КАЛИБРОВКУ");
    lv_obj_center(start_label);
    
    // Countdown label (скрыт по умолчанию)
    g_countdown_label = lv_label_create(screen);
    lv_label_set_text(g_countdown_label, "");
    lv_obj_set_style_text_color(g_countdown_label, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_countdown_label, &montserrat_ru, 0);
    lv_obj_align(g_countdown_label, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_add_flag(g_countdown_label, LV_OBJ_FLAG_HIDDEN);
    
    // Контейнер ввода объема (скрыт по умолчанию)
    g_volume_input_container = lv_obj_create(screen);
    lv_obj_set_size(g_volume_input_container, 220, 80);
    lv_obj_align(g_volume_input_container, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_set_style_bg_color(g_volume_input_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_add_flag(g_volume_input_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *volume_label = lv_label_create(g_volume_input_container);
    lv_label_set_text(volume_label, "Введите реальный объем (мл):");
    lv_obj_set_style_text_color(volume_label, lv_color_white(), 0);
    lv_obj_align(volume_label, LV_ALIGN_TOP_MID, 0, 5);
    
    g_volume_input = lv_textarea_create(g_volume_input_container);
    lv_obj_set_size(g_volume_input, 100, 35);
    lv_textarea_set_one_line(g_volume_input, true);
    lv_textarea_set_max_length(g_volume_input, 6);
    lv_textarea_set_placeholder_text(g_volume_input, "10.5");
    lv_obj_align(g_volume_input, LV_ALIGN_TOP_LEFT, 10, 30);
    
    g_save_btn = lv_btn_create(g_volume_input_container);
    lv_obj_set_size(g_save_btn, 80, 35);
    lv_obj_align(g_save_btn, LV_ALIGN_TOP_RIGHT, -10, 30);
    lv_obj_set_style_bg_color(g_save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(g_save_btn, on_save_calibration_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *save_label = lv_label_create(g_save_btn);
    lv_label_set_text(save_label, "Сохр.");
    lv_obj_center(save_label);
    
    // Результат калибровки
    g_result_label = lv_label_create(screen);
    lv_label_set_text(g_result_label, "");
    lv_obj_set_style_text_color(g_result_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_result_label, &montserrat_ru, 0);
    lv_obj_align(g_result_label, LV_ALIGN_BOTTOM_MID, 0, -45);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // Инициализация первого насоса
    on_pump_selected(NULL);
    
    ESP_LOGI(TAG, "Экран калибровки создан");
    
    return screen;
}

