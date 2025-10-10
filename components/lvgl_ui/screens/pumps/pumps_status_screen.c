/**
 * @file pumps_status_screen.c
 * @brief Экран статуса всех 6 насосов
 */

#include "pumps_status_screen.h"
#include "screen_manager.h"
#include "pump_manager.h"
#include "system_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PUMPS_STATUS";

static lv_obj_t *status_labels[PUMP_INDEX_COUNT];
static lv_obj_t *stats_labels[PUMP_INDEX_COUNT];
static lv_timer_t *update_timer = NULL;

static const char *pump_names[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "WATER"
};

static const char *status_names[] = {
    "IDLE", "RUNNING", "COOLDOWN", "ERROR"
};

static const uint32_t status_colors[] = {
    0x00FF00, 0x0000FF, 0xFFFF00, 0xFF0000
};

/**
 * @brief Обновление данных
 */
static void update_pump_data(lv_timer_t *timer) {
    (void)timer;
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pump_stats_t stats;
        if (pump_manager_get_stats((pump_index_t)i, &stats) == ESP_OK) {
            // Обновление статуса
            lv_label_set_text(status_labels[i], status_names[stats.status]);
            lv_obj_set_style_text_color(status_labels[i], 
                lv_color_hex(status_colors[stats.status]), 0);
            
            // Обновление статистики
            char stats_text[64];
            snprintf(stats_text, sizeof(stats_text), 
                     "%.1f ml | %lu doses", 
                     stats.total_ml_dispensed,
                     (unsigned long)stats.total_doses);
            lv_label_set_text(stats_labels[i], stats_text);
        }
    }
}

/**
 * @brief Обработчик клика по насосу
 */
static void on_pump_click(lv_event_t *e) {
    int pump_idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "Pump %d clicked", pump_idx);
    
    // Переход на детальный экран насоса
    char param[16];
    snprintf(param, sizeof(param), "%d", pump_idx);
    screen_show("pump_detail", param);
}

/**
 * @brief Кнопка "назад"
 */
static void on_back_click(lv_event_t *e) {
    (void)e;
    screen_show("main", NULL);
}

/**
 * @brief Создание экрана
 */
static lv_obj_t* pumps_status_create(const char *param) {
    (void)param;
    
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Pumps Status");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Список насосов
    lv_obj_t *list = lv_obj_create(screen);
    lv_obj_set_size(list, 300, 180);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // Контейнер для насоса
        lv_obj_t *pump_cont = lv_btn_create(list);
        lv_obj_set_size(pump_cont, 280, 25);
        lv_obj_add_event_cb(pump_cont, on_pump_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_flex_flow(pump_cont, LV_FLEX_FLOW_ROW);
        
        // Имя
        lv_obj_t *name_label = lv_label_create(pump_cont);
        lv_label_set_text(name_label, pump_names[i]);
        lv_obj_set_width(name_label, 70);
        
        // Статус
        status_labels[i] = lv_label_create(pump_cont);
        lv_label_set_text(status_labels[i], "IDLE");
        lv_obj_set_width(status_labels[i], 80);
        
        // Статистика
        stats_labels[i] = lv_label_create(pump_cont);
        lv_label_set_text(stats_labels[i], "0.0 ml | 0");
        lv_obj_set_width(stats_labels[i], 110);
    }
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = lv_btn_create(screen);
    lv_obj_set_size(back_btn, 100, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    
    // Таймер обновления
    update_timer = lv_timer_create(update_pump_data, 1000, NULL);
    
    return screen;
}

/**
 * @brief Уничтожение экрана
 */
static void pumps_status_destroy(lv_obj_t *screen) {
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    lv_obj_del(screen);
}

/**
 * @brief Инициализация
 */
void pumps_status_screen_init(void) {
    screen_config_t config = {
        .name = "pumps_status",
        .create_cb = pumps_status_create,
        .destroy_cb = pumps_status_destroy,
        .show_cb = NULL,
        .hide_cb = NULL,
    };
    
    screen_register(&config);
    ESP_LOGI(TAG, "Pumps status screen registered");
}

