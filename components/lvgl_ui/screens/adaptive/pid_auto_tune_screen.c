/**
 * @file pid_auto_tune_screen.c
 * @brief Реализация экрана автонастройки PID (Ziegler-Nichols)
 */

#include "pid_auto_tune_screen.h"
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "PID_AUTOTUNE";

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_pump_dropdown = NULL;
static lv_obj_t *g_start_btn = NULL;
static lv_obj_t *g_stop_btn = NULL;
static lv_obj_t *g_apply_btn = NULL;
static lv_obj_t *g_status_label = NULL;
static lv_obj_t *g_progress_label = NULL;
static lv_obj_t *g_result_label = NULL;

// Задача обновления
static TaskHandle_t g_update_task = NULL;
static bool g_screen_active = false;
static pump_index_t g_selected_pump = PUMP_INDEX_PH_DOWN;

/*******************************************************************************
 * CALLBACKS
 ******************************************************************************/

/**
 * @brief Callback выбора насоса из dropdown
 */
static void on_pump_selected(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    g_selected_pump = (pump_index_t)selected;
    
    ESP_LOGI(TAG, "Выбран насос: %s", PUMP_NAMES[g_selected_pump]);
    
    // Обновление кнопок
    bool is_running = pid_auto_tuner_is_running(g_selected_pump);
    if (is_running) {
        lv_obj_add_state(g_start_btn, LV_STATE_DISABLED);
        lv_obj_remove_state(g_stop_btn, LV_STATE_DISABLED);
        lv_obj_add_state(g_apply_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(g_start_btn, LV_STATE_DISABLED);
        lv_obj_add_state(g_stop_btn, LV_STATE_DISABLED);
        // Кнопка применения активна только если есть результат
        // TODO: проверить наличие результата
    }
}

/**
 * @brief Callback кнопки "Старт"
 */
static void on_start_click(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    ESP_LOGI(TAG, "Запуск автонастройки для %s", PUMP_NAMES[g_selected_pump]);
    
    // Запуск автонастройки (метод Relay)
    tuning_method_t method = TUNING_METHOD_RELAY;
    if (pid_auto_tuner_start(g_selected_pump, method) == ESP_OK) {
        lv_obj_add_state(g_start_btn, LV_STATE_DISABLED);
        lv_obj_remove_state(g_stop_btn, LV_STATE_DISABLED);
        lv_label_set_text(g_status_label, "Статус: Идет настройка...");
    } else {
        ESP_LOGE(TAG, "Не удалось запустить автонастройку");
        lv_label_set_text(g_status_label, "Статус: Ошибка запуска!");
    }
}

/**
 * @brief Callback кнопки "Стоп"
 */
static void on_stop_click(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    ESP_LOGI(TAG, "Остановка автонастройки для %s", PUMP_NAMES[g_selected_pump]);
    
    if (pid_auto_tuner_cancel(g_selected_pump) == ESP_OK) {
        lv_obj_remove_state(g_start_btn, LV_STATE_DISABLED);
        lv_obj_add_state(g_stop_btn, LV_STATE_DISABLED);
        lv_label_set_text(g_status_label, "Статус: Остановлено");
    } else {
        ESP_LOGE(TAG, "Не удалось остановить автонастройку");
    }
}

/**
 * @brief Callback кнопки "Применить"
 */
static void on_apply_click(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) return;
    
    ESP_LOGI(TAG, "Применение результатов автонастройки для %s", PUMP_NAMES[g_selected_pump]);
    
    if (pid_auto_tuner_apply_result(g_selected_pump) == ESP_OK) {
        lv_label_set_text(g_status_label, "Статус: Применено успешно!");
        lv_obj_add_state(g_apply_btn, LV_STATE_DISABLED);
    } else {
        ESP_LOGE(TAG, "Не удалось применить результаты");
        lv_label_set_text(g_status_label, "Статус: Ошибка применения!");
    }
}

