/**
 * @file pid_intelligent_detail.c
 * @brief Реализация детального экрана адаптивного PID
 */

#include "pid_intelligent_detail.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/event_helpers.h"
#include "../../lvgl_styles.h"
#include "montserrat14_ru.h"
#include "adaptive_pid.h"
#include "pump_manager.h"
#include "pid_auto_tuner.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <stdio.h>

static const char *TAG = "PID_DETAIL";

// Глобальные UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_tabview = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_DOWN;

// Вкладка "Обзор"
static lv_obj_t *g_overview_status_label = NULL;
static lv_obj_t *g_overview_values_label = NULL;
static lv_obj_t *g_overview_pid_label = NULL;
static lv_obj_t *g_overview_adaptive_label = NULL;

// Вкладка "Настройки"
static lv_obj_t *g_settings_kp_slider = NULL;
static lv_obj_t *g_settings_ki_slider = NULL;
static lv_obj_t *g_settings_kd_slider = NULL;
static lv_obj_t *g_settings_kp_label = NULL;
static lv_obj_t *g_settings_ki_label = NULL;
static lv_obj_t *g_settings_kd_label = NULL;

// Вкладка "График"
static lv_obj_t *g_chart = NULL;
static lv_chart_series_t *g_series_current = NULL;
static lv_chart_series_t *g_series_target = NULL;
static lv_chart_series_t *g_series_predicted = NULL;

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Обновление вкладки "Обзор"
 */
static void update_overview_tab(void) {
    if (!g_overview_status_label) return;
    
    const adaptive_pid_state_t *state = adaptive_pid_get_state(g_pump_idx);
    if (!state) return;
    
    // Статус
    const char *status_text = "Статус: Неизвестен";
    if (state->safe_mode) {
        status_text = "Статус: Безопасный режим";
    } else if (state->learning_mode) {
        status_text = "Статус: Обучение";
    } else if (state->prediction_enabled) {
        status_text = "Статус: Активен (прогноз)";
    } else {
        status_text = "Статус: Активен";
    }
    lv_label_set_text(g_overview_status_label, status_text);
    
    // Значения
    char values_text[128];
    float current = (g_pump_idx < 2) ? 7.0f : 1.5f; // TODO: реальные датчики
    float target = (g_pump_idx < 2) ? 6.5f : 1.8f;
    snprintf(values_text, sizeof(values_text), 
             "Текущее: %.2f\nЦель: %.2f\nОшибка: %.2f", 
             current, target, current - target);
    lv_label_set_text(g_overview_values_label, values_text);
    
    // PID компоненты
    pid_output_t output;
    if (pump_manager_compute_pid(g_pump_idx, current, target, &output) == ESP_OK) {
        char pid_text[128];
        snprintf(pid_text, sizeof(pid_text),
                 "P: %.3f\nI: %.3f\nD: %.3f\nВыход: %.3f",
                 output.p_term, output.i_term, output.d_term, output.output);
        lv_label_set_text(g_overview_pid_label, pid_text);
    }
    
    // Адаптивная информация
    float base_kp = 0.0f, base_ki = 0.0f, base_kd = 0.0f;
    pump_manager_get_pid_tunings(g_pump_idx, &base_kp, &base_ki, &base_kd);
    
    char adaptive_text[128];
    snprintf(adaptive_text, sizeof(adaptive_text),
             "Kp адапт: %.2f (базовый: %.2f)\nКоррекций: %lu\nБуферная емкость: %.3f",
             state->kp_adaptive, base_kp, 
             (unsigned long)state->total_corrections, state->buffer_capacity);
    lv_label_set_text(g_overview_adaptive_label, adaptive_text);
}

/**
 * @brief Callback для перехода на экран автонастройки
 */
static void on_autotune_click(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        screen_show("pid_auto_tune", (void*)(intptr_t)g_pump_idx);
    }
}

/**
 * @brief Callback изменения слайдера Kp
 */
static void on_kp_slider_changed(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    
    lv_obj_t *slider = lv_event_get_target(e);
    float kp = (float)lv_slider_get_value(slider) / 100.0f;
    
    char text[32];
    snprintf(text, sizeof(text), "Kp: %.2f", kp);
    lv_label_set_text(g_settings_kp_label, text);
    
    // Применение к системе
    float ki, kd;
    pump_manager_get_pid_tunings(g_pump_idx, NULL, &ki, &kd);
    pump_manager_set_pid_tunings(g_pump_idx, kp, ki, kd);
    
    ESP_LOGI(TAG, "Kp изменен на %.2f для %s", kp, PUMP_NAMES[g_pump_idx]);
}

/**
 * @brief Callback изменения слайдера Ki
 */
