/**
 * @file pid_graph_screen.c
 * @brief Реализация экрана графика PID
 */

#include "pid_graph_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_GRAPH_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

static lv_obj_t *g_screen = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

// Переменные для будущей реализации графика
static lv_obj_t *g_chart __attribute__((unused)) = NULL;
static lv_timer_t *g_update_timer __attribute__((unused)) = NULL;

// История графика (60 точек) - для будущей реализации
#define GRAPH_HISTORY_SIZE 60
static float g_history_values[GRAPH_HISTORY_SIZE] __attribute__((unused)) = {0};
static float g_history_setpoints[GRAPH_HISTORY_SIZE] __attribute__((unused)) = {0};
static uint8_t g_history_index __attribute__((unused)) = 0;

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки "Экспорт" (заглушка)
 */
static void on_export_click(lv_event_t *e)
{
    (void)e; // unused
    ESP_LOGI(TAG, "Экспорт графика (заглушка на потом)");
    // TODO: Реализовать экспорт через Telegram bot
}

/**
 * @brief Таймер обновления графика
 */
static void update_graph_timer_cb(lv_timer_t *timer) __attribute__((unused));
static void update_graph_timer_cb(lv_timer_t *timer)
{
    (void)timer; // unused
    // TODO: Получить реальные значения датчиков и обновить график
    // Пока просто заглушка
    ESP_LOGD(TAG, "Обновление графика");
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_graph_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGI(TAG, "Создание экрана графика для насоса %d", g_pump_idx);
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "График PID");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[48];
    snprintf(title_text, sizeof(title_text), "График: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // График (заглушка - будет реализован позже)
    lv_obj_t *chart_container = lv_obj_create(screen);
    lv_obj_set_size(chart_container, 220, 180);
    lv_obj_align(chart_container, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_set_style_bg_color(chart_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(chart_container, 1, 0);
    lv_obj_set_style_border_color(chart_container, lv_color_hex(0x444444), 0);
    
    lv_obj_t *chart_placeholder = lv_label_create(chart_container);
    lv_label_set_text(chart_placeholder, "График PID\n(в разработке)");
    lv_obj_set_style_text_color(chart_placeholder, lv_color_hex(0x888888), 0);
    lv_obj_center(chart_placeholder);
    
    // Кнопка "Экспорт" (заглушка)
    lv_obj_t *export_btn = lv_btn_create(screen);
    lv_obj_set_size(export_btn, 200, 35);
    lv_obj_align(export_btn, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_set_style_bg_color(export_btn, lv_color_hex(0x00BCD4), 0);
    lv_obj_add_event_cb(export_btn, on_export_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(export_btn, on_export_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *export_label = lv_label_create(export_btn);
    lv_label_set_text(export_label, "Экспорт (заглушка)");
    lv_obj_center(export_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // TODO: Запустить таймер обновления
    // g_update_timer = lv_timer_create(update_graph_timer_cb, 2000, NULL);
    
    ESP_LOGI(TAG, "Экран графика создан (заглушка)");
    
    return screen;
}

