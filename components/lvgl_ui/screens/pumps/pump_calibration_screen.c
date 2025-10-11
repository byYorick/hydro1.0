/**
 * @file pump_calibration_screen.c
 * @brief Реализация экрана калибровки насосов с управлением энкодером
 */

#include "pump_calibration_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/encoder_value_edit.h"
#include "../../widgets/event_helpers.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "data_logger.h"
#include "ph_ec_controller.h"
#include "system_config.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PUMP_CALIB_SCREEN";

// UI элементы для каждого насоса
typedef struct {
    lv_obj_t *container;           // Контейнер виджета
    lv_obj_t *name_label;          // Название насоса
    lv_obj_t *rate_label;          // Текущий расход
    lv_obj_t *time_value;          // Виджет времени (редактируемый)
    lv_obj_t *volume_value;        // Виджет объема (редактируемый)
    lv_obj_t *calib_btn;           // Кнопка калибровки
    lv_obj_t *status_label;        // Статус калибровки
    lv_obj_t *save_btn;            // Кнопка сохранения
    
    float old_flow_rate;           // Старый расход
    bool is_calibrating;           // Флаг калибровки
    lv_timer_t *countdown_timer;   // Таймер обратного отсчета
    int countdown_remaining;       // Оставшееся время
} pump_calib_widget_t;

// Массив виджетов для всех насосов
static pump_calib_widget_t g_pump_widgets[PUMP_INDEX_COUNT] = {0};
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_scroll_container = NULL;


/* =============================
 *  ТАЙМЕР ОБРАТНОГО ОТСЧЕТА
 * ============================= */

static void countdown_timer_cb(lv_timer_t *timer)
{
    pump_calib_widget_t *widget = (pump_calib_widget_t *)lv_timer_get_user_data(timer);
    if (!widget) return;
    
    widget->countdown_remaining -= 100;
    
    if (widget->countdown_remaining <= 0) {
        widget->is_calibrating = false;
        lv_label_set_text(widget->status_label, "Введите объем:");
        lv_obj_set_style_text_color(widget->status_label, lv_color_hex(0x4CAF50), 0);
        
        lv_obj_clear_flag(widget->volume_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget->save_btn, LV_OBJ_FLAG_HIDDEN);
        
        if (widget->countdown_timer) {
            lv_timer_del(widget->countdown_timer);
            widget->countdown_timer = NULL;
        }
        
        ESP_LOGI(TAG, "Калибровка завершена для %s", PUMP_NAMES[(int)(widget - g_pump_widgets)]);
    } else {
        char text[32];
        snprintf(text, sizeof(text), "%.1f сек", widget->countdown_remaining / 1000.0f);
        lv_label_set_text(widget->status_label, text);
    }
}


/* =============================
 *  CALLBACKS
 * ============================= */

static void on_calibrate_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    pump_calib_widget_t *widget = &g_pump_widgets[pump_idx];
    
    // Получаем время из виджета
    uint32_t time_ms = (uint32_t)widget_encoder_value_get(widget->time_value);
    
    ESP_LOGI(TAG, "Запуск калибровки %s на %lu мс", 
             PUMP_NAMES[pump_idx], (unsigned long)time_ms);
    
    widget->is_calibrating = true;
    widget->countdown_remaining = time_ms;
    
    lv_obj_add_flag(widget->volume_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget->save_btn, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_set_style_text_color(widget->status_label, lv_color_hex(0xFFC107), 0);
    lv_label_set_text(widget->status_label, "Запуск...");
    
    pump_manager_run_direct(pump_idx, time_ms);
    
    widget->countdown_timer = lv_timer_create(countdown_timer_cb, 100, widget);
}

