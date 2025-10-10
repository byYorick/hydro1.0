/**
 * @file pumps_manual_screen.c
 * @brief Реализация экрана ручного управления насосами
 */

#include "pumps_manual_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "PUMPS_MANUAL_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP",
    "pH DOWN",
    "EC A",
    "EC B",
    "EC C",
    "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_duration_inputs[PUMP_INDEX_COUNT] = {NULL};

/* =============================
 *  POPUP ПОДТВЕРЖДЕНИЯ
 * ============================= */

/**
 * @brief Показать информацию и запустить насос
 */
static void show_pump_confirmation(pump_index_t pump_idx, uint32_t duration_ms)
{
    // Получение статистики для отображения
    pump_stats_t stats;
    pump_manager_get_stats(pump_idx, &stats);
    
    // Получение суточного объема
    float daily_volume = 0;
    pump_manager_get_daily_volume(pump_idx, &daily_volume);
    
    // Получение конфигурации для лимитов
    const system_config_t *config = config_manager_get_cached();
    uint32_t max_daily = config ? config->pump_pid[pump_idx].max_daily_volume : 0;
    
    // Расчет cooldown
    uint64_t now_ms = esp_timer_get_time() / 1000ULL;
    uint64_t elapsed_ms = now_ms - stats.last_run_time;
    uint32_t cooldown_ms = config ? config->pump_pid[pump_idx].cooldown_time_ms : 0;
    int cooldown_remaining = (int)((cooldown_ms - elapsed_ms) / 1000);
    if (cooldown_remaining < 0) cooldown_remaining = 0;
    
    ESP_LOGI(TAG, "Запуск насоса %s: cooldown=%d сек, суточный=%.1f/%.lu мл",
             PUMP_NAMES[pump_idx], cooldown_remaining, daily_volume, (unsigned long)max_daily);
    
    // Прямой запуск насоса
    pump_manager_run_direct(pump_idx, duration_ms);
}

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки старта насоса
 */
static void on_pump_start_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    
    // Получить длительность из поля ввода (по умолчанию 5000 мс)
    uint32_t duration_ms = 5000;
    if (g_duration_inputs[pump_idx] != NULL) {
        const char *text = lv_textarea_get_text(g_duration_inputs[pump_idx]);
        if (text != NULL && text[0] != '\0') {
            duration_ms = (uint32_t)atoi(text);
            if (duration_ms < 100) duration_ms = 100;
            if (duration_ms > 30000) duration_ms = 30000;
        }
    }
    
    ESP_LOGI(TAG, "Запрос запуска насоса %s на %lu мс", 
             PUMP_NAMES[pump_idx], (unsigned long)duration_ms);
    
    // Показать popup подтверждения
    show_pump_confirmation(pump_idx, duration_ms);
}

/**
 * @brief Callback для кнопки "Стоп все"
 */
static void on_stop_all_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Остановка всех насосов");
    
    // TODO: Реализовать остановку всех насосов
    // Пока просто логируем
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pumps_manual_screen_create(void *context)
{
    ESP_LOGI(TAG, "Создание экрана ручного управления");
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Ручное управление");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Ручное управление");
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
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
        
        // Поле ввода времени (мс)
        lv_obj_t *duration_input = lv_textarea_create(pump_item);
        lv_obj_set_size(duration_input, 60, 30);
        lv_textarea_set_text(duration_input, "5000");
        lv_textarea_set_one_line(duration_input, true);
        lv_textarea_set_max_length(duration_input, 5);
        g_duration_inputs[i] = duration_input;
        
        // Кнопка "Старт"
        lv_obj_t *start_btn = lv_btn_create(pump_item);
        lv_obj_set_size(start_btn, 60, 30);
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x4CAF50), 0);
        lv_obj_add_event_cb(start_btn, on_pump_start_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(start_btn, on_pump_start_click, LV_EVENT_PRESSED, (void*)(intptr_t)i);
        
        lv_obj_t *start_label = lv_label_create(start_btn);
        lv_label_set_text(start_label, "Старт");
        lv_obj_center(start_label);
    }
    
    // Кнопка "Стоп все"
    lv_obj_t *stop_all_btn = lv_btn_create(screen);
    lv_obj_set_size(stop_all_btn, 200, 35);
    lv_obj_align(stop_all_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(stop_all_btn, lv_color_hex(0xF44336), 0);
    lv_obj_add_event_cb(stop_all_btn, on_stop_all_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stop_all_btn, on_stop_all_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *stop_all_label = lv_label_create(stop_all_btn);
    lv_label_set_text(stop_all_label, "СТОП ВСЕ");
    lv_obj_center(stop_all_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Экран ручного управления создан");
    
    return screen;
}

