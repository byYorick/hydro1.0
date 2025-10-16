/**
 * @file pid_intelligent_dashboard.c
 * @brief Реализация главного экрана интеллектуальной PID системы
 */

#include "pid_intelligent_dashboard.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/intelligent_pid_card.h"
#include "../../lvgl_styles.h"
#include "montserrat14_ru.h"
#include "adaptive_pid.h"
#include "pump_manager.h"
#include "pid_auto_tuner.h"
#include "../../widgets/event_helpers.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <math.h>

static const char *TAG = "PID_DASHBOARD";

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_prediction_panel = NULL;
static lv_obj_t *g_prediction_ph_label = NULL;
static lv_obj_t *g_prediction_ec_label = NULL;
static intelligent_pid_card_t *g_cards[PUMP_INDEX_COUNT] = {NULL};

// Задача обновления
static TaskHandle_t g_update_task = NULL;
static bool g_screen_active = false;

/*******************************************************************************
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/

/**
 * @brief Определение статуса PID для карточки
 */
static pid_card_status_t determine_status(pump_index_t pump_idx) {
    // Проверка автонастройки
    if (pid_auto_tuner_is_running(pump_idx)) {
        return PID_STATUS_AUTO_TUNING;
    }
    
    // Получение адаптивного состояния
    const adaptive_pid_state_t *state = adaptive_pid_get_state(pump_idx);
    if (!state) {
        return PID_STATUS_IDLE;
    }
    
    // Безопасный режим
    if (state->safe_mode) {
        return PID_STATUS_IDLE;
    }
    
    // Режим обучения
    if (state->learning_mode && state->total_corrections < MIN_CORRECTIONS_FOR_LEARNING) {
        return PID_STATUS_LEARNING;
    }
    
    // Упреждающая коррекция
    if (state->prediction_enabled) {
        prediction_result_t pred;
        float current = 7.0f; // TODO: Получить реальное значение
        float target = 6.5f;
        
        if (adaptive_pid_predict(pump_idx, current, target, &pred) == ESP_OK) {
            if (pred.needs_preemptive_correction) {
                return PID_STATUS_PREDICTING;
            }
        }
    }
    
    // Проверка достижения цели (deadband)
    // TODO: Получить реальные значения и проверить
    
    return PID_STATUS_ACTIVE; // По умолчанию активен
}

/**
 * @brief Обновление секции прогноза системы
 */
static void update_prediction_panel(void) {
    if (!g_prediction_panel) return;
    
    // Получение прогнозов для pH (компактный формат)
    const adaptive_pid_state_t *ph_state = adaptive_pid_get_state(PUMP_INDEX_PH_DOWN);
    if (ph_state && ph_state->prediction_enabled) {
        char ph_text[32];
        snprintf(ph_text, sizeof(ph_text), "pH:%.1f→%.1f",
                 7.2f, ph_state->predicted_value_1h); // TODO: реальные значения
        lv_label_set_text(g_prediction_ph_label, ph_text);
    } else {
        lv_label_set_text(g_prediction_ph_label, "pH:-");
    }
    
    // Получение прогнозов для EC (компактный формат)
    const adaptive_pid_state_t *ec_state = adaptive_pid_get_state(PUMP_INDEX_EC_A);
    if (ec_state && ec_state->prediction_enabled) {
        char ec_text[32];
        snprintf(ec_text, sizeof(ec_text), "EC:%.1f→%.1f",
                 1.5f, ec_state->predicted_value_1h); // TODO: реальные значения
        lv_label_set_text(g_prediction_ec_label, ec_text);
    } else {
        lv_label_set_text(g_prediction_ec_label, "EC:-");
    }
}

/**
 * @brief Задача обновления dashboard в реальном времени
 * 
 * ВАЖНО: Обновление LVGL объектов ТОЛЬКО с блокировкой lv_lock()!
 */
static void dashboard_update_task(void *arg) {
    const TickType_t UPDATE_INTERVAL = pdMS_TO_TICKS(2000); // 2 сек (медленнее для стабильности)
    
    ESP_LOGI(TAG, "Задача обновления dashboard запущена");
    
    // Диагностика стека при старте
    UBaseType_t stack_start = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Стек задачи dashboard: %lu байт свободно", (unsigned long)stack_start * 4);
    
    while (g_screen_active) {
        // КРИТИЧНО: Сброс watchdog для предотвращения timeout
        esp_task_wdt_reset();
        
        // КРИТИЧНО: Блокировка LVGL перед обновлением UI
        lv_lock();
        
        // Обновление всех карточек
        for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
            // Сброс watchdog внутри цикла (для каждой карточки)
            esp_task_wdt_reset();
            
            if (g_cards[i]) {
                // Получение данных PID
                pid_output_t output;
                float current = (i < 2) ? 7.0f : 1.5f; // TODO: реальные значения датчиков
                float target = (i < 2) ? 6.5f : 1.8f;
                
                if (pump_manager_compute_pid(i, current, target, &output) == ESP_OK) {
                    // Обновление карточки
                    widget_intelligent_pid_card_update(
                        g_cards[i],
                        current,
                        target,
                        output.p_term,
                        output.i_term,
                        output.d_term
                    );
                    
                    // Определение и установка статуса
                    pid_card_status_t status = determine_status(i);
                    widget_intelligent_pid_card_set_status(g_cards[i], status);
                }
            }
        }
        
        // Обновление секции прогноза
        update_prediction_panel();
        
        // КРИТИЧНО: Разблокировка LVGL
        lv_unlock();
        
        vTaskDelay(UPDATE_INTERVAL);
    }
    
    // Диагностика стека при завершении
    UBaseType_t stack_end = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Задача обновления dashboard завершена. Минимальный свободный стек: %lu байт", 
             (unsigned long)stack_end * 4);
    
    vTaskDelete(NULL);
}

