/**
 * @file pid_tuning_screen.c
 * @brief Реализация экрана настройки базовых параметров PID
 */

#include "pid_tuning_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PID_TUNING_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_kp_input = NULL;
static lv_obj_t *g_ki_input = NULL;
static lv_obj_t *g_kd_input = NULL;
static lv_obj_t *g_kp_warning = NULL;
static lv_obj_t *g_ki_warning = NULL;
static lv_obj_t *g_kd_warning = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  ВАЛИДАЦИЯ
 * ============================= */

static void validate_kp(float value, lv_obj_t *warning_label)
{
    if (value > 5.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение слишком высокое");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else if (value < 0.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение не может быть отрицательным");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void validate_ki(float value, lv_obj_t *warning_label)
{
    if (value > 3.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение слишком высокое");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else if (value < 0.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение не может быть отрицательным");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void validate_kd(float value, lv_obj_t *warning_label)
{
    if (value > 1.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение слишком высокое");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else if (value < 0.0f) {
        lv_label_set_text(warning_label, "⚠️ Значение не может быть отрицательным");
        lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для изменения значений (валидация)
 */
static void on_value_changed(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    const char *text = lv_textarea_get_text(textarea);
    float value = atof(text);
    
    if (textarea == g_kp_input) {
        validate_kp(value, g_kp_warning);
    } else if (textarea == g_ki_input) {
        validate_ki(value, g_ki_warning);
    } else if (textarea == g_kd_input) {
        validate_kd(value, g_kd_warning);
    }
}

/**
 * @brief Callback для кнопки "Сохранить"
 */
static void on_save_click(lv_event_t *e)
{
    // Получение значений
    float kp = atof(lv_textarea_get_text(g_kp_input));
    float ki = atof(lv_textarea_get_text(g_ki_input));
    float kd = atof(lv_textarea_get_text(g_kd_input));
    
    ESP_LOGI(TAG, "Сохранение PID для %s: Kp=%.2f Ki=%.2f Kd=%.2f",
             PUMP_NAMES[g_pump_idx], kp, ki, kd);
    
    // Установка в pump_manager
    pump_manager_set_pid_tunings(g_pump_idx, kp, ki, kd);
    
    // Сохранение в config
    system_config_t config;
    esp_err_t err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_pid[g_pump_idx].kp = kp;
        config.pump_pid[g_pump_idx].ki = ki;
        config.pump_pid[g_pump_idx].kd = kd;
        err = config_save(&config);
    }
    
    // Логирование результата
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Настройки PID успешно сохранены");
    } else {
        ESP_LOGE(TAG, "Ошибка сохранения настроек PID");
    }
}

/**
 * @brief Callback для кнопки "По умолчанию"
 */
static void on_defaults_click(lv_event_t *e)
{
    // Получение дефолтных значений
    system_config_t defaults;
    config_manager_get_defaults(&defaults);
    
    float kp = defaults.pump_pid[g_pump_idx].kp;
    float ki = defaults.pump_pid[g_pump_idx].ki;
    float kd = defaults.pump_pid[g_pump_idx].kd;
    
    // Установка в поля ввода
    char text[16];
    
    snprintf(text, sizeof(text), "%.2f", kp);
    lv_textarea_set_text(g_kp_input, text);
    
    snprintf(text, sizeof(text), "%.2f", ki);
    lv_textarea_set_text(g_ki_input, text);
    
    snprintf(text, sizeof(text), "%.2f", kd);
    lv_textarea_set_text(g_kd_input, text);
    
    // Скрыть предупреждения
    lv_obj_add_flag(g_kp_warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ki_warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_kd_warning, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Восстановлены дефолтные значения PID");
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_tuning_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGI(TAG, "Создание экрана настройки PID для насоса %d", g_pump_idx);
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "PID Настройка");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[48];
    snprintf(title_text, sizeof(title_text), "Настройка PID: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Получение текущей конфигурации
    const system_config_t *config = config_manager_get_cached();
    float kp = config ? config->pump_pid[g_pump_idx].kp : 1.0f;
    float ki = config ? config->pump_pid[g_pump_idx].ki : 0.1f;
    float kd = config ? config->pump_pid[g_pump_idx].kd : 0.0f;
    
    int y_offset = 65;
    int row_height = 50;
    
    // Kp параметр
    lv_obj_t *kp_label = lv_label_create(screen);
    lv_label_set_text(kp_label, "Kp:");
    lv_obj_set_style_text_color(kp_label, lv_color_white(), 0);
    lv_obj_align(kp_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    g_kp_input = lv_textarea_create(screen);
    lv_obj_set_size(g_kp_input, 120, 35);
    char kp_text[16];
    snprintf(kp_text, sizeof(kp_text), "%.2f", kp);
    lv_textarea_set_text(g_kp_input, kp_text);
    lv_textarea_set_one_line(g_kp_input, true);
    lv_textarea_set_max_length(g_kp_input, 6);
    lv_obj_align(g_kp_input, LV_ALIGN_TOP_LEFT, 50, y_offset - 5);
    lv_obj_add_event_cb(g_kp_input, on_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_kp_warning = lv_label_create(screen);
    lv_label_set_text(g_kp_warning, "");
    lv_obj_set_style_text_color(g_kp_warning, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_kp_warning, &montserrat_ru, 0);
    lv_obj_align(g_kp_warning, LV_ALIGN_TOP_LEFT, 50, y_offset + 28);
    lv_obj_add_flag(g_kp_warning, LV_OBJ_FLAG_HIDDEN);
    
    // Ki параметр
    y_offset += row_height;
    lv_obj_t *ki_label = lv_label_create(screen);
    lv_label_set_text(ki_label, "Ki:");
    lv_obj_set_style_text_color(ki_label, lv_color_white(), 0);
    lv_obj_align(ki_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    g_ki_input = lv_textarea_create(screen);
    lv_obj_set_size(g_ki_input, 120, 35);
    char ki_text[16];
    snprintf(ki_text, sizeof(ki_text), "%.2f", ki);
    lv_textarea_set_text(g_ki_input, ki_text);
    lv_textarea_set_one_line(g_ki_input, true);
    lv_textarea_set_max_length(g_ki_input, 6);
    lv_obj_align(g_ki_input, LV_ALIGN_TOP_LEFT, 50, y_offset - 5);
    lv_obj_add_event_cb(g_ki_input, on_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_ki_warning = lv_label_create(screen);
    lv_label_set_text(g_ki_warning, "");
    lv_obj_set_style_text_color(g_ki_warning, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_ki_warning, &montserrat_ru, 0);
    lv_obj_align(g_ki_warning, LV_ALIGN_TOP_LEFT, 50, y_offset + 28);
    lv_obj_add_flag(g_ki_warning, LV_OBJ_FLAG_HIDDEN);
    
    // Kd параметр
    y_offset += row_height;
    lv_obj_t *kd_label = lv_label_create(screen);
    lv_label_set_text(kd_label, "Kd:");
    lv_obj_set_style_text_color(kd_label, lv_color_white(), 0);
    lv_obj_align(kd_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    g_kd_input = lv_textarea_create(screen);
    lv_obj_set_size(g_kd_input, 120, 35);
    char kd_text[16];
    snprintf(kd_text, sizeof(kd_text), "%.2f", kd);
    lv_textarea_set_text(g_kd_input, kd_text);
    lv_textarea_set_one_line(g_kd_input, true);
    lv_textarea_set_max_length(g_kd_input, 6);
    lv_obj_align(g_kd_input, LV_ALIGN_TOP_LEFT, 50, y_offset - 5);
    lv_obj_add_event_cb(g_kd_input, on_value_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_kd_warning = lv_label_create(screen);
    lv_label_set_text(g_kd_warning, "");
    lv_obj_set_style_text_color(g_kd_warning, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_kd_warning, &montserrat_ru, 0);
    lv_obj_align(g_kd_warning, LV_ALIGN_TOP_LEFT, 50, y_offset + 28);
    lv_obj_add_flag(g_kd_warning, LV_OBJ_FLAG_HIDDEN);
    
    // Кнопки
    y_offset += row_height + 10;
    
    // Кнопка "Сохранить"
    lv_obj_t *save_btn = lv_btn_create(screen);
    lv_obj_set_size(save_btn, 100, 35);
    lv_obj_align(save_btn, LV_ALIGN_TOP_LEFT, 10, y_offset);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(save_btn, on_save_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Сохранить");
    lv_obj_center(save_label);
    
    // Кнопка "По умолчанию"
    lv_obj_t *defaults_btn = lv_btn_create(screen);
    lv_obj_set_size(defaults_btn, 100, 35);
    lv_obj_align(defaults_btn, LV_ALIGN_TOP_RIGHT, -10, y_offset);
    lv_obj_set_style_bg_color(defaults_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(defaults_btn, on_defaults_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *defaults_label = lv_label_create(defaults_btn);
    lv_label_set_text(defaults_label, "Дефолт");
    lv_obj_center(defaults_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Экран настройки PID создан");
    
    return screen;
}

