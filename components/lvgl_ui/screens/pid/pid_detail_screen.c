/**
 * @file pid_detail_screen.c
 * @brief Реализация экрана детальной информации PID
 */

#include "pid_detail_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/event_helpers.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "system_config.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_DETAIL_SCREEN";

// UI элементы
static lv_obj_t *g_screen = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для кнопки "Настроить"
 */
static void on_tune_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к настройке PID для насоса %d", g_pump_idx);
    screen_show("pid_tuning", (void*)(intptr_t)g_pump_idx);
}

/**
 * @brief Callback для кнопки "Расширенные"
 */
static void on_advanced_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к расширенным настройкам для насоса %d", g_pump_idx);
    screen_show("pid_advanced", (void*)(intptr_t)g_pump_idx);
}

/**
 * @brief Callback для кнопки "Пороги"
 */
static void on_thresholds_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к настройке порогов для насоса %d", g_pump_idx);
    screen_show("pid_thresholds", (void*)(intptr_t)g_pump_idx);
}

/**
 * @brief Callback для кнопки "Сбросить интеграл"
 */
static void on_reset_integral_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Сброс интеграла для насоса %d", g_pump_idx);
    pump_manager_reset_pid(g_pump_idx);
}

/**
 * @brief Callback для кнопки "Тест"
 */
static void on_test_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Тестовый запуск насоса %d на 5 секунд", g_pump_idx);
    
    // Запуск насоса на 5 секунд напрямую
    pump_manager_run_direct(g_pump_idx, 5000);
}

/**
 * @brief Callback для кнопки "График"
 */