static void on_save_calibration_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    pump_calib_widget_t *widget = &g_pump_widgets[pump_idx];
    
    // Получаем значения из виджетов
    float volume_ml = widget_encoder_value_get(widget->volume_value);
    uint32_t time_ms = (uint32_t)widget_encoder_value_get(widget->time_value);
    
    if (volume_ml <= 0 || volume_ml > 999) {
        ESP_LOGW(TAG, "Неверный объем: %.2f", volume_ml);
        lv_label_set_text(widget->status_label, "Ошибка объема!");
        lv_obj_set_style_text_color(widget->status_label, lv_color_hex(0xF44336), 0);
        return;
    }
    
    float new_flow_rate = volume_ml / (time_ms / 1000.0f);
    
    ESP_LOGI(TAG, "Калибровка %s: старый=%.3f, новый=%.3f мл/сек",
             PUMP_NAMES[pump_idx], widget->old_flow_rate, new_flow_rate);
    
    system_config_t config;
    esp_err_t err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_config[pump_idx].flow_rate_ml_per_sec = new_flow_rate;
        err = config_save(&config);
        
        if (err == ESP_OK) {
            ph_ec_controller_apply_config(&config);
            data_logger_log_pump_calibration(pump_idx, widget->old_flow_rate, new_flow_rate);
            
            widget->old_flow_rate = new_flow_rate;
            char text[32];
            snprintf(text, sizeof(text), "%.3f мл/с", new_flow_rate);
            lv_label_set_text(widget->rate_label, text);
            
            lv_label_set_text(widget->status_label, "Сохранено!");
            lv_obj_set_style_text_color(widget->status_label, lv_color_hex(0x4CAF50), 0);
            
            lv_obj_add_flag(widget->volume_value, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget->save_btn, LV_OBJ_FLAG_HIDDEN);
            
            ESP_LOGI(TAG, "Калибровка %s сохранена успешно", PUMP_NAMES[pump_idx]);
        }
    }
}

/* =============================
 *  СОЗДАНИЕ ВИДЖЕТА НАСОСА
 * ============================= */

