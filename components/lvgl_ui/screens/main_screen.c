/**
 * @file main_screen.c
 * @brief Реализация главного экрана
 */

#include "main_screen.h"
#include "screen_manager/screen_manager.h"
#include "widgets/status_bar.h"
#include "widgets/sensor_card.h"
#include "widgets/event_helpers.h"
#include "lvgl_styles.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MAIN_SCREEN";

// Ссылки на карточки датчиков для обновления
static lv_obj_t *sensor_cards[6] = {NULL};

/* =============================
 *  CALLBACKS
 * ============================= */

/**
 * @brief Callback при клике на карточку датчика
 */
static void on_sensor_card_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем и клик мышью и нажатие Enter от энкодера
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        int sensor_id = (int)(intptr_t)lv_event_get_user_data(e);
        
        // ID экранов детализации
        const char *detail_screens[] = {
            "detail_ph", "detail_ec", "detail_temp",
            "detail_humidity", "detail_lux", "detail_co2"
        };
        
        if (sensor_id >= 0 && sensor_id < 6) {
            ESP_LOGI(TAG, "Opening detail screen for sensor %d (event: %d)", sensor_id, code);
            screen_show(detail_screens[sensor_id], NULL);
        }
    }
}

/**
 * @brief Callback при клике на кнопку системных настроек
 */
static void on_system_settings_click(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Обрабатываем и клик мышью и нажатие Enter от энкодера
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "Opening system settings (event: %d)", code);
        screen_show("system_menu", NULL);
    }
}

/**
 * @brief Callback при показе экрана - проверка группы энкодера
 */
static esp_err_t main_screen_on_show(lv_obj_t *screen, void *params)
{
    ESP_LOGD(TAG, "Main Screen ON_SHOW Callback");
    
    // LVGL автоматически управляет фокусом через группы
    // Дополнительная настройка не требуется
    
    return ESP_OK;
}

/* =============================
 *  СОЗДАНИЕ UI
 * ============================= */

/**
 * @brief Функция создания главного экрана
 */