static void on_graph_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к графику для насоса %d", g_pump_idx);
    screen_show("pid_graph", (void*)(intptr_t)g_pump_idx);
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_detail_screen_create(void *context)
{
    // Получение индекса насоса из контекста
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGD(TAG, "Создание экрана деталей PID для насоса %d", g_pump_idx);
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create PID detail screen");
        return NULL;
    }
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "PID Детали");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок с именем насоса
    lv_obj_t *title = lv_label_create(screen);
    char title_text[32];
    snprintf(title_text, sizeof(title_text), "PID: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Получение конфигурации
    const system_config_t *config = config_manager_get_cached();
    
    // Информационная панель
    lv_obj_t *info_container = lv_obj_create(screen);
    if (!info_container) {
        ESP_LOGW(TAG, "Failed to create info_container, skipping info panel");
    } else {
        lv_obj_set_size(info_container, 220, 140);
        lv_obj_align(info_container, LV_ALIGN_TOP_MID, 0, 65);
        lv_obj_set_style_bg_color(info_container, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_width(info_container, 1, 0);
        lv_obj_set_style_border_color(info_container, lv_color_hex(0x444444), 0);
        lv_obj_set_style_pad_all(info_container, 8, 0);
        lv_obj_clear_flag(info_container, LV_OBJ_FLAG_SCROLLABLE);
        
        // Параметры PID
        char info_text[256];
        if (config) {
            snprintf(info_text, sizeof(info_text),
                     "PID Параметры:\n"
                     "Kp: %.2f  Ki: %.2f  Kd: %.2f\n\n"
                     "Пороги:\n"
                     "Активация: %.2f\n"
                     "Деактивация: %.2f\n\n"
                     "Лимиты:\n"
                     "Выход: %.1f-%.1f мл\n"
                     "Макс. доза: %.1f мл\n"
                     "Суточный лимит: %lu мл",
                     config->pump_pid[g_pump_idx].kp,
                     config->pump_pid[g_pump_idx].ki,
                     config->pump_pid[g_pump_idx].kd,
                     config->pump_pid[g_pump_idx].activation_threshold,
                     config->pump_pid[g_pump_idx].deactivation_threshold,
                     config->pump_pid[g_pump_idx].output_min,
                     config->pump_pid[g_pump_idx].output_max,
                     config->pump_pid[g_pump_idx].max_dose_per_cycle,
                     (unsigned long)config->pump_pid[g_pump_idx].max_daily_volume);
        } else {
            snprintf(info_text, sizeof(info_text), "Конфигурация недоступна");
        }
        
        lv_obj_t *info_label = lv_label_create(info_container);
        if (info_label) {
            lv_label_set_text(info_label, info_text);
            lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
        }
    }
    
    // Кнопки управления (3 ряда по 2 кнопки)
    int btn_y_start = 215;
    int btn_spacing = 38;
    
    // Ряд 1: Настроить, Расширенные
    lv_obj_t *tune_btn = lv_btn_create(screen);
    lv_obj_set_size(tune_btn, 100, 32);
    lv_obj_align(tune_btn, LV_ALIGN_TOP_LEFT, 10, btn_y_start);
    lv_obj_set_style_bg_color(tune_btn, lv_color_hex(0x2196F3), 0);
    widget_add_click_handler(tune_btn, on_tune_click, NULL);
    lv_obj_t *tune_label = lv_label_create(tune_btn);
    lv_label_set_text(tune_label, "Настр.");
    lv_obj_center(tune_label);
    
    lv_obj_t *advanced_btn = lv_btn_create(screen);
    lv_obj_set_size(advanced_btn, 100, 32);
    lv_obj_align(advanced_btn, LV_ALIGN_TOP_RIGHT, -10, btn_y_start);
    lv_obj_set_style_bg_color(advanced_btn, lv_color_hex(0x9C27B0), 0);
    widget_add_click_handler(advanced_btn, on_advanced_click, NULL);
    lv_obj_t *adv_label = lv_label_create(advanced_btn);
    lv_label_set_text(adv_label, "Расшир.");
    lv_obj_center(adv_label);
    
    // Ряд 2: Пороги, Сброс
    lv_obj_t *thresh_btn = lv_btn_create(screen);
    lv_obj_set_size(thresh_btn, 100, 32);
    lv_obj_align(thresh_btn, LV_ALIGN_TOP_LEFT, 10, btn_y_start + btn_spacing);
    lv_obj_set_style_bg_color(thresh_btn, lv_color_hex(0xFF9800), 0);
    widget_add_click_handler(thresh_btn, on_thresholds_click, NULL);
    lv_obj_t *thresh_label = lv_label_create(thresh_btn);
    lv_label_set_text(thresh_label, "Пороги");
    lv_obj_center(thresh_label);
    
    lv_obj_t *reset_btn = lv_btn_create(screen);
    lv_obj_set_size(reset_btn, 100, 32);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_RIGHT, -10, btn_y_start + btn_spacing);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xF44336), 0);
    widget_add_click_handler(reset_btn, on_reset_integral_click, NULL);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Сброс I");
    lv_obj_center(reset_label);
    
    // Ряд 3: Тест, График
    lv_obj_t *test_btn = lv_btn_create(screen);
    lv_obj_set_size(test_btn, 100, 32);
    lv_obj_align(test_btn, LV_ALIGN_TOP_LEFT, 10, btn_y_start + btn_spacing * 2);
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(0x4CAF50), 0);
    widget_add_click_handler(test_btn, on_test_click, NULL);
    lv_obj_t *test_label = lv_label_create(test_btn);
    lv_label_set_text(test_label, "Тест 5с");
    lv_obj_center(test_label);
    
    lv_obj_t *graph_btn = lv_btn_create(screen);
    lv_obj_set_size(graph_btn, 100, 32);
    lv_obj_align(graph_btn, LV_ALIGN_TOP_RIGHT, -10, btn_y_start + btn_spacing * 2);
    lv_obj_set_style_bg_color(graph_btn, lv_color_hex(0x00BCD4), 0);
    widget_add_click_handler(graph_btn, on_graph_click, NULL);
    lv_obj_t *graph_label = lv_label_create(graph_btn);
    lv_label_set_text(graph_label, "График");
    lv_obj_center(graph_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGD(TAG, "Экран деталей PID создан для насоса %s", PUMP_NAMES[g_pump_idx]);
    
    return screen;
}

