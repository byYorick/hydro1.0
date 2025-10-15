/**
 * @file pumps_status_screen.c
 * @brief Реализация экрана статуса насосов
 */

#include "pumps_status_screen.h"
#include "screen_manager/screen_manager.h"
#include "../base/screen_template.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/event_helpers.h"
#include "pump_manager.h"
#include "system_config.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdio.h>

static const char *TAG = "PUMPS_STATUS_SCREEN";

// UI элементы
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_pump_labels[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_status_labels[PUMP_INDEX_COUNT] = {NULL};
static lv_obj_t *g_stats_labels[PUMP_INDEX_COUNT] = {NULL};

/* =============================
 *  CALLBACKS
 * ============================= */

// Callbacks для нижних кнопок убраны - функции доступны через меню

/**
 * @brief Callback для клика на насос
 */
static void on_pump_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем и клик мышью и нажатие Enter от энкодера
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
        ESP_LOGI(TAG, "Клик на насос %d (event: %d)", pump_idx, code);
        
        // Переход к детальному экрану интеллектуального адаптивного PID
        screen_show("pid_intelligent_detail", (void*)(intptr_t)pump_idx);
    }
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pumps_status_screen_create(void *context)
{
    ESP_LOGD(TAG, "Создание экрана статуса насосов");
    
    // Используем новые компактные стили
    extern lv_style_t style_bg;
    extern lv_style_t style_card;
    extern lv_style_t style_label;
    
    // Создание контейнера экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_add_style(screen, &style_bg, 0);
    lv_obj_set_style_pad_all(screen, 8, 0);  // Компактные отступы
    g_screen = screen;
    
    // Status bar (компактный 30px)
    widget_create_status_bar(screen, "Статус насосов");
    
    // Back button
    widget_create_back_button(screen, NULL, NULL);
    
    // Контейнер для списка насосов (занимает всё доступное место под хедером)
    lv_obj_t *list_container = lv_obj_create(screen);
    lv_obj_add_style(list_container, &style_card, 0);
    lv_obj_set_size(list_container, LV_PCT(100), 270);  // Больше места под компактный хедер
    lv_obj_align(list_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(list_container, 4, 0);  // Компактные отступы
    lv_obj_set_style_pad_row(list_container, 2, 0);  // Минимальные отступы между элементами
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Используем стиль фокуса для интерактивных элементов
    extern lv_style_t style_card_focused;
    
    // Создание компактных элементов для каждого насоса
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // КРИТИЧНО: Feed watchdog при создании каждого виджета
        esp_task_wdt_reset();
        
        // Используем кнопку для правильной обработки KEY_ENTER
        lv_obj_t *pump_item = lv_btn_create(list_container);
        if (!pump_item) {
            ESP_LOGE(TAG, "Failed to create pump_item for pump %d", i);
            continue;  // Пропускаем этот элемент
        }
        lv_obj_set_size(pump_item, LV_PCT(100), 36);  // Компактная высота
        lv_obj_add_style(pump_item, &style_card, 0);
        lv_obj_add_style(pump_item, &style_card_focused, LV_STATE_FOCUSED);  // Стиль при фокусе
        lv_obj_set_style_pad_all(pump_item, 4, 0);  // Компактные отступы
        
        // Обработчик клика (клик мышью и нажатие энкодера)
        widget_add_click_handler(pump_item, on_pump_click, (void*)(intptr_t)i);
        
        // Flex layout для правильного расположения элементов
        lv_obj_set_flex_flow(pump_item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pump_item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Имя насоса
        lv_obj_t *name_label = lv_label_create(pump_item);
        lv_label_set_text(name_label, PUMP_NAMES[i]);
        lv_obj_add_style(name_label, &style_label, 0);
        g_pump_labels[i] = name_label;
        
        // Статус и статистика в одной строке
        lv_obj_t *right_container = lv_obj_create(pump_item);
        lv_obj_remove_style_all(right_container);
        lv_obj_set_size(right_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(right_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(right_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(right_container, 6, 0);
        
        // Статистика (компактно)
        lv_obj_t *stats_label = lv_label_create(right_container);
        lv_label_set_text(stats_label, "0/0мл");
        lv_obj_set_style_text_color(stats_label, lv_color_hex(0xaaaaaa), 0);
        g_stats_labels[i] = stats_label;
        
        // Статус (активен/остановлен)
        lv_obj_t *status_label = lv_label_create(right_container);
        lv_label_set_text(status_label, "●");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x808080), 0);
        g_status_labels[i] = status_label;
    }
    
    // Нижние кнопки убраны - все функции доступны через меню насосов
    
    ESP_LOGD(TAG, "Экран статуса насосов создан");
    
    // КРИТИЧНО: Feed watchdog перед обновлением
    esp_task_wdt_reset();
    
    // Начальное обновление
    pumps_status_screen_update_all();
    
    // КРИТИЧНО: Feed watchdog после обновления
    esp_task_wdt_reset();
    
    return screen;
}

/* =============================
 *  ОБНОВЛЕНИЕ ДАННЫХ
 * ============================= */

esp_err_t pumps_status_screen_update(pump_index_t pump_idx)
{
    if (pump_idx >= PUMP_INDEX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_pump_labels[pump_idx] == NULL || g_stats_labels[pump_idx] == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Получение статистики насоса
    pump_stats_t stats;
    esp_err_t err = pump_manager_get_stats(pump_idx, &stats);
    
    if (err == ESP_OK) {
        // Обновление статистики
        char stats_text[64];
        snprintf(stats_text, sizeof(stats_text), 
                 "Запусков: %lu, Объем: %.1f мл",
                 (unsigned long)stats.total_runs,
                 stats.total_volume_ml);
        lv_label_set_text(g_stats_labels[pump_idx], stats_text);
        
        // Обновление статуса (зеленый = активен недавно, серый = остановлен)
        uint64_t now = esp_timer_get_time() / 1000ULL; // мс
        bool recently_active = (now - stats.last_run_time) < 5000; // активен если работал < 5 сек назад
        
        lv_obj_set_style_text_color(g_status_labels[pump_idx], 
                                     recently_active ? lv_color_hex(0x4CAF50) : lv_color_hex(0x808080), 
                                     0);
    }
    
    return ESP_OK;
}

esp_err_t pumps_status_screen_on_show(lv_obj_t *screen, void *params)
{
    (void)screen;
    (void)params;
    ESP_LOGD(TAG, "Pumps status screen shown");
    return ESP_OK;
}

esp_err_t pumps_status_screen_on_hide(lv_obj_t *screen)
{
    (void)screen;
    ESP_LOGI(TAG, "Pumps status screen hidden");
    return ESP_OK;
}

esp_err_t pumps_status_screen_update_all(void)
{
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        pumps_status_screen_update(i);
    }
    return ESP_OK;
}