static lv_obj_t* main_screen_create(void *params)
{
    ESP_LOGD(TAG, "=========================================");
    ESP_LOGD(TAG, "   Creating Main Screen                ");
    ESP_LOGD(TAG, "=========================================");
    
    // Создаем корневой объект экрана
    lv_obj_t *screen = lv_obj_create(NULL);
    ESP_LOGD(TAG, "  Screen object created: %p", screen);
    
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &style_bg, 0);
    ESP_LOGD(TAG, "  Background style applied");
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Настраиваем flex layout
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, 
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    
    // Создаем компактный статус-бар с максимальной информативностью
    lv_obj_t *status_bar = lv_obj_create(screen);
    lv_obj_add_style(status_bar, &style_card, 0);
    lv_obj_set_size(status_bar, LV_PCT(100), 40);  // Еще компактнее
    lv_obj_set_style_pad_all(status_bar, 6, 0);
    
    // Flex layout для равномерного распределения
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Заголовок - компактный
    extern lv_style_t style_title;
    lv_obj_t *title = lv_label_create(status_bar);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Hydro v3.0");
    lv_obj_set_flex_grow(title, 1);
    
    // Время работы системы
    lv_obj_t *uptime_label = lv_label_create(status_bar);
    lv_obj_add_style(uptime_label, &style_unit, 0);
    lv_label_set_text(uptime_label, "00:00");
    
    // Кнопка SET - минималистичная
    lv_obj_t *set_btn = lv_btn_create(status_bar);
    lv_obj_add_style(set_btn, &style_card, 0);
    lv_obj_set_size(set_btn, 35, 28);
    // Обработка клика мышью и нажатия энкодера
    widget_add_click_handler(set_btn, on_system_settings_click, NULL);
    
    lv_obj_t *set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "SET");
    lv_obj_center(set_label);
    
    // Контейнер для карточек датчиков - оптимизированная сетка
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
    
    // GRID LAYOUT: 2 колонки × 3 ряда с оптимизированными размерами
    // Расчёт: 240px - 4px (left) - 4px (right) - 4px (между) = 228px / 2 = 114px
    static int32_t col_dsc[] = {110, 110, LV_GRID_TEMPLATE_LAST};  // 2 колонки по 110px (уменьшено для экрана 240px)
    static int32_t row_dsc[] = {85, 85, 85, LV_GRID_TEMPLATE_LAST}; // 3 ряда по 85px
    lv_obj_set_grid_dsc_array(content, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(content, 5, 0);     // Увеличен до 5px для симметрии
    lv_obj_set_style_pad_row(content, 4, 0);
    lv_obj_set_style_pad_column(content, 10, 0);  // Увеличен до 10px для красоты
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Метаданные датчиков
    const char *sensor_names[] = {"pH", "EC", "Temperature", "Humidity", "Light", "CO2"};
    const char *sensor_units[] = {"", "mS/cm", "°C", "%", "lux", "ppm"};
    const uint8_t decimals[] = {2, 2, 1, 1, 0, 0};
    
    // Создаем 6 карточек датчиков и размещаем в сетке 2x3
    for (int i = 0; i < 6; i++) {
        // КРИТИЧНО: Feed watchdog при создании каждого виджета
        extern void esp_task_wdt_reset(void);
        esp_task_wdt_reset();
        
        sensor_card_config_t card_cfg = {
            .name = sensor_names[i],
            .unit = sensor_units[i],
            .current_value = NAN,  // Начальное значение - нет данных
            .decimals = decimals[i],
            .on_click = on_sensor_card_click,
            .user_data = (void*)(intptr_t)i,
        };
        
        lv_obj_t *card = widget_create_sensor_card(content, &card_cfg);
        sensor_cards[i] = card;  // Сохраняем для обновления
        
        // Размещаем в grid: col, row
        int col = i % 2;  // 0, 1, 0, 1, 0, 1
        int row = i / 2;  // 0, 0, 1, 1, 2, 2
        lv_obj_set_grid_cell(card, LV_GRID_ALIGN_CENTER, col, 1,
                            LV_GRID_ALIGN_CENTER, row, 1);
        
        ESP_LOGD(TAG, "  Card %d ('%s') created at grid[%d][%d]", i, sensor_names[i], row, col);
    }
    
    // ВАЖНО: Группа энкодера будет настроена в on_show callback
    // Сохраняем ссылку на SET кнопку для добавления в группу позже
    lv_obj_set_user_data(screen, set_btn);  // Сохраняем для on_show
    
    ESP_LOGD(TAG, "Main screen created with 6 sensor cards");
    
    return screen;
}

/* =============================
 *  ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================= */

esp_err_t main_screen_init(void)
{
    ESP_LOGD(TAG, "Initializing main screen");
    
    screen_config_t config = {
        .id = "main",
        .title = "Hydroponics Monitor",
        .category = SCREEN_CATEGORY_MAIN,
        .parent_id = "",                // Нет родителя
        .is_root = true,                // Корневой экран
        .can_go_back = false,           // С главного некуда возвращаться
        .lazy_load = false,             // Создать сразу при инициализации
        .cache_on_hide = true,          // Всегда держать в памяти
        .destroy_on_hide = false,
        .has_status_bar = false,        // Свой статус-бар
        .has_back_button = false,       // Нет кнопки назад
        .create_fn = main_screen_create,
        .on_show = main_screen_on_show, // ВАЖНО! Настройка группы энкодера
    };
    
    esp_err_t ret = screen_register(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register main screen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Main screen registered successfully");
    return ESP_OK;
}

esp_err_t main_screen_update_sensor(uint8_t sensor_index, float value)
{
    if (sensor_index >= 6) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (sensor_cards[sensor_index]) {
        widget_sensor_card_update_value(sensor_cards[sensor_index], value);
        return ESP_OK;
    }
    
    return ESP_ERR_INVALID_STATE;
}

