/**
 * @file system_screens.c
 * @brief Реализация всех системных экранов
 */

#include "system_screens.h"
#include "wifi_settings_screen.h"
#include "screen_manager/screen_manager.h"
#include "screen_manager/screen_lifecycle.h"
#include "screens/base/screen_base.h"
#include "widgets/back_button.h"
#include "lvgl_styles.h"
#include "montserrat14_ru.h"
#include "esp_log.h"
#include "lcd_ili9341.h"
#include "config_manager.h"

static const char *TAG = "SYSTEM_SCREENS";

/* =============================
 *  МАКРОСЫ ДЛЯ УПРОЩЕНИЯ КОДА
 * ============================= */

/**
 * @brief Макрос для инициализации базовой конфигурации системных экранов
 * 
 * Все системные экраны имеют одинаковые базовые параметры:
 * - Status bar включен
 * - Кнопка назад включена
 * - Callback назад NULL (автоматическая навигация)
 * 
 * Использование: 
 *   screen_base_config_t cfg;
 *   INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Title");
 */
#define INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, screen_title) \
    do { \
        (cfg).title = (screen_title); \
        (cfg).has_status_bar = true; \
        (cfg).has_back_button = true; \
        (cfg).back_callback = NULL; \
        (cfg).back_user_data = NULL; \
    } while(0)

/* =============================
 *  ОБЩИЙ CALLBACK для всех системных экранов
 * ============================= */

/**
 * @brief Универсальный callback для автоматической настройки encoder группы
 * 
 * Использует новую универсальную функцию screen_auto_setup_encoder_group()
 * вместо ручного обхода виджетов, что упрощает код и уменьшает дублирование.
 */
static esp_err_t system_screen_on_show(lv_obj_t *screen_obj, void *params)
{
    const char *screen_id = (const char*)params;
    if (!screen_id) {
        ESP_LOGW(TAG, "No screen ID in params");
        return ESP_OK;
    }
    
    ESP_LOGD(TAG, "System screen '%s' shown - auto-configuring encoder", screen_id);
    
    // Получаем instance для доступа к encoder_group
    screen_instance_t *inst = screen_get_by_id(screen_id);
    if (!inst || !inst->encoder_group) {
        ESP_LOGW(TAG, "No encoder group available for %s", screen_id);
        return ESP_OK;
    }
    
    // Используем универсальную функцию для настройки encoder группы
    int added = screen_auto_setup_encoder_group(screen_obj, inst->encoder_group);
    
    if (added > 0) {
        ESP_LOGI(TAG, "System screen '%s': %d elements added to encoder group", screen_id, added);
    } else {
        ESP_LOGW(TAG, "System screen '%s': no interactive elements found", screen_id);
    }
    
    return ESP_OK;
}

/* =============================
 *  AUTO CONTROL SCREEN
 * ============================= */

static lv_obj_t* auto_control_create(void *params)
{
    ESP_LOGI(TAG, "Creating auto control screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Авто контроль");
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Настройки авто контроля\n\n(В разработке)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  WIFI SETTINGS SCREEN
 * ============================= */

// WiFi экран реализован в отдельном файле wifi_settings_screen.c
// static lv_obj_t* wifi_settings_create(void *params) - см. wifi_settings_screen.c

/* =============================
 *  DISPLAY SETTINGS SCREEN
 * ============================= */

// Callback для изменения яркости
static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
    
    int32_t value = lv_slider_get_value(slider);
    
    // Обновляем текст
    lv_label_set_text_fmt(label, "%d%%", (int)value);
    
    // Применяем яркость
    lcd_ili9341_set_brightness((uint8_t)value);
    
    // Сохраняем в конфигурацию
    system_config_t config;
    if (config_load(&config) == ESP_OK) {
        config.display_brightness = (uint8_t)value;
        esp_err_t ret = config_save(&config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save brightness: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Brightness saved: %d%%", (int)value);
        }
    }
}

static lv_obj_t* display_settings_create(void *params)
{
    ESP_LOGI(TAG, "Creating display settings screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Дисплей");
    screen_base_t base = screen_base_create(&cfg);
    
    // Получаем текущую яркость
    uint8_t current_brightness = lcd_ili9341_get_brightness();
    
    // Контейнер для настроек яркости
    lv_obj_t *container = lv_obj_create(base.content);
    if (!container) {
        ESP_LOGE(TAG, "Failed to create container for display settings");
        return base.screen;  // Возвращаем экран без контента
    }
    lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1A2332), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 15, 0);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 10);
    
    // Заголовок "Яркость"
    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, "Brightness");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &montserrat_ru, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Метка со значением яркости
    lv_obj_t *value_label = lv_label_create(container);
    lv_label_set_text_fmt(value_label, "%d%%", current_brightness);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(value_label, &montserrat_ru, 0);
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    // Слайдер яркости
    lv_obj_t *slider = lv_slider_create(container);
    if (slider) {
        lv_obj_set_width(slider, LV_PCT(100));
        lv_slider_set_range(slider, 10, 100);  // От 10% до 100%
        lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
        lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 40);
        
        // Стиль слайдера
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x2D3E50), LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x00D4AA), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x00D4AA), LV_PART_KNOB);
        lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB);
        
        // Добавляем событие
        lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, value_label);
    } else {
        ESP_LOGW(TAG, "Failed to create brightness slider");
    }
    
    // Подсказка
    lv_obj_t *hint_label = lv_label_create(container);
    lv_label_set_text(hint_label, "Rotate encoder to adjust");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(hint_label, &montserrat_ru, 0);
    lv_obj_align(hint_label, LV_ALIGN_TOP_MID, 0, 70);
    
    return base.screen;
}