static void on_ki_slider_changed(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    
    lv_obj_t *slider = lv_event_get_target(e);
    float ki = (float)lv_slider_get_value(slider) / 1000.0f;
    
    char text[32];
    snprintf(text, sizeof(text), "Ki: %.3f", ki);
    lv_label_set_text(g_settings_ki_label, text);
    
    // Применение к системе
    float kp, kd;
    pump_manager_get_pid_tunings(g_pump_idx, &kp, NULL, &kd);
    pump_manager_set_pid_tunings(g_pump_idx, kp, ki, kd);
    
    ESP_LOGI(TAG, "Ki изменен на %.3f для %s", ki, PUMP_NAMES[g_pump_idx]);
}

/**
 * @brief Callback изменения слайдера Kd
 */
static void on_kd_slider_changed(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    
    lv_obj_t *slider = lv_event_get_target(e);
    float kd = (float)lv_slider_get_value(slider) / 100.0f;
    
    char text[32];
    snprintf(text, sizeof(text), "Kd: %.2f", kd);
    lv_label_set_text(g_settings_kd_label, text);
    
    // Применение к системе
    float kp, ki;
    pump_manager_get_pid_tunings(g_pump_idx, &kp, &ki, NULL);
    pump_manager_set_pid_tunings(g_pump_idx, kp, ki, kd);
    
    ESP_LOGI(TAG, "Kd изменен на %.2f для %s", kd, PUMP_NAMES[g_pump_idx]);
}

/**
 * @brief Создание вкладки "Обзор"
 */
static void create_overview_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    
    // Статус
    g_overview_status_label = lv_label_create(parent);
    lv_label_set_text(g_overview_status_label, "Статус: Загрузка...");
    lv_obj_set_style_text_color(g_overview_status_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(g_overview_status_label, &montserrat_ru, 0);
    
    // Значения
    g_overview_values_label = lv_label_create(parent);
    lv_label_set_text(g_overview_values_label, "Загрузка...");
    lv_obj_set_style_text_color(g_overview_values_label, lv_color_white(), 0);
    
    // PID компоненты
    g_overview_pid_label = lv_label_create(parent);
    lv_label_set_text(g_overview_pid_label, "Загрузка...");
    lv_obj_set_style_text_color(g_overview_pid_label, lv_color_hex(0xaaaaaa), 0);
    
    // Адаптивная информация
    g_overview_adaptive_label = lv_label_create(parent);
    lv_label_set_text(g_overview_adaptive_label, "Загрузка...");
    lv_obj_set_style_text_color(g_overview_adaptive_label, lv_color_hex(0xffaa00), 0);
}

/**
 * @brief Создание вкладки "Настройки"
 */
static void create_settings_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    
    // Получение текущих коэффициентов
    float kp, ki, kd;
    pump_manager_get_pid_tunings(g_pump_idx, &kp, &ki, &kd);
    
    // Слайдер Kp
    g_settings_kp_label = lv_label_create(parent);
    char kp_text[32];
    snprintf(kp_text, sizeof(kp_text), "Kp: %.2f", kp);
    lv_label_set_text(g_settings_kp_label, kp_text);
    
    g_settings_kp_slider = lv_slider_create(parent);
    lv_obj_set_width(g_settings_kp_slider, LV_PCT(90));
    lv_slider_set_range(g_settings_kp_slider, 0, 1000); // 0.00 - 10.00
    lv_slider_set_value(g_settings_kp_slider, (int32_t)(kp * 100), LV_ANIM_OFF);
    lv_obj_add_event_cb(g_settings_kp_slider, on_kp_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Слайдер Ki
    g_settings_ki_label = lv_label_create(parent);
    char ki_text[32];
    snprintf(ki_text, sizeof(ki_text), "Ki: %.3f", ki);
    lv_label_set_text(g_settings_ki_label, ki_text);
    
    g_settings_ki_slider = lv_slider_create(parent);
    lv_obj_set_width(g_settings_ki_slider, LV_PCT(90));
    lv_slider_set_range(g_settings_ki_slider, 0, 1000); // 0.000 - 1.000
    lv_slider_set_value(g_settings_ki_slider, (int32_t)(ki * 1000), LV_ANIM_OFF);
    lv_obj_add_event_cb(g_settings_ki_slider, on_ki_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Слайдер Kd
    g_settings_kd_label = lv_label_create(parent);
    char kd_text[32];
    snprintf(kd_text, sizeof(kd_text), "Kd: %.2f", kd);
    lv_label_set_text(g_settings_kd_label, kd_text);
    
    g_settings_kd_slider = lv_slider_create(parent);
    lv_obj_set_width(g_settings_kd_slider, LV_PCT(90));
    lv_slider_set_range(g_settings_kd_slider, 0, 500); // 0.00 - 5.00
    lv_slider_set_value(g_settings_kd_slider, (int32_t)(kd * 100), LV_ANIM_OFF);
    lv_obj_add_event_cb(g_settings_kd_slider, on_kd_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Кнопка автонастройки
    lv_obj_t *autotune_btn = lv_btn_create(parent);
    lv_obj_set_width(autotune_btn, LV_PCT(90));
    lv_obj_t *autotune_label = lv_label_create(autotune_btn);
    lv_label_set_text(autotune_label, "Автонастройка PID");
    lv_obj_center(autotune_label);
    widget_add_click_handler(autotune_btn, on_autotune_click, NULL);
}

/**
 * @brief Создание вкладки "График"
 */
static void create_graph_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 8, 0);
    
    // Создание графика (компактно для вкладки)
    g_chart = lv_chart_create(parent);
    lv_obj_set_size(g_chart, LV_PCT(95), 160);  // Уменьшено с 180 до 160px
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, 20); // 20 точек
    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100); // 0-10.0 (умножено на 10)
    
    // Серии данных
    g_series_current = lv_chart_add_series(g_chart, lv_color_hex(0x00D4AA), LV_CHART_AXIS_PRIMARY_Y);
    g_series_target = lv_chart_add_series(g_chart, lv_color_hex(0xffaa00), LV_CHART_AXIS_PRIMARY_Y);
    g_series_predicted = lv_chart_add_series(g_chart, lv_color_hex(0xaa00ff), LV_CHART_AXIS_PRIMARY_Y);
    
    // Легенда
    lv_obj_t *legend = lv_label_create(parent);
    lv_label_set_text(legend, "⚫ Текущее  ⚫ Цель  ⚫ Прогноз");
    lv_obj_set_style_text_font(legend, &lv_font_montserrat_10, 0);
}

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