/**
 * @brief Задача обновления статуса автонастройки
 */
static void autotune_update_task(void *arg) {
    const TickType_t UPDATE_INTERVAL = pdMS_TO_TICKS(500); // 0.5 сек
    
    ESP_LOGI(TAG, "Задача обновления автонастройки запущена");
    
    // Диагностика стека
    UBaseType_t stack_start = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Стек задачи autotune: %lu байт свободно", (unsigned long)stack_start * 4);
    
    while (g_screen_active) {
        // КРИТИЧНО: Сброс watchdog для предотвращения timeout
        esp_task_wdt_reset();
        
        lv_lock();
        
        bool is_running = pid_auto_tuner_is_running(g_selected_pump);
        
        if (is_running) {
            // Обновление прогресса
            tuning_result_t result;
            if (pid_auto_tuner_get_result(g_selected_pump, &result) == ESP_OK) {
                char progress_text[128];
                snprintf(progress_text, sizeof(progress_text),
                         "Прогресс: %d%%\nОсцилляций: %d\nПериод: %.1f сек",
                         result.progress_percent,
                         result.oscillations_detected,
                         result.ultimate_period_sec);
                lv_label_set_text(g_progress_label, progress_text);
            }
        } else {
            // Проверка результатов
            tuning_result_t result;
            if (pid_auto_tuner_get_result(g_selected_pump, &result) == ESP_OK) {
                if (result.tuning_successful) {
                    char result_text[128];
                    snprintf(result_text, sizeof(result_text),
                             "Результат:\nKp = %.3f\nKi = %.3f\nKd = %.3f",
                             result.kp_calculated, result.ki_calculated, result.kd_calculated);
                    lv_label_set_text(g_result_label, result_text);
                    lv_label_set_text(g_status_label, result.status_message);
                    lv_obj_remove_state(g_apply_btn, LV_STATE_DISABLED);
                } else {
                    lv_label_set_text(g_status_label, result.status_message);
                    lv_label_set_text(g_result_label, result.error_message);
                }
            }
        }
        
        lv_unlock();
        
        vTaskDelay(UPDATE_INTERVAL);
    }
    
    // Диагностика стека при завершении
    UBaseType_t stack_end = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Задача обновления автонастройки завершена. Минимальный свободный стек: %lu байт", 
             (unsigned long)stack_end * 4);
    
    vTaskDelete(NULL);
}

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

