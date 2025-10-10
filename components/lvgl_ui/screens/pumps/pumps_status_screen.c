/**
 * @file pumps_status_screen.c
 * @brief Реализация экрана статуса насосов
 */

#include "pumps_status_screen.h"
#include "screen_manager/screen_manager.h"
#include "../base/screen_template.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "PUMPS_STATUS_SCREEN";

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
static lv_obj_t *g_pump_labels[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_status_labels[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_stats_labels[PUMP_INDEX_COUNT] = {NULL};

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки "Ручное управление"
 */
static void on_manual_control_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к ручному управлению");
    screen_show("pumps_manual", NULL);
}

/**
 * @brief Callback для кнопки "Калибровка"
 */
static void on_calibration_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к калибровке насосов");
    screen_show("pump_calibration", NULL);
}

/**
 * @brief Callback для клика на насос
 */
static void on_pump_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Клик на насос %d", pump_idx);
    
    // Переход к детальному экрану PID для этого насоса
    screen_show("pid_detail", (void*)(intptr_t)pump_idx);
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pumps_status_screen_create(void *context)
{
    ESP_LOGI(TAG, "Создание экрана статуса насосов");
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Статус насосов");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Статус насосов");
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Контейнер для списка насосов (прокручиваемый)
    lv_obj_t *list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, 220, 200);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_all(list_container, 5, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Создание элементов для каждого насоса
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // Контейнер для одного насоса
        lv_obj_t *pump_item = lv_obj_create(list_container);
        lv_obj_set_size(pump_item, 200, 50);
        lv_obj_set_style_bg_color(pump_item, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(pump_item, 1, 0);
        lv_obj_set_style_border_color(pump_item, lv_color_hex(0x555555), 0);
        lv_obj_set_style_radius(pump_item, 5, 0);
        lv_obj_clear_flag(pump_item, LV_OBJ_FLAG_SCROLLABLE);
        
        // Кликабельность
        lv_obj_add_flag(pump_item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(pump_item, on_pump_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Имя насоса
        lv_obj_t *name_label = lv_label_create(pump_item);
        lv_label_set_text(name_label, PUMP_NAMES[i]);
        lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(name_label, &montserrat_ru, 0);
        lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 5, 5);
        g_pump_labels[i] = name_label;
        
        // Статус (активен/остановлен)
        lv_obj_t *status_label = lv_label_create(pump_item);
        lv_label_set_text(status_label, "●");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x808080), 0); // Серый по умолчанию
        lv_obj_set_style_text_font(status_label, &montserrat_ru, 0);
        lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -5, 2);
        g_status_labels[i] = status_label;
        
        // Статистика
        lv_obj_t *stats_label = lv_label_create(pump_item);
        lv_label_set_text(stats_label, "Запусков: 0, Объем: 0 мл");
        lv_obj_set_style_text_color(stats_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(stats_label, &montserrat_ru, 0);
        lv_obj_align(stats_label, LV_ALIGN_BOTTOM_LEFT, 5, -2);
        g_stats_labels[i] = stats_label;
    }
    
    // Кнопка "Ручное управление"
    lv_obj_t *manual_btn = lv_btn_create(screen);
    lv_obj_set_size(manual_btn, 100, 35);
    lv_obj_align(manual_btn, LV_ALIGN_BOTTOM_LEFT, 10, -40);
    lv_obj_set_style_bg_color(manual_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_add_event_cb(manual_btn, on_manual_control_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *manual_label = lv_label_create(manual_btn);
    lv_label_set_text(manual_label, "Ручное");
    lv_obj_center(manual_label);
    
    // Кнопка "Калибровка"
    lv_obj_t *calib_btn = lv_btn_create(screen);
    lv_obj_set_size(calib_btn, 100, 35);
    lv_obj_align(calib_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -40);
    lv_obj_set_style_bg_color(calib_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(calib_btn, on_calibration_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *calib_label = lv_label_create(calib_btn);
    lv_label_set_text(calib_label, "Калибр.");
    lv_obj_center(calib_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Экран статуса насосов создан");
    
    // Начальное обновление
    pumps_status_screen_update_all();
    
    return screen;
}

/* =============================
 *  ОБНОВЛЕНИЕ ДАННЫХ
 * ============================= */

esp_err_t pumps_status_screen_update(pump_index_t pump_idx)
{
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_pump_labels[pump_idx] == NULL || g_stats_labels[pump_idx] == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Получение статистики насоса
    pump_stats_t stats;
    esp_err_t err = pump_manager_get_stats(pump_idx, &stats);
    
    if (err == ESP_OK) {
        // Обновление статистики
        char stats_text[64];
        snprintf(stats_text, sizeof(stats_text), 
                 "Запусков: %lu, Объем: %.1f мл",
                 (unsigned long)stats.total_runs,
                 stats.total_volume_ml);
        lv_label_set_text(g_stats_labels[pump_idx], stats_text);
        
        // Обновление статуса (зеленый = активен недавно, серый = остановлен)
        uint64_t now = esp_timer_get_time() / 1000ULL; // мс
        bool recently_active = (now - stats.last_run_time) < 5000; // активен если работал < 5 сек назад
        
        lv_obj_set_style_text_color(g_status_labels[pump_idx], 
                                     recently_active ? lv_color_hex(0x4CAF50) : lv_color_hex(0x808080), 
                                     0);
    }
    
    return ESP_OK;
}

esp_err_t pumps_status_screen_update_all(void)
{
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pumps_status_screen_update(i);
    }
    return ESP_OK;
}

