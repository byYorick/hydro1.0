/**
 * @file pid_tuning_screen.c
 * @brief Реализация экрана настройки базовых параметров PID с управлением энкодером
 */

#include "pid_tuning_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/encoder_value_edit.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_TUNING_SCREEN";

static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_kp_value = NULL;
static lv_obj_t *g_ki_value = NULL;
static lv_obj_t *g_kd_value = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  CALLBACKS
 * ============================= */

static void on_save_click(lv_event_t *e)
{
    // Получение значений из виджетов
    float kp = widget_encoder_value_get(g_kp_value);
    float ki = widget_encoder_value_get(g_ki_value);
    float kd = widget_encoder_value_get(g_kd_value);
    
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
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Настройки PID успешно сохранены");
        } else {
            ESP_LOGE(TAG, "Ошибка сохранения настроек PID");
        }
    }
}

static void on_defaults_click(lv_event_t *e)
{
    // Дефолтные значения
    float kp = 1.0f;
    float ki = 0.1f;
    float kd = 0.0f;
    
    // Установка значений в виджеты
    widget_encoder_value_set(g_kp_value, kp);
    widget_encoder_value_set(g_ki_value, ki);
    widget_encoder_value_set(g_kd_value, kd);
    
    ESP_LOGI(TAG, "Восстановлены дефолтные значения PID");
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_tuning_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGI(TAG, "Создание экрана настройки PID для %s", PUMP_NAMES[g_pump_idx]);
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "PID Tuning");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "PID: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &montserrat_ru, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // Загрузка текущих значений
    const system_config_t *config = config_manager_get_cached();
    float kp = config ? config->pump_pid[g_pump_idx].kp : 1.0f;
    float ki = config ? config->pump_pid[g_pump_idx].ki : 0.1f;
    float kd = config ? config->pump_pid[g_pump_idx].kd : 0.0f;
    
    int y_offset = 65;
    int row_height = 35;
    
    // Kp (Пропорциональный)
    lv_obj_t *kp_label = lv_label_create(screen);
    lv_label_set_text(kp_label, "Kp:");
    lv_obj_set_style_text_color(kp_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(kp_label, &montserrat_ru, 0);
    lv_obj_align(kp_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    encoder_value_config_t kp_cfg = {
        .min_value = 0.0f,
        .max_value = 10.0f,
        .step = 0.1f,
        .initial_value = kp,
        .decimals = 2,
        .unit = NULL,
        .edit_color = lv_color_hex(0x2196F3),
    };
    g_kp_value = widget_encoder_value_create(screen, &kp_cfg);
    lv_obj_set_size(g_kp_value, 90, 28);
    lv_obj_set_style_text_font(g_kp_value, &montserrat_ru, 0);
    lv_obj_align(g_kp_value, LV_ALIGN_TOP_RIGHT, -10, y_offset - 2);
    
    y_offset += row_height;
    
    // Ki (Интегральный)
    lv_obj_t *ki_label = lv_label_create(screen);
    lv_label_set_text(ki_label, "Ki:");
    lv_obj_set_style_text_color(ki_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ki_label, &montserrat_ru, 0);
    lv_obj_align(ki_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    encoder_value_config_t ki_cfg = {
        .min_value = 0.0f,
        .max_value = 5.0f,
        .step = 0.01f,
        .initial_value = ki,
        .decimals = 2,
        .unit = NULL,
        .edit_color = lv_color_hex(0x4CAF50),
    };
    g_ki_value = widget_encoder_value_create(screen, &ki_cfg);
    lv_obj_set_size(g_ki_value, 90, 28);
    lv_obj_set_style_text_font(g_ki_value, &montserrat_ru, 0);
    lv_obj_align(g_ki_value, LV_ALIGN_TOP_RIGHT, -10, y_offset - 2);
    
    y_offset += row_height;
    
    // Kd (Дифференциальный)
    lv_obj_t *kd_label = lv_label_create(screen);
    lv_label_set_text(kd_label, "Kd:");
    lv_obj_set_style_text_color(kd_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(kd_label, &montserrat_ru, 0);
    lv_obj_align(kd_label, LV_ALIGN_TOP_LEFT, 10, y_offset);
    
    encoder_value_config_t kd_cfg = {
        .min_value = 0.0f,
        .max_value = 2.0f,
        .step = 0.01f,
        .initial_value = kd,
        .decimals = 2,
        .unit = NULL,
        .edit_color = lv_color_hex(0xFF9800),
    };
    g_kd_value = widget_encoder_value_create(screen, &kd_cfg);
    lv_obj_set_size(g_kd_value, 90, 28);
    lv_obj_set_style_text_font(g_kd_value, &montserrat_ru, 0);
    lv_obj_align(g_kd_value, LV_ALIGN_TOP_RIGHT, -10, y_offset - 2);
    
    y_offset += row_height + 15;
    
    // Подсказка
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Нажмите Enter для изменения\nПоверните для настройки");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(hint, &montserrat_ru, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, 200);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, y_offset);
    
    y_offset += 50;
    
    // Кнопки
    lv_obj_t *save_btn = lv_btn_create(screen);
    lv_obj_set_size(save_btn, 100, 35);
    lv_obj_align(save_btn, LV_ALIGN_TOP_LEFT, 10, y_offset);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(save_btn, on_save_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(save_btn, on_save_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Сохранить");
    lv_obj_center(save_label);
    
    lv_obj_t *defaults_btn = lv_btn_create(screen);
    lv_obj_set_size(defaults_btn, 100, 35);
    lv_obj_align(defaults_btn, LV_ALIGN_TOP_RIGHT, -10, y_offset);
    lv_obj_set_style_bg_color(defaults_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_add_event_cb(defaults_btn, on_defaults_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(defaults_btn, on_defaults_click, LV_EVENT_PRESSED, NULL);
    
    lv_obj_t *defaults_label = lv_label_create(defaults_btn);
    lv_label_set_text(defaults_label, "Дефолт");
    lv_obj_center(defaults_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGI(TAG, "Экран настройки PID создан");
    
    return screen;
}
