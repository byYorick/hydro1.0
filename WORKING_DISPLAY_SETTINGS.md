# Рабочие настройки для четкого отображения текста на ILI9341

## Описание
Эти настройки обеспечивают четкое отображение текста без "рваных" символов и "плывущих" цветов на дисплее ILI9341 с ESP32S3 и LVGL.

## Настройки sdkconfig (LVGL конфигурация)

### Цветовые настройки
```
CONFIG_LV_COLOR_DEPTH=16
CONFIG_LV_COLOR_16_SWAP=y
CONFIG_LV_COLOR_MIX_ROUND_OFS=128
CONFIG_LV_COLOR_CHROMA_KEY_HEX=0x00FF00
```

### Настройки HAL
```
CONFIG_LV_DISP_DEF_REFR_PERIOD=30
CONFIG_LV_INDEV_DEF_READ_PERIOD=30
CONFIG_LV_DPI_DEF=130
```

### Настройки отрисовки
```
CONFIG_LV_DRAW_COMPLEX=y
CONFIG_LV_SHADOW_CACHE_SIZE=0
CONFIG_LV_CIRCLE_CACHE_SIZE=4
CONFIG_LV_LAYER_SIMPLE_BUF_SIZE=24576
CONFIG_LV_IMG_CACHE_DEF_SIZE=0
CONFIG_LV_GRADIENT_MAX_STOPS=2
CONFIG_LV_GRAD_CACHE_DEF_SIZE=0
CONFIG_LV_DISP_ROT_MAX_BUF=10240
```

### Настройки шрифтов
```
# CONFIG_LV_USE_FONT_SUBPX is not set
CONFIG_LV_USE_FONT_PLACEHOLDER=y
```

## Настройки дисплея в lcd_ili9341.c

### Конфигурация панели
```c
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = PIN_NUM_LCD_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // BGR для ILI9341
    .bits_per_pixel = 16,                        // 16 бит на пиксель
};
```

### Настройки шрифтов в lvgl_main.c
```c
// Основные шрифты
lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
lv_style_set_text_font(&style_unit, &lv_font_montserrat_14);
lv_style_set_text_font(&style_badge, &lv_font_montserrat_14);

// Большие значения
lv_style_set_text_font(&style_value, &lv_font_montserrat_20);
lv_style_set_text_font(&style_value_large, &lv_font_montserrat_20);
```

## Ключевые принципы

1. **Порядок цветов**: BGR для ILI9341 (не RGB)
2. **Обмен байтов**: Включен (CONFIG_LV_COLOR_16_SWAP=y)
3. **DPI**: Стандартное значение 130
4. **Субпиксельный рендеринг**: Отключен
5. **Антиалиасинг**: Настраивается через конфигурацию LVGL
6. **Период обновления**: 30ms (стандартное значение)
7. **Шрифты**: Montserrat 14 для основного текста, 20 для больших значений

## Важные замечания

- НЕ использовать функции `lv_style_set_text_antialias` - они не поддерживаются в этой версии LVGL
- НЕ включать `CONFIG_LV_USE_FONT_SUBPX` - может вызывать размытие текста
- НЕ менять порядок цветов на RGB для ILI9341
- Использовать стандартные размеры буферов (24576, 10240)

## Результат
- ✅ Четкое отображение текста
- ✅ Отсутствие "рваных" символов
- ✅ Отсутствие "плывущих" цветов
- ✅ Стабильная работа дисплея