/* =============================
 *  DATA LOGGER SCREEN
 * ============================= */

static lv_obj_t* data_logger_create(void *params)
{
    ESP_LOGI(TAG, "Creating data logger screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Логи данных");
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Data Logger Settings\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  SYSTEM INFO SCREEN
 * ============================= */

static lv_obj_t* system_info_create(void *params)
{
    ESP_LOGI(TAG, "Creating system info screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "О системе");
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "System Information\n\nESP32-S3\nVersion 3.0");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  RESET CONFIRM SCREEN
 * ============================= */

static lv_obj_t* reset_confirm_create(void *params)
{
    ESP_LOGI(TAG, "Creating reset confirm screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Подтверждение сброса");
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "Reset all settings?\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  SYSTEM STATUS SCREEN
 * ============================= */

static lv_obj_t* system_status_create(void *params)
{
    ESP_LOGI(TAG, "Creating system status screen");
    
    screen_base_config_t cfg;
    INIT_SYSTEM_SCREEN_BASE_CONFIG(cfg, "Статус системы");
    screen_base_t base = screen_base_create(&cfg);
    
    lv_obj_t *label = lv_label_create(base.content);
    lv_obj_add_style(label, &style_label, 0);
    lv_label_set_text(label, "System Status\n\n(Placeholder)");
    lv_obj_center(label);
    
    return base.screen;
}

/* =============================
 *  МЕТАДАННЫЕ ДЛЯ РЕГИСТРАЦИИ
 * ============================= */

/**
 * @brief Метаданные системного экрана
 */
typedef struct {
    const char *id;                  ///< ID экрана
    const char *title;               ///< Заголовок
    screen_category_t category;      ///< Категория
    screen_create_fn_t create_fn;    ///< Функция создания
} system_screen_meta_t;

/**
 * @brief Массив метаданных всех системных экранов
 * 
 * Упрощает регистрацию экранов и уменьшает дублирование кода.
 * Все общие параметры (parent_id, lazy_load, destroy_on_hide) вынесены в цикл.
 */
static const system_screen_meta_t SYSTEM_SCREENS_META[] = {
    {
        .id = "system_status",
        .title = "System Status",
        .category = SCREEN_CATEGORY_INFO,
        .create_fn = system_status_create,
    },
    {
        .id = "auto_control",
        .title = "Auto Control",
        .category = SCREEN_CATEGORY_SETTINGS,
        .create_fn = auto_control_create,
    },
    {
        .id = "wifi_settings",
        .title = "WiFi",
        .category = SCREEN_CATEGORY_SETTINGS,
        .create_fn = wifi_settings_screen_create,  // Из wifi_settings_screen.c
    },
    {
        .id = "display_settings",
        .title = "Display Settings",
        .category = SCREEN_CATEGORY_SETTINGS,
        .create_fn = display_settings_create,
    },
    {
        .id = "data_logger",
        .title = "Data Logger",
        .category = SCREEN_CATEGORY_SETTINGS,
        .create_fn = data_logger_create,
    },
    {
        .id = "system_info",
        .title = "System Info",
        .category = SCREEN_CATEGORY_INFO,
        .create_fn = system_info_create,
    },
    {
        .id = "reset_confirm",
        .title = "Reset Confirm",
        .category = SCREEN_CATEGORY_DIALOG,
        .create_fn = reset_confirm_create,
    },
};

#define SYSTEM_SCREENS_COUNT (sizeof(SYSTEM_SCREENS_META) / sizeof(system_screen_meta_t))

/* =============================
 *  РЕГИСТРАЦИЯ
 * ============================= */

/**
 * @brief Зарегистрировать все системные экраны
 * 
 * Использует массив метаданных для упрощённой регистрации.
 * Все экраны имеют общие параметры:
 * - parent_id: "system_menu"
 * - lazy_load: true (создавать при показе)
 * - destroy_on_hide: true (уничтожать для экономии памяти)
 * - can_go_back: true
 * 
 * @return ESP_OK при успехе
 */
esp_err_t system_screens_register_all(void)
{
    ESP_LOGI(TAG, "Registering %d system screens...", SYSTEM_SCREENS_COUNT);
    
    for (int i = 0; i < SYSTEM_SCREENS_COUNT; i++) {
        const system_screen_meta_t *meta = &SYSTEM_SCREENS_META[i];
        
        // Создаём конфигурацию с общими параметрами
        screen_config_t config = {
            .id = {0},
            .title = meta->title,
            .category = meta->category,
            .parent_id = "system_menu",     // Общий parent для всех
            .can_go_back = true,            // Все экраны с кнопкой назад
            .lazy_load = true,              // Создавать при показе
            .cache_on_hide = false,         // Не кэшировать
            .destroy_on_hide = true,        // Уничтожать для экономии памяти
            .has_status_bar = true,
            .has_back_button = true,
            .create_fn = meta->create_fn,
            .on_show = system_screen_on_show,
            .user_data = (void*)meta->id,   // ID для callback
        };
        
        // Копируем ID (обязательное поле)
        strncpy(config.id, meta->id, MAX_SCREEN_ID_LEN - 1);
        
        // Регистрируем экран
        esp_err_t ret = screen_register(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register '%s': %s", 
                     meta->id, esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGD(TAG, "Registered '%s'", meta->id);
    }
    
    ESP_LOGI(TAG, "All %d system screens registered successfully", SYSTEM_SCREENS_COUNT);
    
    // WiFi экран имеет специфические callbacks - они установлены в config.on_show
    // через wifi_settings_screen_on_show и wifi_settings_screen_on_hide
    
    return ESP_OK;
}