lv_obj_t* pid_intelligent_detail_create(void *params) {
    pump_index_t pump_idx = (pump_index_t)(intptr_t)params;
    
    if (pump_idx >= PUMP_INDEX_COUNT) {
        ESP_LOGE(TAG, "Некорректный pump_idx: %d", pump_idx);
        return NULL;
    }
    
    g_pump_idx = pump_idx;
    
    ESP_LOGI(TAG, "Создание детального экрана для %s", PUMP_NAMES[pump_idx]);
    
    // Создание экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) {
        ESP_LOGE(TAG, "Не удалось создать экран");
        return NULL;
    }
    
    extern lv_style_t style_bg;
    lv_obj_add_style(screen, &style_bg, 0);
    lv_obj_set_style_pad_all(screen, 4, 0);
    g_screen = screen;
    
    // Status bar с названием насоса
    char title[64];
    snprintf(title, sizeof(title), "PID: %s", PUMP_NAMES[pump_idx]);
    widget_create_status_bar(screen, title);
    
    // Back button
    widget_create_back_button(screen, NULL, NULL);
    
    // Табвью с 3 вкладками
    g_tabview = lv_tabview_create(screen);
    lv_obj_set_size(g_tabview, LV_PCT(100), 270);
    lv_obj_align(g_tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(g_tabview, lv_color_hex(0x1a1a1a), 0);
    
    // КРИТИЧНО: Feed watchdog при создании вкладок
    esp_task_wdt_reset();
    
    // Создание вкладок
    lv_obj_t *tab_overview = lv_tabview_add_tab(g_tabview, "Обзор");
    lv_obj_t *tab_settings = lv_tabview_add_tab(g_tabview, "Настройки");
    lv_obj_t *tab_graph = lv_tabview_add_tab(g_tabview, "График");
    
    esp_task_wdt_reset();
    
    // Заполнение вкладок
    create_overview_tab(tab_overview);
    esp_task_wdt_reset();
    
    create_settings_tab(tab_settings);
    esp_task_wdt_reset();
    
    create_graph_tab(tab_graph);
    esp_task_wdt_reset();
    
    ESP_LOGI(TAG, "Детальный экран создан успешно");
    
    return screen;
}

esp_err_t pid_intelligent_detail_on_show(lv_obj_t *screen, void *params) {
    (void)screen;
    
    pump_index_t pump_idx = (pump_index_t)(intptr_t)params;
    g_pump_idx = pump_idx;
    
    ESP_LOGI(TAG, "Детальный экран показан для %s", PUMP_NAMES[pump_idx]);
    
    // Обновление данных
    update_overview_tab();
    
    return ESP_OK;
}

esp_err_t pid_intelligent_detail_on_hide(lv_obj_t *screen) {
    (void)screen;
    
    ESP_LOGI(TAG, "Детальный экран скрыт");
    
    // Очистка данных
    g_overview_status_label = NULL;
    g_overview_values_label = NULL;
    g_overview_pid_label = NULL;
    g_overview_adaptive_label = NULL;
    g_settings_kp_slider = NULL;
    g_settings_ki_slider = NULL;
    g_settings_kd_slider = NULL;
    g_chart = NULL;
    g_tabview = NULL;
    g_screen = NULL;
    
    return ESP_OK;
}

