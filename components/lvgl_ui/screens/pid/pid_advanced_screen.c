/**
 * @file pid_advanced_screen.c
 * @brief Реализация экрана расширенных настроек PID
 */

#include "pid_advanced_screen.h"
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

static const char *TAG = "PID_ADVANCED_SCREEN";

static lv_obj_t *g_screen = NULL;
static pump_index_t g_pump_idx = PUMP_INDEX_PH_UP;

/* =============================
 *  CALLBACKS
 * ============================= */

static void on_thresholds_click(lv_event_t *e)
{
    ESP_LOGI(TAG, "Переход к настройке порогов");
    screen_show("pid_thresholds", (void*)(intptr_t)g_pump_idx);
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_advanced_screen_create(void *context)
{
    g_pump_idx = (pump_index_t)(intptr_t)context;
    
    ESP_LOGD(TAG, "Создание экрана расширенных настроек для насоса %d", g_pump_idx);
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create PID advanced screen");
        return NULL;
    }
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    g_screen = screen;
    
    // Status bar
    lv_obj_t *status_bar = widget_create_status_bar(screen, "Расширенные");
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // Заголовок
    lv_obj_t *title = lv_label_create(screen);
    char title_text[48];
    snprintf(title_text, sizeof(title_text), "Расширенные: %s", PUMP_NAMES[g_pump_idx]);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
    
    // TODO: Добавить редакторы для всех расширенных параметров
    // Пока создаем заглушку с основной информацией
    
    const system_config_t *config = config_manager_get_cached();
    
    lv_obj_t *info_label = lv_label_create(screen);
    char info_text[256];
    if (config) {
        snprintf(info_text, sizeof(info_text),
                 "Расширенные параметры:\n\n"
                 "Output: %.1f - %.1f мл\n"
                 "Deadband: %.2f\n"
                 "Integral max: %.1f\n"
                 "Sample time: %.0f мс\n"
                 "Max dose: %.1f мл\n"
                 "Cooldown: %lu мс\n"
                 "Daily limit: %lu мл",
                 config->pump_pid[g_pump_idx].output_min,
                 config->pump_pid[g_pump_idx].output_max,
                 config->pump_pid[g_pump_idx].deadband,
                 config->pump_pid[g_pump_idx].integral_max,
                 config->pump_pid[g_pump_idx].sample_time_ms,
                 config->pump_pid[g_pump_idx].max_dose_per_cycle,
                 (unsigned long)config->pump_pid[g_pump_idx].cooldown_time_ms,
                 (unsigned long)config->pump_pid[g_pump_idx].max_daily_volume);
    } else {
        snprintf(info_text, sizeof(info_text), "Конфигурация недоступна");
    }
    
    lv_label_set_text(info_label, info_text);
    lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
    lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 10, 70);
    
    // Кнопка "Настройка порогов"
    lv_obj_t *thresh_btn = lv_btn_create(screen);
    lv_obj_set_size(thresh_btn, 200, 40);
    lv_obj_align(thresh_btn, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_set_style_bg_color(thresh_btn, lv_color_hex(0xFF9800), 0);
    widget_add_click_handler(thresh_btn, on_thresholds_click, NULL);
    
    lv_obj_t *thresh_label = lv_label_create(thresh_btn);
    lv_label_set_text(thresh_label, "Настройка порогов");
    lv_obj_center(thresh_label);
    
    // Кнопка "Назад"
    lv_obj_t *back_btn = widget_create_back_button(screen, NULL, NULL);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    ESP_LOGD(TAG, "Экран расширенных настроек создан (заглушка)");
    
    return screen;
}

