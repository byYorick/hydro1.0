/**
 * @file screen_base.c
 * @brief Реализация базового экрана
 */

#include "screen_base.h"
#include "../../widgets/status_bar.h"
#include "../../widgets/back_button.h"
#include "esp_log.h"

// Внешние стили
extern lv_style_t style_bg;

static const char *TAG = "SCREEN_BASE";

screen_base_t screen_base_create(const screen_base_config_t *config)
{
    screen_base_t base = {0};
    
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return base;
    }
    
    ESP_LOGI(TAG, "Creating base screen '%s'", config->title ? config->title : "(no title)");
    
    // Создаем корневой объект экрана
    base.screen = lv_obj_create(NULL);
    if (!base.screen) {
        ESP_LOGE(TAG, "Failed to create screen object");
        return base;
    }
    
    // Применяем базовый стиль
    lv_obj_remove_style_all(base.screen);
    lv_obj_add_style(base.screen, &style_bg, 0);
    lv_obj_set_style_pad_all(base.screen, 16, 0);
    
    int content_y_offset = 0;
    
    // Создаем статус-бар если нужен
    if (config->has_status_bar) {
        base.status_bar = widget_create_status_bar(base.screen, config->title);
        content_y_offset += 70;  // Высота статус-бара + отступ
        ESP_LOGD(TAG, "Status bar created");
    }
    
    // Создаем кнопку назад если нужна
    if (config->has_back_button) {
        base.back_button = widget_create_back_button(base.screen, 
                                                      config->back_callback,
                                                      config->back_user_data);
        ESP_LOGD(TAG, "Back button created");
    }
    
    // Создаем контентную область
    base.content = lv_obj_create(base.screen);
    lv_obj_remove_style_all(base.content);
    lv_obj_set_size(base.content, LV_PCT(100), LV_PCT(100) - content_y_offset);
    lv_obj_align(base.content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(base.content, 0, 0);
    
    ESP_LOGI(TAG, "Base screen created successfully");
    
    return base;
}

void screen_base_destroy(screen_base_t *base)
{
    if (!base || !base->screen) {
        return;
    }
    
    // LVGL автоматически удалит дочерние объекты при удалении родителя
    lv_obj_del(base->screen);
    
    memset(base, 0, sizeof(screen_base_t));
    
    ESP_LOGD(TAG, "Base screen destroyed");
}