static void create_pump_widget(lv_obj_t *parent, pump_index_t pump_idx)
{
    // КРИТИЧНО: Feed watchdog в начале создания виджета (тяжелая операция)
    esp_task_wdt_reset();
    
    pump_calib_widget_t *widget = &g_pump_widgets[pump_idx];
    
    const system_config_t *config = config_manager_get_cached();
    if (config) {
        widget->old_flow_rate = config->pump_config[pump_idx].flow_rate_ml_per_sec;
    }
    
    // ОПТИМИЗАЦИЯ: Используем готовый стиль вместо 5 вызовов set_style
    extern lv_style_t style_pump_widget;
    
    widget->container = lv_obj_create(parent);
    if (!widget->container) {
        ESP_LOGE(TAG, "Failed to create container for pump %d", pump_idx);
        return;
    }
    lv_obj_set_size(widget->container, 220, 110);
    lv_obj_add_style(widget->container, &style_pump_widget, 0);  // Один вызов вместо 5!
    lv_obj_clear_flag(widget->container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Название
    widget->name_label = lv_label_create(widget->container);
    if (widget->name_label) {
        lv_label_set_text(widget->name_label, PUMP_NAMES[pump_idx]);
        lv_obj_set_style_text_color(widget->name_label, lv_color_hex(0x2196F3), 0);
        lv_obj_align(widget->name_label, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    
    // Текущий расход
    widget->rate_label = lv_label_create(widget->container);
    if (widget->rate_label) {
        char rate_text[32];
        snprintf(rate_text, sizeof(rate_text), "%.3f мл/с", widget->old_flow_rate);
        lv_label_set_text(widget->rate_label, rate_text);
        lv_obj_set_style_text_color(widget->rate_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_align(widget->rate_label, LV_ALIGN_TOP_RIGHT, 0, 2);
    }
    
    // КРИТИЧНО: Feed watchdog перед созданием сложного encoder_value виджета
    esp_task_wdt_reset();
    
    // Виджет времени (редактируемое энкодером)
    encoder_value_config_t time_cfg = {
        .min_value = 1000,
        .max_value = 30000,
        .step = 100,
        .initial_value = 10000,
        .decimals = 0,
        .unit = "мс",
        .edit_color = lv_color_hex(0xFFC107),  // Желтый при редактировании
    };
    widget->time_value = widget_encoder_value_create(widget->container, &time_cfg);
    if (widget->time_value) {
        lv_obj_set_size(widget->time_value, 90, 28);
        lv_obj_align(widget->time_value, LV_ALIGN_TOP_LEFT, 0, 22);
    } else {
        ESP_LOGW(TAG, "Failed to create time_value widget for pump %d", pump_idx);
    }
    
    // Кнопка калибровки
    widget->calib_btn = lv_btn_create(widget->container);
    if (widget->calib_btn) {
        lv_obj_set_size(widget->calib_btn, 60, 28);
        lv_obj_set_style_bg_color(widget->calib_btn, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_radius(widget->calib_btn, 4, 0);
        lv_obj_align(widget->calib_btn, LV_ALIGN_TOP_RIGHT, 0, 22);
        widget_add_click_handler(widget->calib_btn, on_calibrate_click, (void*)(intptr_t)pump_idx);
        
        lv_obj_t *calib_label = lv_label_create(widget->calib_btn);
        if (calib_label) {
            lv_label_set_text(calib_label, "Калиб");
            lv_obj_center(calib_label);
        }
    } else {
        ESP_LOGE(TAG, "Failed to create calib_btn for pump %d", pump_idx);
    }
    
    // Статус
    widget->status_label = lv_label_create(widget->container);
    if (widget->status_label) {
        lv_label_set_text(widget->status_label, "Готов");
        lv_obj_set_style_text_color(widget->status_label, lv_color_hex(0x888888), 0);
        lv_obj_align(widget->status_label, LV_ALIGN_TOP_LEFT, 0, 55);
    }
    
    // КРИТИЧНО: Feed watchdog перед созданием второго encoder_value виджета
    esp_task_wdt_reset();
    
    // Виджет объема (редактируемый энкодером, скрыт)
    encoder_value_config_t volume_cfg = {
        .min_value = 0.1f,
        .max_value = 999.0f,
        .step = 0.1f,
        .initial_value = 10.0f,
        .decimals = 1,
        .unit = "мл",
        .edit_color = lv_color_hex(0x4CAF50),  // Зеленый при редактировании
    };
    widget->volume_value = widget_encoder_value_create(widget->container, &volume_cfg);
    if (widget->volume_value) {
        lv_obj_set_size(widget->volume_value, 90, 28);
        lv_obj_align(widget->volume_value, LV_ALIGN_TOP_LEFT, 0, 77);
        lv_obj_add_flag(widget->volume_value, LV_OBJ_FLAG_HIDDEN);
    } else {
        ESP_LOGW(TAG, "Failed to create volume_value widget for pump %d", pump_idx);
    }
    
    // Кнопка сохранения (скрыта)
    widget->save_btn = lv_btn_create(widget->container);
    if (widget->save_btn) {
        lv_obj_set_size(widget->save_btn, 70, 28);
        lv_obj_set_style_bg_color(widget->save_btn, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_radius(widget->save_btn, 4, 0);
        lv_obj_align(widget->save_btn, LV_ALIGN_TOP_RIGHT, 0, 77);
        widget_add_click_handler(widget->save_btn, on_save_calibration_click, (void*)(intptr_t)pump_idx);
        lv_obj_add_flag(widget->save_btn, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_t *save_label = lv_label_create(widget->save_btn);
        if (save_label) {
            lv_label_set_text(save_label, "Сохр");
            lv_obj_center(save_label);
        }
    } else {
        ESP_LOGW(TAG, "Failed to create save_btn for pump %d", pump_idx);
    }
    
    // КРИТИЧНО: Feed watchdog в конце создания виджета
    esp_task_wdt_reset();
}

/* =============================
 *  СОЗДАНИЕ ЭКРАНА
 * ============================= */

lv_obj_t* pump_calibration_screen_create(void *context)
{
    ESP_LOGD(TAG, "Создание экрана калибровки");
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Заголовок
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 32);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Калибровка насосов");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    
    lv_obj_t *back_btn = widget_create_back_button(header, NULL, NULL);
    lv_obj_set_size(back_btn, 40, 24);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -2, 0);
    
    // Скроллируемый контейнер
    g_scroll_container = lv_obj_create(screen);
    lv_obj_set_size(g_scroll_container, LV_PCT(100), 280);
    lv_obj_align(g_scroll_container, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(g_scroll_container, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(g_scroll_container, 0, 0);
    lv_obj_set_style_pad_all(g_scroll_container, 8, 0);
    lv_obj_set_flex_flow(g_scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(g_scroll_container, 8, 0);
    
    // Создать виджеты для всех 6 насосов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // КРИТИЧНО: Feed watchdog при создании каждого виджета (тяжелая операция)
        esp_task_wdt_reset();
        
        create_pump_widget(g_scroll_container, (pump_index_t)i);
    }
    
    // КРИТИЧНО: Feed watchdog после создания всех виджетов
    esp_task_wdt_reset();
    
    ESP_LOGD(TAG, "Экран калибровки создан с %d виджетами насосов", PUMP_INDEX_COUNT);
    
    return screen;
}
