/**
 * @file pid_thresholds_screen.c
 * @brief Реализация экрана настройки порогов срабатывания PID
 */

#include "pid_thresholds_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PID_THRESH_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_activation_input = NULL;
static lv_obj_t *g_deactivation_input = NULL;
static lv_obj_t *g_warning_label = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Валидация порогов
 */
static void validate_thresholds(void)
{
    float activation = atof(lv_textarea_get_text(g_activation_input));
    float deactivation = atof(lv_textarea_get_text(g_deactivation_input));
    
    if (deactivation >= activation) {
        lv_label_set_text(g_warning_label, "⚠️ Деактивация должна быть меньше активации!");
        lv_obj_clear_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    } else if (activation <= 0 || deactivation <= 0) {
        lv_label_set_text(g_warning_label, "⚠️ Значения должны быть положительными!");
        lv_obj_clear_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Callback для изменения значений
 */
static void on_value_changed(lv_event_t *e)
{
    validate_thresholds();
}

/**
 * @brief Callback для кнопки "Применить"
 */
static void on_apply_click(lv_event_t *e)
{
    float activation = atof(lv_textarea_get_text(g_activation_input));
    float deactivation = atof(lv_textarea_get_text(g_deactivation_input));
    
    // Валидация
    if (deactivation >= activation) {
        ESP_LOGW(TAG, "Ошибка валидации: порог деактивации >= активации");
        return;
    }
    
    ESP_LOGI(TAG, "Сохранение порогов для %s: активация=%.2f деактивация=%.2f",
             PUMP_NAMES[g_pump_idx], activation, deactivation);
    
    // Сохранение в конфигурацию
    system_config_t config;
    esp_err_t err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_pid[g_pump_idx].activation_threshold = activation;
        config.pump_pid[g_pump_idx].deactivation_threshold = deactivation;
        err = config_save(&config);
        
        if (err == ESP_OK) {
            // Применить конфигурацию
            pump_manager_apply_config(&config);
            ESP_LOGI(TAG, "Пороги срабатывания успешно сохранены");
        }
    }
}

/**
 * @brief Callback для кнопки "По умолчанию"
 */
static void on_defaults_click(lv_event_t *e)
{
    system_config_t defaults;
    config_manager_get_defaults(&defaults);
    
    float activation = defaults.pump_pid[g_pump_idx].activation_threshold;
    float deactivation = defaults.pump_pid[g_pump_idx].deactivation_threshold;
    
    char text[16];
    snprintf(text, sizeof(text), "%.2f", activation);
    lv_textarea_set_text(g_activation_input, text);
    
    snprintf(text, sizeof(text), "%.2f", deactivation);
    lv_textarea_set_text(g_deactivation_input, text);
    
    validate_thresholds();
    
    ESP_LOGI(TAG, "Восстановлены дефолтные пороги");
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_thresholds_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGI(TAG, "Создание экрана настройки порогов для насоса %d", g_pump_idx);
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Пороги");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[48];
    snprintf(title_text, sizeof(title_text), "Пороги: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Получение текущей конфигурации
    const system_config_t *config = config_manager_get_cached();
    float activation = config ? config->pump_pid[g_pump_idx].activation_threshold : 0.3f;
    float deactivation = config ? config->pump_pid[g_pump_idx].deactivation_threshold : 0.05f;
    
    // Описание
    lv_obj_t *desc_label = lv_label_create(screen);
    lv_label_set_text(desc_label, 
                      "PID начинает работу при отклонении\n"
                      "больше порога активации.\n"
                      "Цель достигнута при отклонении\n"
                      "меньше порога деактивации.");
    lv_obj_set_style_text_color(desc_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(desc_label, &montserrat_ru, 0);
    lv_obj_align(desc_label, LV_ALIGN_TOP_LEFT, 10, 65);
    
    // Порог активации
    int y_offset = 135;
    lv_obj_t *act_label = lv_label_create(screen);
    lv_label_set_text(act_label, "Активация:");
    lv_obj_set_style_text_color(act_label, lv_color_white(), 0);
    lv_obj_align(act_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    g_activation_input = lv_textarea_create(screen);
    lv_obj_set_size(g_activation_input, 100, 35);
    char act_text[16];
    snprintf(act_text, sizeof(act_text), "%.2f", activation);
    lv_textarea_set_text(g_activation_input, act_text);
    lv_textarea_set_one_line(g_activation_input, true);
    lv_textarea_set_max_length(g_activation_input, 6);
    lv_obj_align(g_activation_input, LV_ALIGN_TOP_RIGHT, -10, y_offset - 5);
    lv_obj_add_event_cb(g_activation_input, on_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Порог деактивации
    y_offset += 45;
    lv_obj_t *deact_label = lv_label_create(screen);
    lv_label_set_text(deact_label, "Деактивация:");
    lv_obj_set_style_text_color(deact_label, lv_color_white(), 0);
    lv_obj_align(deact_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    g_deactivation_input = lv_textarea_create(screen);
    lv_obj_set_size(g_deactivation_input, 100, 35);
    char deact_text[16];
    snprintf(deact_text, sizeof(deact_text), "%.2f", deactivation);
    lv_textarea_set_text(g_deactivation_input, deact_text);
    lv_textarea_set_one_line(g_deactivation_input, true);
    lv_textarea_set_max_length(g_deactivation_input, 6);
    lv_obj_align(g_deactivation_input, LV_ALIGN_TOP_RIGHT, -10, y_offset - 5);
    lv_obj_add_event_cb(g_deactivation_input, on_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Предупреждение
    g_warning_label = lv_label_create(screen);
    lv_label_set_text(g_warning_label, "");
    lv_obj_set_style_text_color(g_warning_label, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_warning_label, &montserrat_ru, 0);
    lv_obj_align(g_warning_label, LV_ALIGN_TOP_MID, 0, y_offset + 35);
    lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    
    // Кнопки
    lv_obj_t *apply_btn = lv_btn_create(screen);
    lv_obj_set_size(apply_btn, 100, 35);
    lv_obj_align(apply_btn, LV_ALIGN_BOTTOM_LEFT, 10, -40);
    lv_obj_set_style_bg_color(apply_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(apply_btn, on_apply_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(apply_btn, on_apply_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *apply_label = lv_label_create(apply_btn);
    lv_label_set_text(apply_label, "Применить");
    lv_obj_center(apply_label);
    
    lv_obj_t *defaults_btn = lv_btn_create(screen);
    lv_obj_set_size(defaults_btn, 100, 35);
    lv_obj_align(defaults_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -40);
    lv_obj_set_style_bg_color(defaults_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(defaults_btn, on_defaults_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(defaults_btn, on_defaults_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *def_label = lv_label_create(defaults_btn);
    lv_label_set_text(def_label, "Дефолт");
    lv_obj_center(def_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Экран настройки порогов создан");
    
    return screen;
}

