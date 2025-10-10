# Добавление иконок в шрифт

## Проблема
Иконки отображаются квадратами, потому что в пользовательском шрифте `montserrat_ru` нет глифов для символов LVGL (LV_SYMBOL_*).

## Решение 1: Fallback шрифт (Рекомендуется для LVGL 8.2+)

Уже реализовано в `montserrat14_ru.c` - используется встроенный шрифт `lv_font_montserrat_14` как fallback для отсутствующих символов.

```c
.fallback = &lv_font_montserrat_14,  // Для иконок
```

## Решение 2: Пересоздание шрифта с иконками

Если fallback не работает, нужно пересоздать шрифт с включением диапазона иконок.

### Шаги:

1. **Скачайте LVGL Font Converter:**
   - Online: https://lvgl.io/tools/fontconverter
   - Offline: `npm install -g lv_font_conv`

2. **Подготовьте шрифт Montserrat:**
   ```bash
   # Скачайте Montserrat-Regular.ttf
   ```

3. **Создайте шрифт с иконками:**

   **Вариант A: Online converter**
   - Откройте https://lvgl.io/tools/fontconverter
   - Загрузите `Montserrat-Regular.ttf`
   - Размер: 14
   - BPP: 4
   - Диапазоны:
     ```
     0x20-0x7F      # ASCII
     0x400-0x4FF    # Кириллица
     0xF000-0xF8FF  # Иконки LVGL (Private Use Area)
     ```
   - TTF/WOFF font: выберите файл шрифта
   - Output name: `montserrat14_ru`
   - Format: LVGL
   - Try Unicode: ✓

   **Вариант B: Командная строка**
   ```bash
   lv_font_conv --font Montserrat-Regular.ttf \
                --size 14 \
                --bpp 4 \
                --format lvgl \
                --range 0x20-0x7F,0x400-0x4FF,0xF000-0xF8FF \
                --no-compress \
                --output montserrat14_ru.c
   ```

4. **Замените файлы:**
   - Замените `main/montserrat14_ru.c` на новый
   - Обновите `main/montserrat14_ru.h` если нужно

## Решение 3: Использование отдельного шрифта для иконок

Создайте отдельный шрифт только для иконок:

```c
// В lvgl_ui.c
#include "lv_font_montserrat_14.h"  // Встроенный шрифт с иконками

// При создании объектов с иконками
lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);

// Для текста с кириллицей
lv_obj_set_style_text_font(text_label, &montserrat_ru, 0);
```

## Проверка

После применения решения символы должны отображаться правильно:
- ⚙ LV_SYMBOL_SETTINGS
- ◄ LV_SYMBOL_LEFT  
- ► LV_SYMBOL_RIGHT
- ⟳ LV_SYMBOL_REFRESH
- ⚠ LV_SYMBOL_WARNING
- и т.д.

## Примечания

- Символы LVGL находятся в диапазоне Unicode 0xF000-0xF8FF (Private Use Area)
- Встроенные шрифты LVGL уже содержат эти символы
- Fallback шрифт - самое простое решение, не требует изменения кода UI

## Дополнительная информация

Список символов LVGL: https://docs.lvgl.io/master/overview/font.html#built-in-fonts

