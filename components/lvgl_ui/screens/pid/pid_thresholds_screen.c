/**
 * @file pid_thresholds_screen.c
 * @brief Реализация экрана настройки порогов срабатывания PID с управлением энкодером
 */

#include "pid_thresholds_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/encoder_value_edit.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_THRESH_SCREEN";

static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_activation_value = NULL;
static lv_obj_t *g_deactivation_value = NULL;
static lv_obj_t *g_warning_label = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  CALLBACKS
 * ============================= */

static void on_apply_click(lv_event_t *e)
{
    // Получение значений из виджетов
    float activation = widget_encoder_value_get(g_activation_value);
    float deactivation = widget_encoder_value_get(g_deactivation_value);
    
    // Валидация
    if (deactivation >= activation) {
        ESP_LOGW(TAG, "Ошибка: порог деактивации >= активации");
        lv_label_set_text(g_warning_label, "⚠️ Деактивация < Активации!");
        lv_obj_clear_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Сохранение порогов для %s: акт=%.2f деакт=%.2f",
             PUMP_NAMES[g_pump_idx], activation, deactivation);
    
    // Сохранение в конфигурацию
    system_config_t config;
    esp_err_t err = config_load(&config);
    if (err == ESP_OK) {
        config.pump_pid[g_pump_idx].activation_threshold = activation;
        config.pump_pid[g_pump_idx].deactivation_threshold = deactivation;
        err = config_save(&config);
        
        if (err == ESP_OK) {
            pump_manager_apply_config(&config);
            ESP_LOGI(TAG, "Пороги успешно сохранены");
        }
    }
}

static void on_defaults_click(lv_event_t *e)
{
    // Дефолтные значения (обычно 0.2 и 0.05)
    float activation = 0.2f;
    float deactivation = 0.05f;
    
    widget_encoder_value_set(g_activation_value, activation);
    widget_encoder_value_set(g_deactivation_value, deactivation);
    
    lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Восстановлены дефолтные пороги");
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_thresholds_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGI(TAG, "Создание экрана настройки порогов для %s", PUMP_NAMES[g_pump_idx]);
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "PID Thresholds");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "Пороги: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Загрузка текущих значений
    const system_config_t *config = config_manager_get_cached();
    float activation = config ? config->pump_pid[g_pump_idx].activation_threshold : 0.2f;
    float deactivation = config ? config->pump_pid[g_pump_idx].deactivation_threshold : 0.05f;
    
    int y_offset = 70;
    
    // Описание
    lv_obj_t *desc = lv_label_create(screen);
    lv_label_set_text(desc, "Активация - минимальное\nотклонение для включения PID\n\nДеактивация - отклонение\nдля выключения PID");
    lv_obj_set_style_text_color(desc, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(desc, &montserrat_ru, 0);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, 210);
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, y_offset);
    
    y_offset += 85;
    
    // Порог активации
    lv_obj_t *act_label = lv_label_create(screen);
    lv_label_set_text(act_label, "Активация:");
    lv_obj_set_style_text_color(act_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(act_label, &montserrat_ru, 0);
    lv_obj_align(act_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    encoder_value_config_t act_cfg = {
        .min_value = 0.01f,
        .max_value = 2.0f,
        .step = 0.01f,
        .initial_value = activation,
        .decimals = 2,
        .unit = NULL,
        .edit_color = lv_color_hex(0xFF9800),
    };
    g_activation_value = widget_encoder_value_create(screen, &act_cfg);
    lv_obj_set_size(g_activation_value, 90, 28);
    lv_obj_set_style_text_font(g_activation_value, &montserrat_ru, 0);
    lv_obj_align(g_activation_value, LV_ALIGN_TOP_RIGHT, -10, y_offset - 2);
    
    y_offset += 35;
    
    // Порог деактивации
    lv_obj_t *deact_label = lv_label_create(screen);
    lv_label_set_text(deact_label, "Деактивация:");
    lv_obj_set_style_text_color(deact_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(deact_label, &montserrat_ru, 0);
    lv_obj_align(deact_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    encoder_value_config_t deact_cfg = {
        .min_value = 0.01f,
        .max_value = 1.0f,
        .step = 0.01f,
        .initial_value = deactivation,
        .decimals = 2,
        .unit = NULL,
        .edit_color = lv_color_hex(0x4CAF50),
    };
    g_deactivation_value = widget_encoder_value_create(screen, &deact_cfg);
    lv_obj_set_size(g_deactivation_value, 90, 28);
    lv_obj_set_style_text_font(g_deactivation_value, &montserrat_ru, 0);
    lv_obj_align(g_deactivation_value, LV_ALIGN_TOP_RIGHT, -10, y_offset - 2);
    
    y_offset += 40;
    
    // Предупреждение
    g_warning_label = lv_label_create(screen);
    lv_label_set_text(g_warning_label, "");
    lv_obj_set_style_text_color(g_warning_label, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_warning_label, &montserrat_ru, 0);
    lv_label_set_long_mode(g_warning_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_warning_label, 200);
    lv_obj_align(g_warning_label, LV_ALIGN_TOP_MID, 0, y_offset);
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
