/**
 * @file pid_main_screen.c
 * @brief Реализация главного экрана PID контроллеров
 */

#include "pid_main_screen.h"
#include "screen_manager/screen_manager.h"
#include "../../widgets/back_button.h"
#include "../../widgets/status_bar.h"
#include "pump_manager.h"
#include "config_manager.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "PID_MAIN_SCREEN";

// Имена насосов
static const char* PUMP_NAMES[PUMP_INDEX_COUNT] = {
    "pH UP", "pH DOWN", "EC A", "EC B", "EC C", "Water"
};

// UI элементы
static lv_obj_t *g_screen = NULL;

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback для клика на элемент PID
 */
static void on_pid_item_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем и клик мышью и нажатие Enter от энкодера
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        pump_index_t pump_idx = (pump_index_t)(intptr_t)lv_event_get_user_data(e);
        ESP_LOGI(TAG, "Переход к детальному экрану PID для насоса %d (event: %d)", pump_idx, code);
        
        screen_show("pid_detail", (void*)(intptr_t)pump_idx);
    }
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

lv_obj_t* pid_main_screen_create(void *context)
{
    (void)context;
    ESP_LOGI(TAG, "Создание главного экрана PID");
    
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
    widget_create_status_bar(screen, "PID");
    
    // Back button
    widget_create_back_button(screen, NULL, NULL);
    
    // Получение конфигурации
    const system_config_t *config = config_manager_get_cached();
    
    // Контейнер для списка PID (занимает всё место)
    lv_obj_t *list_container = lv_obj_create(screen);
    lv_obj_add_style(list_container, &style_card, 0);
    lv_obj_set_size(list_container, LV_PCT(100), 270);  // Под компактный хедер
    lv_obj_align(list_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(list_container, 4, 0);  // Компактные отступы
    lv_obj_set_style_pad_row(list_container, 2, 0);  // Минимальные отступы
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Используем стиль фокуса для интерактивных элементов
    extern lv_style_t style_card_focused;
    
    // Создание компактных элементов для каждого PID
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
        // Используем кнопку для правильной обработки KEY_ENTER
        lv_obj_t *pid_item = lv_btn_create(list_container);
        lv_obj_set_size(pid_item, LV_PCT(100), 40);  // Компактная высота
        lv_obj_add_style(pid_item, &style_card, 0);
        lv_obj_add_style(pid_item, &style_card_focused, LV_STATE_FOCUSED);  // Стиль при фокусе
        lv_obj_set_style_pad_all(pid_item, 4, 0);  // Компактные отступы
        
        // Обработчик клика (клик мышью и нажатие энкодера)
        lv_obj_add_event_cb(pid_item, on_pid_item_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(pid_item, on_pid_item_click, LV_EVENT_PRESSED, (void*)(intptr_t)i);
        
        // Flex layout для компактного расположения
        lv_obj_set_flex_flow(pid_item, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(pid_item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(pid_item, 1, 0);
        
        // Первая строка: Имя и статус
        lv_obj_t *top_row = lv_obj_create(pid_item);
        lv_obj_remove_style_all(top_row);
        lv_obj_set_size(top_row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Имя насоса
        lv_obj_t *name_label = lv_label_create(top_row);
        lv_label_set_text(name_label, PUMP_NAMES[i]);
        lv_obj_add_style(name_label, &style_label, 0);
        
        // Статус (enabled/disabled)
        bool enabled = config ? config->pump_pid[i].enabled : false;
        lv_obj_t *status_label = lv_label_create(top_row);
        lv_label_set_text(status_label, enabled ? "ON" : "OFF");
        lv_obj_set_style_text_color(status_label, 
                                     enabled ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336), 
                                     0);
        lv_obj_set_style_text_font(status_label, &montserrat_ru, 0);
        
        // Вторая строка: Параметры PID (компактно)
        char params[48];
        if (config) {
            snprintf(params, sizeof(params), 
                     "%.1f/%.1f/%.1f",  // Компактный формат
                     config->pump_pid[i].kp,
                     config->pump_pid[i].ki,
                     config->pump_pid[i].kd);
        } else {
            snprintf(params, sizeof(params), "-/-/-");
        }
        
        lv_obj_t *params_label = lv_label_create(pid_item);
        lv_label_set_text(params_label, params);
        lv_obj_set_style_text_color(params_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(params_label, &montserrat_ru, 0);
    }
    
    ESP_LOGI(TAG, "Главный экран PID создан");
    
    return screen;
}

esp_err_t pid_main_screen_on_show(lv_obj_t *screen, void *params)
{
    (void)screen;
    (void)params;
    ESP_LOGI(TAG, "PID main screen shown");
    return ESP_OK;
}

esp_err_t pid_main_screen_on_hide(lv_obj_t *screen)
{
    (void)screen;
    ESP_LOGI(TAG, "PID main screen hidden");
    return ESP_OK;
}

esp_err_t pid_main_screen_update(void)
{
    // TODO: Обновление данных на экране
    return ESP_OK;
}

