/**
 * @file pid_detail_screen.c
 * @brief Детальный экран PID с графиком и настройками
 */

#include "pid_detail_screen.h"
#include "screen_manager.h"
#include "pump_pid_manager.h"
#include "system_config.h"
#include "config_manager.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PID_DETAIL";

static int current_pid_idx = 0;
static lv_obj_t *chart = NULL;
static lv_chart_series_t *ser_error = NULL;
static lv_chart_series_t *ser_output = NULL;
static lv_obj_t *label_p = NULL;
static lv_obj_t *label_i = NULL;
static lv_obj_t *label_d = NULL;
static lv_obj_t *label_output = NULL;
static lv_obj_t *spinbox_kp = NULL;
static lv_obj_t *spinbox_ki = NULL;
static lv_obj_t *spinbox_kd = NULL;
static lv_timer_t *update_timer = NULL;

static const char *pid_names[] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "WATER"
};

/**
 * @brief Обновление графика
 */
static void update_chart(lv_timer_t *timer) {
    (void)timer;
    
    pump_pid_instance_t *instance = pump_pid_get_instance((pump_pid_index_t)current_pid_idx);
    if (!instance) return;
    
    pid_output_t output = pid_get_last_output(&instance->pid);
    
    // Добавление точек на график
    lv_chart_set_next_value(chart, ser_error, (int32_t)(output.error * 10));
    lv_chart_set_next_value(chart, ser_output, (int32_t)(output.output));
    
    // Обновление вкладов
    char text[32];
    snprintf(text, sizeof(text), "P: %.2f", output.p_term);
    lv_label_set_text(label_p, text);
    
    snprintf(text, sizeof(text), "I: %.2f", output.i_term);
    lv_label_set_text(label_i, text);
    
    snprintf(text, sizeof(text), "D: %.2f", output.d_term);
    lv_label_set_text(label_d, text);
    
    snprintf(text, sizeof(text), "Out: %.2f ml", output.output);
    lv_label_set_text(label_output, text);
}

/**
 * @brief Сохранение коэффициентов
 */
static void on_save_click(lv_event_t *e) {
    (void)e;
    
    // Получение значений из spinbox
    float kp = lv_spinbox_get_value(spinbox_kp) / 10.0f;
    float ki = lv_spinbox_get_value(spinbox_ki) / 100.0f;
    float kd = lv_spinbox_get_value(spinbox_kd) / 100.0f;
    
    // Применение к PID
    pump_pid_set_tunings((pump_pid_index_t)current_pid_idx, kp, ki, kd);
    
    // Сохранение в NVS
    system_config_t sys_config;
    if (config_load(&sys_config) == ESP_OK) {
        sys_config.pump_pid[current_pid_idx].kp = kp;
        sys_config.pump_pid[current_pid_idx].ki = ki;
        sys_config.pump_pid[current_pid_idx].kd = kd;
        config_save(&sys_config);
        
        ESP_LOGI(TAG, "PID %s saved: Kp=%.2f Ki=%.3f Kd=%.3f",
                 pid_names[current_pid_idx], kp, ki, kd);
    }
}

/**
 * @brief Сброс интеграла
 */
static void on_reset_click(lv_event_t *e) {
    (void)e;
    pump_pid_reset((pump_pid_index_t)current_pid_idx);
    ESP_LOGI(TAG, "PID %s integral reset", pid_names[current_pid_idx]);
}

/**
 * @brief Кнопка назад
 */
static void on_back_click(lv_event_t *e) {
    (void)e;
    screen_show("pid_main", NULL);
}

/**
 * @brief Инкремент spinbox
 */
static void spinbox_increment(lv_event_t *e) {
    lv_obj_t *spinbox = lv_event_get_target(e);
    lv_spinbox_increment(spinbox);
}

/**
 * @brief Декремент spinbox
 */
static void spinbox_decrement(lv_event_t *e) {
    lv_obj_t *spinbox = lv_event_get_target(e);
    lv_spinbox_decrement(spinbox);
}

/**
 * @brief Создание spinbox с кнопками +/-
 */
static lv_obj_t* create_spinbox(lv_obj_t *parent, const char *label_text, 
                                 int32_t value, int32_t min, int32_t max, uint8_t digit_count) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 280, 35);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(cont, 2, 0);
    
    // Label
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, label_text);
    lv_obj_set_width(label, 50);
    
    // Кнопка -
    lv_obj_t *btn_dec = lv_btn_create(cont);
    lv_obj_set_size(btn_dec, 30, 30);
    lv_obj_t *label_dec = lv_label_create(btn_dec);
    lv_label_set_text(label_dec, LV_SYMBOL_MINUS);
    lv_obj_center(label_dec);
    
    // Spinbox
    lv_obj_t *spinbox = lv_spinbox_create(cont);
    lv_spinbox_set_range(spinbox, min, max);
    lv_spinbox_set_value(spinbox, value);
    lv_spinbox_set_digit_format(spinbox, digit_count, digit_count - 2);
    lv_obj_set_width(spinbox, 100);
    
    // Кнопка +
    lv_obj_t *btn_inc = lv_btn_create(cont);
    lv_obj_set_size(btn_inc, 30, 30);
    lv_obj_t *label_inc = lv_label_create(btn_inc);
    lv_label_set_text(label_inc, LV_SYMBOL_PLUS);
    lv_obj_center(label_inc);
    
    // События
    lv_obj_add_event_cb(btn_dec, spinbox_decrement, LV_EVENT_CLICKED, spinbox);
    lv_obj_add_event_cb(btn_inc, spinbox_increment, LV_EVENT_CLICKED, spinbox);
    
    return spinbox;
}

