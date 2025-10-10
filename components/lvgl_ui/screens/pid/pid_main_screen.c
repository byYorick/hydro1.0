/**
 * @file pid_main_screen.c
 * @brief Главный экран PID - показывает все 6 контроллеров
 */

#include "pid_main_screen.h"
#include "screen_manager.h"
#include "pump_pid_manager.h"
#include "system_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_MAIN";

static lv_obj_t *pid_status_labels[6];
static lv_obj_t *pid_coeff_labels[6];
static lv_timer_t *update_timer = NULL;

static const char *pid_names[6] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "WATER"
};

/**
 * @brief Обновление данных PID
 */
static void update_pid_data(lv_timer_t *timer) {
    (void)timer;
    
    for (int i = 0; i < 6; i++) {
        pump_pid_instance_t *instance = pump_pid_get_instance((pump_pid_index_t)i);
        if (instance) {
            // Статус
            const char *mode = instance->auto_mode ? "AUTO" : "MANUAL";
            const char *enabled = instance->enabled ? "" : " [OFF]";
            char status[32];
            snprintf(status, sizeof(status), "[%s]%s", mode, enabled);
            lv_label_set_text(pid_status_labels[i], status);
            
            // Коэффициенты
            char coeffs[64];
            snprintf(coeffs, sizeof(coeffs), "P:%.1f I:%.2f D:%.2f",
                     instance->pid.config.kp,
                     instance->pid.config.ki,
                     instance->pid.config.kd);
            lv_label_set_text(pid_coeff_labels[i], coeffs);
        }
    }
}

/**
 * @brief Клик по PID
 */
static void on_pid_click(lv_event_t *e) {
    int pid_idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "PID %d clicked", pid_idx);
    
    // Переход на детальный экран
    char param[16];
    snprintf(param, sizeof(param), "%d", pid_idx);
    screen_show("pid_detail", param);
}

/**
 * @brief Кнопка назад
 */
static void on_back_click(lv_event_t *e) {
    (void)e;
    screen_show("main", NULL);
}

/**
 * @brief Создание экрана
 */
static lv_obj_t* pid_main_create(const char *param) {
    (void)param;
    
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "PID Controllers");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Список PID
    lv_obj_t *list = lv_obj_create(screen);
    lv_obj_set_size(list, 300, 180);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    
    for (int i = 0; i < 6; i++) {
        // Контейнер PID
        lv_obj_t *pid_cont = lv_btn_create(list);
        lv_obj_set_size(pid_cont, 280, 25);
        lv_obj_add_event_cb(pid_cont, on_pid_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_flex_flow(pid_cont, LV_FLEX_FLOW_ROW);
        
        // Имя
        lv_obj_t *name = lv_label_create(pid_cont);
        lv_label_set_text(name, pid_names[i]);
        lv_obj_set_width(name, 70);
        
        // Статус
        pid_status_labels[i] = lv_label_create(pid_cont);
        lv_label_set_text(pid_status_labels[i], "[AUTO]");
        lv_obj_set_width(pid_status_labels[i], 70);
        
        // Коэффициенты
        pid_coeff_labels[i] = lv_label_create(pid_cont);
        lv_label_set_text(pid_coeff_labels[i], "P:2.0 I:0.5");
        lv_obj_set_width(pid_coeff_labels[i], 120);
    }
    
    // Кнопка назад
    lv_obj_t *back_btn = lv_btn_create(screen);
    lv_obj_set_size(back_btn, 100, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    
    // Таймер обновления
    update_timer = lv_timer_create(update_pid_data, 1000, NULL);
    
    return screen;
}

/**
 * @brief Уничтожение экрана
 */
static void pid_main_destroy(lv_obj_t *screen) {
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    lv_obj_del(screen);
}

/**
 * @brief Инициализация
 */
void pid_main_screen_init(void) {
    screen_config_t config = {
        .name = "pid_main",
        .create_cb = pid_main_create,
        .destroy_cb = pid_main_destroy,
        .show_cb = NULL,
        .hide_cb = NULL,
    };
    
    screen_register(&config);
    ESP_LOGI(TAG, "PID main screen registered");
}

