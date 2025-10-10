/**
 * @file pid_main_screen.c
 * @brief Реализация главного экрана PID контроллеров
 */

#include "pid_main_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_MAIN_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для клика на элемент PID
 */
static void on_pid_item_click(lv_event_t *e)
{
    pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Переход к детальному экрану PID для насоса %d", pump_idx);
    
    screen_show("pid_detail", (void*)(intptr_t)pump_idx);
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_main_screen_create(void *context)
{
    ESP_LOGI(TAG, "Создание главного экрана PID");
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "PID");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "PID Контроллеры");
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Получение конфигурации
    const system_config_t *config = config_manager_get_cached();
    
    // Контейнер для списка PID
    lv_obj_t *list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, 220, 230);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_all(list_container, 5, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Создание элементов для каждого PID
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // Контейнер для одного PID
        lv_obj_t *pid_item = lv_obj_create(list_container);
        lv_obj_set_size(pid_item, 200, 55);
        lv_obj_set_style_bg_color(pid_item, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(pid_item, 1, 0);
        lv_obj_set_style_border_color(pid_item, lv_color_hex(0x555555), 0);
        lv_obj_set_style_radius(pid_item, 5, 0);
        lv_obj_clear_flag(pid_item, LV_OBJ_FLAG_SCROLLABLE);
        
        // Кликабельность
        lv_obj_add_flag(pid_item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(pid_item, on_pid_item_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Имя насоса
        lv_obj_t *name_label = lv_label_create(pid_item);
        lv_label_set_text(name_label, PUMP_NAMES[i]);
        lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(name_label, &montserrat_ru, 0);
        lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 5, 5);
        
        // Статус (enabled/disabled)
        bool enabled = config ? config->pump_pid[i].enabled : false;
        lv_obj_t *status_label = lv_label_create(pid_item);
        lv_label_set_text(status_label, enabled ? "ON" : "OFF");
        lv_obj_set_style_text_color(status_label, 
                                     enabled ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336), 
                                     0);
        lv_obj_set_style_text_font(status_label, &montserrat_ru, 0);
        lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -5, 5);
        
        // Параметры PID
        char params[64];
        if (config) {
            snprintf(params, sizeof(params), 
                     "Kp=%.2f Ki=%.2f Kd=%.2f",
                     config->pump_pid[i].kp,
                     config->pump_pid[i].ki,
                     config->pump_pid[i].kd);
        } else {
            snprintf(params, sizeof(params), "Kp=- Ki=- Kd=-");
        }
        
        lv_obj_t *params_label = lv_label_create(pid_item);
        lv_label_set_text(params_label, params);
        lv_obj_set_style_text_color(params_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(params_label, &montserrat_ru, 0);
        lv_obj_align(params_label, LV_ALIGN_BOTTOM_LEFT, 5, -2);
    }
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Главный экран PID создан");
    
    return screen;
}

esp_err_t pid_main_screen_update(void)
{
    // TODO: Обновление данных на экране
    return ESP_OK;
}

