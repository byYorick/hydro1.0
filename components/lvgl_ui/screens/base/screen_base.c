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
    
    // Применяем базовый стиль - компактный дизайн
    lv_obj_remove_style_all(base.screen);
    lv_obj_add_style(base.screen, &style_bg, 0);
    lv_obj_set_style_pad_all(base.screen, 8, 0);  // Компактные отступы экрана
    
    int content_y_offset = 0;
    
    // Создаем статус-бар если нужен
    if (config->has_status_bar) {
        base.status_bar = widget_create_status_bar(base.screen, config->title);
        content_y_offset += 30;  // Компактная высота статус-бара
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
    if (!base.content) {
        ESP_LOGE(TAG, "Failed to create content area");
        lv_obj_del(base.screen);
        base.screen = NULL;
        return base;
    }
    lv_obj_remove_style_all(base.content);
    
    // Правильно вычисляем размер (240x320 экран, зазор 3px между header и body)
    int screen_height = 320;
    int content_height = screen_height - content_y_offset - 11;  // 8 нижний + 3 зазор
    
    lv_obj_set_size(base.content, LV_PCT(100), content_height);
    lv_obj_align(base.content, LV_ALIGN_TOP_LEFT, 0, content_y_offset + 3);  // Зазор 3px после header
    lv_obj_set_style_pad_all(base.content, 0, 0);  // БЕЗ отступов - body на весь экран
    
    ESP_LOGD(TAG, "Content area: height=%d (screen_height=%d - offset=%d - padding=32)", 
             content_height, screen_height, content_y_offset);
    
    ESP_LOGD(TAG, "Base screen created successfully");
    
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