/**
 * @brief Создание экрана
 */
static lv_obj_t* pid_detail_create(const char *param) {
    // Парсинг параметра
    if (param) {
        current_pid_idx = atoi(param);
    }
    
    if (current_pid_idx < 0 || current_pid_idx >= 6) {
        current_pid_idx = 0;
    }
    
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[32];
    snprintf(title_text, sizeof(title_text), "PID: %s", pid_names[current_pid_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    
    // График ошибки и выхода
    chart = lv_chart_create(screen);
    lv_obj_set_size(chart, 300, 80);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 20);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -50, 50);
    lv_chart_set_point_count(chart, 20);
    
    ser_error = lv_chart_add_series(chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
    ser_output = lv_chart_add_series(chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
    
    // Вклады P/I/D
    lv_obj_t *contrib_cont = lv_obj_create(screen);
    lv_obj_set_size(contrib_cont, 300, 30);
    lv_obj_align(contrib_cont, LV_ALIGN_TOP_MID, 0, 105);
    lv_obj_set_flex_flow(contrib_cont, LV_FLEX_FLOW_ROW);
    
    label_p = lv_label_create(contrib_cont);
    lv_label_set_text(label_p, "P:0.0");
    lv_obj_set_width(label_p, 60);
    
    label_i = lv_label_create(contrib_cont);
    lv_label_set_text(label_i, "I:0.0");
    lv_obj_set_width(label_i, 60);
    
    label_d = lv_label_create(contrib_cont);
    lv_label_set_text(label_d, "D:0.0");
    lv_obj_set_width(label_d, 60);
    
    label_output = lv_label_create(contrib_cont);
    lv_label_set_text(label_output, "Out:0.0");
    
    // Настройка коэффициентов
    pump_pid_instance_t *instance = pump_pid_get_instance((pump_pid_index_t)current_pid_idx);
    if (instance) {
        int32_t kp_val = (int32_t)(instance->pid.config.kp * 10);
        int32_t ki_val = (int32_t)(instance->pid.config.ki * 100);
        int32_t kd_val = (int32_t)(instance->pid.config.kd * 100);
        
        spinbox_kp = create_spinbox(screen, "Kp:", kp_val, 0, 100, 3);
        lv_obj_align(spinbox_kp->parent, LV_ALIGN_TOP_LEFT, 10, 140);
        
        spinbox_ki = create_spinbox(screen, "Ki:", ki_val, 0, 200, 4);
        lv_obj_align(spinbox_ki->parent, LV_ALIGN_TOP_LEFT, 10, 175);
        
        spinbox_kd = create_spinbox(screen, "Kd:", kd_val, 0, 100, 4);
        lv_obj_align(spinbox_kd->parent, LV_ALIGN_TOP_LEFT, 10, 180);
    }
    
    // Кнопки
    lv_obj_t *btn_save = lv_btn_create(screen);
    lv_obj_set_size(btn_save, 90, 30);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_add_event_cb(btn_save, on_save_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "Save");
    lv_obj_center(label_save);
    
    lv_obj_t *btn_reset = lv_btn_create(screen);
    lv_obj_set_size(btn_reset, 90, 30);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(btn_reset, on_reset_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_reset = lv_label_create(btn_reset);
    lv_label_set_text(label_reset, "Reset I");
    lv_obj_center(label_reset);
    
    lv_obj_t *btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 90, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_add_event_cb(btn_back, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(label_back);
    
    // Таймер обновления
    update_timer = lv_timer_create(update_chart, 500, NULL);
    
    return screen;
}

/**
 * @brief Уничтожение экрана
 */
static void pid_detail_destroy(lv_obj_t *screen) {
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    lv_obj_del(screen);
}

/**
 * @brief Инициализация
 */
void pid_detail_screen_init(void) {
    screen_config_t config = {
        .name = "pid_detail",
        .create_cb = pid_detail_create,
        .destroy_cb = pid_detail_destroy,
        .show_cb = NULL,
        .hide_cb = NULL,
    };
    
    screen_register(&config);
    ESP_LOGI(TAG, "PID detail screen registered");
}