lv_obj_t* pid_auto_tune_screen_create(void *params) {
    pump_index_t pump_idx = PUMP_INDEX_PH_DOWN;
    if (params) {
        pump_idx = (pump_index_t)(intptr_t)params;
        if (pump_idx >= PUMP_INDEX_COUNT) pump_idx = PUMP_INDEX_PH_DOWN;
    }
    g_selected_pump = pump_idx;
    
    ESP_LOGI(TAG, "Создание экрана автонастройки для %s", PUMP_NAMES[pump_idx]);
    
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
    
    // Status bar
    widget_create_status_bar(screen, "Автонастройка PID");
    
    // Back button
    widget_create_back_button(screen, NULL, NULL);
    
    // Контейнер содержимого
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(100), 270);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    
    esp_task_wdt_reset();
    
    // Dropdown выбора насоса
    lv_obj_t *pump_label = lv_label_create(content);
    lv_label_set_text(pump_label, "Выберите насос:");
    lv_obj_set_style_text_color(pump_label, lv_color_white(), 0);
    
    g_pump_dropdown = lv_dropdown_create(content);
    lv_obj_set_width(g_pump_dropdown, LV_PCT(90));
    lv_dropdown_set_options(g_pump_dropdown, 
        "pH▼ (кислота)\npH▲ (щелочь)\nEC▼ (вода)\nEC▲ (A)\nEC (B)\nEC (C)");
    lv_dropdown_set_selected(g_pump_dropdown, pump_idx);
    lv_obj_add_event_cb(g_pump_dropdown, on_pump_selected, LV_EVENT_VALUE_CHANGED, NULL);
    
    esp_task_wdt_reset();
    
    // Кнопки управления
    lv_obj_t *btn_row = lv_obj_create(content);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    g_start_btn = lv_btn_create(btn_row);
    lv_obj_set_width(g_start_btn, 100);
    lv_obj_t *start_label = lv_label_create(g_start_btn);
    lv_label_set_text(start_label, "Старт");
    lv_obj_center(start_label);
    widget_add_click_handler(g_start_btn, on_start_click, NULL);
    
    g_stop_btn = lv_btn_create(btn_row);
    lv_obj_set_width(g_stop_btn, 100);
    lv_obj_add_state(g_stop_btn, LV_STATE_DISABLED);
    lv_obj_t *stop_label = lv_label_create(g_stop_btn);
    lv_label_set_text(stop_label, "Стоп");
    lv_obj_center(stop_label);
    widget_add_click_handler(g_stop_btn, on_stop_click, NULL);
    
    esp_task_wdt_reset();
    
    // Статус
    g_status_label = lv_label_create(content);
    lv_label_set_text(g_status_label, "Статус: Готов к настройке");
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x00D4AA), 0);
    
    // Прогресс
    g_progress_label = lv_label_create(content);
    lv_label_set_text(g_progress_label, "Прогресс:\nОсцилляций: 0/4");
    lv_obj_set_style_text_color(g_progress_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(g_progress_label, &lv_font_montserrat_10, 0);
    
    // Результат
    g_result_label = lv_label_create(content);
    lv_label_set_text(g_result_label, "Результат:\n-");
    lv_obj_set_style_text_color(g_result_label, lv_color_white(), 0);
    
    // Кнопка применения
    g_apply_btn = lv_btn_create(content);
    lv_obj_set_width(g_apply_btn, LV_PCT(90));
    lv_obj_add_state(g_apply_btn, LV_STATE_DISABLED);
    lv_obj_t *apply_label = lv_label_create(g_apply_btn);
    lv_label_set_text(apply_label, "Применить результаты");
    lv_obj_center(apply_label);
    widget_add_click_handler(g_apply_btn, on_apply_click, NULL);
    
    esp_task_wdt_reset();
    
    ESP_LOGI(TAG, "Экран автонастройки создан успешно");
    
    return screen;
}

esp_err_t pid_auto_tune_screen_on_show(lv_obj_t *screen, void *params) {
    (void)screen;
    
    if (params) {
        pump_index_t pump_idx = (pump_index_t)(intptr_t)params;
        if (pump_idx < PUMP_INDEX_COUNT) {
            g_selected_pump = pump_idx;
            lv_dropdown_set_selected(g_pump_dropdown, pump_idx);
        }
    }
    
    ESP_LOGI(TAG, "Экран автонастройки показан для %s", PUMP_NAMES[g_selected_pump]);
    
    // Запуск задачи обновления
    g_screen_active = true;
    xTaskCreate(autotune_update_task, "autotune_upd", 4096, NULL, 5, &g_update_task);  // 16KB стек
    
    return ESP_OK;
}

esp_err_t pid_auto_tune_screen_on_hide(lv_obj_t *screen) {
    (void)screen;
    
    ESP_LOGI(TAG, "Экран автонастройки скрыт");
    
    // Остановка задачи обновления
    g_screen_active = false;
    if (g_update_task) {
        // Задача сама удалится
        g_update_task = NULL;
    }
    
    // Очистка указателей
    g_pump_dropdown = NULL;
    g_start_btn = NULL;
    g_stop_btn = NULL;
    g_apply_btn = NULL;
    g_status_label = NULL;
    g_progress_label = NULL;
    g_result_label = NULL;
    g_screen = NULL;
    
    return ESP_OK;
}