/**
 * @brief Обработчик клика на карточку
 */
static void on_card_click(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
        ESP_LOGI(TAG, "Клик на карточку насоса %d", pump_idx);
        
        // Переход к детальному экрану
        screen_show("pid_intelligent_detail", (void*)(intptr_t)pump_idx);
    }
}

/*******************************************************************************
 * API РЕАЛИЗАЦИЯ
 ******************************************************************************/

lv_obj_t* pid_intelligent_dashboard_create(void *context) {
    (void)context;
    ESP_LOGI(TAG, "Создание интеллектуального PID dashboard");
    
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
    widget_create_status_bar(screen, "Интеллектуальный PID");
    
    // Back button
    widget_create_back_button(screen, NULL, NULL);
    
    // Контейнер для содержимого
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(100), 270);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 3, 0);  // Минимальные отступы
    lv_obj_set_style_pad_row(content, 1, 0);  // Минимальные отступы между карточками
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    
    // Секция прогноза системы (компактно)
    g_prediction_panel = lv_obj_create(content);
    lv_obj_set_size(g_prediction_panel, LV_PCT(100), 24);  // Супер компактно
    lv_obj_set_style_bg_color(g_prediction_panel, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_radius(g_prediction_panel, 4, 0);
    lv_obj_set_style_pad_all(g_prediction_panel, 2, 0);  // Меньше отступы
    lv_obj_set_flex_flow(g_prediction_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_prediction_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    
    // Прогноз системы в одну строку (компактно)
    lv_obj_t *pred_row = lv_obj_create(g_prediction_panel);
    lv_obj_remove_style_all(pred_row);
    lv_obj_set_size(pred_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pred_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pred_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Прогноз pH (короткий текст)
    g_prediction_ph_label = lv_label_create(pred_row);
    lv_label_set_text(g_prediction_ph_label, "pH:-");
    lv_obj_set_style_text_color(g_prediction_ph_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(g_prediction_ph_label, &lv_font_montserrat_10, 0);
    
    // Прогноз EC (короткий текст)
    g_prediction_ec_label = lv_label_create(pred_row);
    lv_label_set_text(g_prediction_ec_label, "EC:-");
    lv_obj_set_style_text_color(g_prediction_ec_label, lv_color_hex(0xffaa00), 0);
    lv_obj_set_style_text_font(g_prediction_ec_label, &lv_font_montserrat_10, 0);
    
    // Создание карточек для всех 6 насосов
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // КРИТИЧНО: Feed watchdog при создании каждого виджета
        esp_task_wdt_reset();
        
        g_cards[i] = widget_intelligent_pid_card_create(content, i);
        
        if (g_cards[i]) {
            // КРИТИЧНО: Используем widget_add_click_handler для правильной обработки KEY_ENTER
            widget_add_click_handler(g_cards[i]->container, on_card_click, (void*)(intptr_t)i);
            
            // Фокус управляется автоматически через screen_manager, не устанавливаем вручную
        }
    }
    
    ESP_LOGI(TAG, "Dashboard создан с %d карточками", PUMP_INDEX_COUNT);
    
    return screen;
}

esp_err_t pid_intelligent_dashboard_on_show(lv_obj_t *screen, void *params) {
    (void)screen;
    (void)params;
    
    ESP_LOGI(TAG, "Dashboard показан, запуск задачи обновления");
    
    g_screen_active = true;
    
    // Создание задачи обновления
    BaseType_t result = xTaskCreate(
        dashboard_update_task,
        "pid_dash_upd",
        4096,  // 16KB стек (увеличено с 12KB для безопасности)
        NULL,
        5,     // Средний приоритет
        &g_update_task
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу обновления");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t pid_intelligent_dashboard_on_hide(lv_obj_t *screen) {
    (void)screen;
    
    ESP_LOGI(TAG, "Dashboard скрыт, остановка задачи обновления");
    
    g_screen_active = false;
    
    // Подождать завершения задачи
    if (g_update_task) {
        vTaskDelay(pdMS_TO_TICKS(600)); // Подождать немного
        g_update_task = NULL;
    }
    
    // КРИТИЧНО: Очищаем указатели на карточки (память освободится через LVGL delete callbacks)
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        g_cards[i] = NULL;
    }
    
    // Очищаем указатели на UI элементы
    g_screen = NULL;
    g_prediction_panel = NULL;
    g_prediction_ph_label = NULL;
    g_prediction_ec_label = NULL;
    
    return ESP_OK;
}

esp_err_t pid_intelligent_dashboard_update(void) {
    // Обновление вызывается задачей, здесь можно оставить пустым
    return ESP_OK;
}

