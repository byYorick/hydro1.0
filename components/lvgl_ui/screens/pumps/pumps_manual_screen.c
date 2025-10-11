/**
 * @file pumps_manual_screen.c
 * @brief Реализация экрана ручного управления насосами с асинхронной работой
 */

#include "pumps_manual_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/encoder_value_edit.h"
#include "../../widgets/event_helpers.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "peristaltic_pump.h"
#include "system_config.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdio.h>

static const char *TAG = "PUMPS_MANUAL_SCREEN";

// GPIO пины насосов
static const int PUMP_PINS[PUMP_INDEX_COUNT] = {
    PUMP_PH_UP_PIN,
    PUMP_PH_DOWN_PIN,
    PUMP_EC_A_PIN,
    PUMP_EC_B_PIN,
    PUMP_EC_C_PIN,
    PUMP_WATER_PIN
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_duration_inputs[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_start_buttons[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_button_labels[PUMP_INDEX_COUNT] = {NULL};

// Состояние насосов
static bool g_pump_running[PUMP_INDEX_COUNT] = {false};
static lv_timer_t *g_pump_timers[PUMP_INDEX_COUNT] = {NULL};

/* =============================
 *  УПРАВЛЕНИЕ НАСОСАМИ
 * ============================= */

/**
 * @brief Callback таймера - останавливает насос по истечении времени
 */
static void pump_timer_callback(lv_timer_t *timer)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_timer_get_user_data(timer);
    
    // Остановить насос
    pump_stop(PUMP_PINS[pump_idx]);
    g_pump_running[pump_idx] = false;
    
    // Обновить кнопку
    if (g_button_labels[pump_idx]) {
        lv_label_set_text(g_button_labels[pump_idx], "Старт");
    }
    if (g_start_buttons[pump_idx]) {
        lv_obj_set_style_bg_color(g_start_buttons[pump_idx], lv_color_hex(0x4CAF50), 0);
    }
    
    // Удалить таймер
    lv_timer_del(timer);
    g_pump_timers[pump_idx] = NULL;
    
    ESP_LOGI(TAG, "Насос %s остановлен автоматически", PUMP_NAMES[pump_idx]);
}

/**
 * @brief Запустить насос асинхронно
 */
static void start_pump_async(pump_index_t pump_idx, uint32_t duration_ms)
{
    // Включить насос
    pump_start(PUMP_PINS[pump_idx]);
    g_pump_running[pump_idx] = true;
    
    // Обновить кнопку
    if (g_button_labels[pump_idx]) {
        lv_label_set_text(g_button_labels[pump_idx], "Стоп");
    }
    if (g_start_buttons[pump_idx]) {
        lv_obj_set_style_bg_color(g_start_buttons[pump_idx], lv_color_hex(0xF44336), 0);
    }
    
    // Создать таймер для автоматической остановки
    g_pump_timers[pump_idx] = lv_timer_create(pump_timer_callback, duration_ms, (void*)(intptr_t)pump_idx);
    lv_timer_set_repeat_count(g_pump_timers[pump_idx], 1);  // Однократный
    
    ESP_LOGI(TAG, "Насос %s запущен на %lu мс", PUMP_NAMES[pump_idx], (unsigned long)duration_ms);
}

/**
 * @brief Остановить насос немедленно
 */
static void stop_pump_immediately(pump_index_t pump_idx)
{
    // Остановить насос
    pump_stop(PUMP_PINS[pump_idx]);
    g_pump_running[pump_idx] = false;
    
    // Обновить кнопку
    if (g_button_labels[pump_idx]) {
        lv_label_set_text(g_button_labels[pump_idx], "Старт");
    }
    if (g_start_buttons[pump_idx]) {
        lv_obj_set_style_bg_color(g_start_buttons[pump_idx], lv_color_hex(0x4CAF50), 0);
    }
    
    // Удалить таймер если есть
    if (g_pump_timers[pump_idx]) {
        lv_timer_del(g_pump_timers[pump_idx]);
        g_pump_timers[pump_idx] = NULL;
    }
    
    ESP_LOGI(TAG, "Насос %s остановлен вручную", PUMP_NAMES[pump_idx]);
}

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки старт/стоп насоса
 */
static void on_pump_toggle_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    
    if (g_pump_running[pump_idx]) {
        // Насос работает - остановить
        stop_pump_immediately(pump_idx);
    } else {
        // Насос остановлен - запустить
        uint32_t duration_ms = 5000;
        if (g_duration_inputs[pump_idx] != NULL) {
            duration_ms = (uint32_t)widget_encoder_value_get(g_duration_inputs[pump_idx]);
        }
        
        start_pump_async(pump_idx, duration_ms);
    }
}

/**
 * @brief Callback для кнопки "Стоп все"
 */
static void on_stop_all_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Остановка всех насосов");
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        if (g_pump_running[i]) {
            stop_pump_immediately((pump_index_t)i);
        }
    }
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pumps_manual_screen_create(void *context)
{
    ESP_LOGD(TAG, "Создание экрана ручного управления");
    
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create pumps manual screen");
        return NULL;
    }
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Ручное управление");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Ручное управление");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Контейнер для списка насосов
    lv_obj_t *list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, 220, 180);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_all(list_container, 5, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Создание элементов для каждого насоса
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // Контейнер для одного насоса
        lv_obj_t *pump_item = lv_obj_create(list_container);
        lv_obj_set_size(pump_item, 200, 40);
        lv_obj_set_style_bg_color(pump_item, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(pump_item, 0, 0);
        lv_obj_set_style_pad_all(pump_item, 2, 0);
        lv_obj_clear_flag(pump_item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(pump_item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pump_item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Имя насоса
        lv_obj_t *name_label = lv_label_create(pump_item);
        lv_label_set_text(name_label, PUMP_NAMES[i]);
        lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
        lv_obj_set_width(name_label, 60);
        
        // Виджет времени (редактируемый энкодером)
        encoder_value_config_t duration_cfg = {
            .min_value = 100,
            .max_value = 30000,
            .step = 100,
            .initial_value = 5000,
            .decimals = 0,
            .unit = "мс",
            .edit_color = lv_color_hex(0xFFC107),
        };
        lv_obj_t *duration_widget = widget_encoder_value_create(pump_item, &duration_cfg);
        lv_obj_set_size(duration_widget, 60, 30);
        g_duration_inputs[i] = duration_widget;
        
        // Кнопка "Старт/Стоп"
        lv_obj_t *toggle_btn = lv_btn_create(pump_item);
        lv_obj_set_size(toggle_btn, 60, 30);
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x4CAF50), 0);
        widget_add_click_handler(toggle_btn, on_pump_toggle_click, (void*)(intptr_t)i);
        g_start_buttons[i] = toggle_btn;
        
        lv_obj_t *btn_label = lv_label_create(toggle_btn);
        lv_label_set_text(btn_label, "Старт");
        lv_obj_center(btn_label);
        g_button_labels[i] = btn_label;
    }
    
    // Кнопка "Стоп все"
    lv_obj_t *stop_all_btn = lv_btn_create(screen);
    lv_obj_set_size(stop_all_btn, 200, 35);
    lv_obj_align(stop_all_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(stop_all_btn, lv_color_hex(0xF44336), 0);
    widget_add_click_handler(stop_all_btn, on_stop_all_click, NULL);
    
    lv_obj_t *stop_all_label = lv_label_create(stop_all_btn);
    lv_label_set_text(stop_all_label, "СТОП ВСЕ");
    lv_obj_center(stop_all_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGD(TAG, "Экран ручного управления создан");
    
    return screen;
}
