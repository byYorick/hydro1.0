/**
 * @file intelligent_pid_card.c
 * @brief Реализация виджета адаптивной карточки PID
 */

#include "intelligent_pid_card.h"
#include "lvgl_styles.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "PID_CARD";

/**
 * @brief Callback для освобождения памяти при удалении карточки
 * КРИТИЧНО: Предотвращает утечку памяти!
 */
static void pid_card_delete_cb(lv_event_t *e) {
    lv_obj_t *card_obj = lv_event_get_target(e);
    intelligent_pid_card_t *card = (intelligent_pid_card_t*)lv_obj_get_user_data(card_obj);
    
    if (card) {
        ESP_LOGD(TAG, "Освобождение памяти карточки PID для насоса %d", card->pump_idx);
        free(card);
        lv_obj_set_user_data(card_obj, NULL);
    }
}

intelligent_pid_card_t* widget_intelligent_pid_card_create(lv_obj_t *parent, pump_index_t pump_idx) {
    if (!parent || pump_idx >= PUMP_INDEX_COUNT) {
        return NULL;
    }
    
    // Выделение памяти для структуры
    intelligent_pid_card_t *card = (intelligent_pid_card_t *)malloc(sizeof(intelligent_pid_card_t));
    if (!card) {
        ESP_LOGE(TAG, "Не удалось выделить память для карточки");
        return NULL;
    }
    
    memset(card, 0, sizeof(intelligent_pid_card_t));
    card->pump_idx = pump_idx;
    card->status = PID_STATUS_IDLE;
    card->is_focused = false;
    
    // КРИТИЧНО: Создание КНОПКИ для правильной обработки KEY_ENTER и фокуса
    card->container = lv_btn_create(parent);
    if (!card->container) {
        free(card);
        return NULL;
    }
    
    // КРИТИЧНО: Сохраняем указатель и добавляем обработчик удаления для предотвращения утечки памяти
    lv_obj_set_user_data(card->container, card);
    lv_obj_add_event_cb(card->container, pid_card_delete_cb, LV_EVENT_DELETE, NULL);
    
    lv_obj_set_size(card->container, LV_PCT(100), 38); // Высота 38px (супер компактно)
    lv_obj_add_style(card->container, &style_pid_card, 0);
    
    // Применяем стиль фокуса для энкодера
    extern lv_style_t style_card_focused;
    lv_obj_add_style(card->container, &style_card_focused, LV_STATE_FOCUSED);
    
    lv_obj_set_flex_flow(card->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card->container, 4, 0);  // Уменьшено с 6 до 4
    lv_obj_set_style_pad_row(card->container, 1, 0);  // Уменьшено с 2 до 1
    
    // Первая строка: Имя + статус
    lv_obj_t *top_row = lv_obj_create(card->container);
    lv_obj_remove_style_all(top_row);
    lv_obj_set_size(top_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Имя насоса
    card->name_label = lv_label_create(top_row);
    lv_label_set_text(card->name_label, PUMP_NAMES[pump_idx]);
    lv_obj_set_style_text_color(card->name_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(card->name_label, &montserrat_ru, 0);
    
    // Индикатор ON/OFF
    card->status_indicator = lv_label_create(top_row);
    lv_label_set_text(card->status_indicator, "OFF");
    lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0x888888), 0);
    
    // Вторая строка: Текущее → Целевое + Прогресс
    lv_obj_t *values_row = lv_obj_create(card->container);
    lv_obj_remove_style_all(values_row);
    lv_obj_set_size(values_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(values_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(values_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Значения "7.2 → 6.5"
    card->values_label = lv_label_create(values_row);
    lv_label_set_text(card->values_label, "- -> -");
    lv_obj_set_style_text_color(card->values_label, lv_color_hex(0xcccccc), 0);
    
    // Прогресс-бар
    card->progress_bar = lv_bar_create(values_row);
    lv_obj_set_size(card->progress_bar, 60, 8);
    lv_obj_add_style(card->progress_bar, &style_progress_bg, LV_PART_MAIN);
    lv_obj_add_style(card->progress_bar, &style_progress_indicator, LV_PART_INDICATOR);
    lv_bar_set_value(card->progress_bar, 0, LV_ANIM_OFF);
    
    // Третья строка: PID компоненты + тренд (всё в одну строку для компактности)
    lv_obj_t *bottom_row = lv_obj_create(card->container);
    lv_obj_remove_style_all(bottom_row);
    lv_obj_set_size(bottom_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // PID компоненты (короткий формат)
    card->pid_components_label = lv_label_create(bottom_row);
    lv_label_set_text(card->pid_components_label, "P:- I:- D:-");
    lv_obj_set_style_text_color(card->pid_components_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(card->pid_components_label, &lv_font_montserrat_10, 0);
    
    // Адаптивные коэффициенты (скрыты, обновляются динамически)
    card->adaptive_label = lv_label_create(bottom_row);
    lv_label_set_text(card->adaptive_label, "Kp:-");
    lv_obj_set_style_text_color(card->adaptive_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(card->adaptive_label, &lv_font_montserrat_10, 0);
    
    // Тренд (символ)
    card->trend_label = lv_label_create(bottom_row);
    lv_label_set_text(card->trend_label, "-");
    lv_obj_set_style_text_color(card->trend_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(card->trend_label, &lv_font_montserrat_10, 0);
    
    ESP_LOGD(TAG, "Карточка PID создана для насоса %d", pump_idx);
    
    return card;
}

void widget_intelligent_pid_card_update(intelligent_pid_card_t *card,
                                         float current,
                                         float target,
                                         float p_term,
                                         float i_term,
                                         float d_term) {
    if (!card) return;
    
    // Обновление значений
    char values_text[32];
    snprintf(values_text, sizeof(values_text), "%.1f -> %.1f", current, target);
    lv_label_set_text(card->values_label, values_text);
    
    // Обновление PID компонентов
    char pid_text[48];
    snprintf(pid_text, sizeof(pid_text), "P:%+.1f I:%+.1f D:%+.1f", p_term, i_term, d_term);
    lv_label_set_text(card->pid_components_label, pid_text);
    
    // Расчет прогресса до цели (0-100%)
    float error = fabsf(current - target);
    float max_error = (card->pump_idx < 2) ? 2.0f : 1.0f; // pH: 2.0, EC: 1.0
    float progress = 100.0f * (1.0f - fminf(error / max_error, 1.0f));
    lv_bar_set_value(card->progress_bar, (int32_t)progress, LV_ANIM_ON);
    
    // Получение адаптивного состояния
    const adaptive_pid_state_t *state = adaptive_pid_get_state(card->pump_idx);
    if (state) {
        // Обновление адаптивных параметров
        char adaptive_text[32];
        
        // Показываем изменение коэффициента
        float kp_base = (card->pump_idx < 2) ? 2.0f : 1.5f;
        const char *kp_change = "";
        if (state->kp_adaptive > kp_base * 1.05f) {
            kp_change = "(↑)";
        } else if (state->kp_adaptive < kp_base * 0.95f) {
            kp_change = "(↓)";
        }
        
        snprintf(adaptive_text, sizeof(adaptive_text), "Kp:%.1f%s Ki:%.1f", 
                 state->kp_adaptive, kp_change, state->ki_adaptive);
        lv_label_set_text(card->adaptive_label, adaptive_text);
        
        // Обновление тренда
        const char *trend_text;
        if (fabsf(state->trend_slope) < 0.01f) {
            trend_text = "Стабильно";
        } else if (state->trend_slope > 0) {
            trend_text = "Растет";
        } else {
            trend_text = "Падает";
        }
        lv_label_set_text(card->trend_label, trend_text);
    }
}

void widget_intelligent_pid_card_set_status(intelligent_pid_card_t *card, pid_card_status_t status) {
    if (!card) return;
    
    card->status = status;
    
    // Очистка всех стилей статуса
    lv_obj_remove_style(card->container, &style_pid_active, 0);
    lv_obj_remove_style(card->container, &style_pid_idle, 0);
    lv_obj_remove_style(card->container, &style_pid_learning, 0);
    lv_obj_remove_style(card->container, &style_pid_predicting, 0);
    lv_obj_remove_style(card->container, &style_pid_tuning, 0);
    lv_obj_remove_style(card->container, &style_pid_target, 0);
    lv_obj_remove_style(card->container, &style_pid_error, 0);
    
    // Применение соответствующего стиля
    const char *status_text;
    
    switch (status) {
        case PID_STATUS_ACTIVE:
            lv_obj_add_style(card->container, &style_pid_active, 0);
            status_text = "ON";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0xFFC107), 0);
            break;
            
        case PID_STATUS_LEARNING:
            lv_obj_add_style(card->container, &style_pid_learning, 0);
            status_text = "ON";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0x2196F3), 0);
            break;
            
        case PID_STATUS_PREDICTING:
            lv_obj_add_style(card->container, &style_pid_predicting, 0);
            status_text = "ON";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0x9C27B0), 0);
            break;
            
        case PID_STATUS_AUTO_TUNING:
            lv_obj_add_style(card->container, &style_pid_tuning, 0);
            status_text = "TUNE";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0xFF9800), 0);
            break;
            
        case PID_STATUS_TARGET_REACHED:
            lv_obj_add_style(card->container, &style_pid_target, 0);
            status_text = "OK";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0x4CAF50), 0);
            break;
            
        case PID_STATUS_ERROR:
            lv_obj_add_style(card->container, &style_pid_error, 0);
            status_text = "ERR";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0xF44336), 0);
            break;
            
        case PID_STATUS_IDLE:
        default:
            lv_obj_add_style(card->container, &style_pid_idle, 0);
            status_text = "OFF";
            lv_obj_set_style_text_color(card->status_indicator, lv_color_hex(0x888888), 0);
            break;
    }
    
    lv_label_set_text(card->status_indicator, status_text);
}

void widget_intelligent_pid_card_set_focused(intelligent_pid_card_t *card, bool focused) {
    if (!card) return;
    
    card->is_focused = focused;
    
    if (focused) {
        // Добавить стиль фокуса
        lv_obj_add_style(card->container, &style_focus, 0);
    } else {
        // Убрать стиль фокуса
        lv_obj_remove_style(card->container, &style_focus, 0);
    }
}

void widget_intelligent_pid_card_delete(intelligent_pid_card_t *card) {
    if (!card) return;
    
    if (card->container) {
        lv_obj_del(card->container);
    }
    
    free(card);
}

