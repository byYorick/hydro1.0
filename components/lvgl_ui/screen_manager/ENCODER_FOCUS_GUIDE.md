# Руководство по работе с фокусом и энкодером

## Версия: 1.1
**Дата:** 2025-10-09  
**Статус:** ✅ Реализовано

---

## 🎯 Обзор улучшений

Screen Manager теперь имеет **полную автоматическую поддержку фокуса и энкодера** на всех экранах:

- ✅ **Автоматическое добавление** всех интерактивных элементов в группу энкодера
- ✅ **Итеративный обход** дерева виджетов без рекурсии (до 20 уровней вложенности)
- ✅ **Автоматическая установка фокуса** при показе экрана
- ✅ **Функции ручного управления** для сложных случаев
- ✅ **Упрощенные callbacks** - больше не нужно дублировать логику в on_show

---

## 🔧 Автоматическая работа

### Что добавляется автоматически

При создании экрана (`screen_create_instance`) система автоматически:

1. Создает группу энкодера (`lv_group_t`)
2. Обходит все дерево UI итеративно (BFS, до 20 уровней)
3. Находит все интерактивные элементы:
   - `lv_button` (кнопки)
   - `lv_slider` (слайдеры)
   - `lv_dropdown` (выпадающие списки)
   - `lv_checkbox` (чекбоксы)
   - `lv_switch` (переключатели)
   - `lv_roller` (роллеры)
   - `lv_textarea` (текстовые поля)
   - Объекты с флагом `LV_OBJ_FLAG_CLICKABLE`
4. Добавляет их в группу энкодера
5. Устанавливает группу на устройство ввода (encoder indev)
6. Устанавливает начальный фокус на первый элемент

### При показе экрана

При показе экрана (`screen_show_instance`):

1. Группа энкодера привязывается к устройству ввода
2. Автоматически устанавливается фокус на первый элемент
3. Вызывается `on_show` callback (если есть)

**Важно:** Больше не нужно вручную добавлять виджеты в группу в `on_show` callback!

---

## 📝 Примеры использования

### Пример 1: Простой экран (автоматически работает)

```c
static lv_obj_t* my_screen_create(void *params)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Создаем кнопки
    lv_obj_t *btn1 = lv_btn_create(screen);
    lv_obj_t *btn2 = lv_btn_create(screen);
    lv_obj_t *btn3 = lv_btn_create(screen);
    
    // ВСЁ! Кнопки автоматически добавятся в группу энкодера
    // Фокус и навигация энкодером работают из коробки
    
    return screen;
}
```

### Пример 2: Упрощенный on_show callback

```c
// ДО (старый способ - дублирование логики):
static esp_err_t my_screen_on_show(lv_obj_t *screen, void *params)
{
    screen_instance_t *inst = screen_get_by_id("my_screen");
    
    // Вручную ищем и добавляем все кнопки...
    lv_obj_t *child = lv_obj_get_child(screen, 0);
    while (child != NULL) {
        if (lv_obj_check_type(child, &lv_button_class)) {
            lv_group_add_obj(inst->encoder_group, child);
        }
        // И так далее для всех уровней...
    }
    
    return ESP_OK;
}

// ПОСЛЕ (новый способ - проще и надежнее):
static esp_err_t my_screen_on_show(lv_obj_t *screen, void *params)
{
    // Просто проверяем и логируем
    screen_instance_t *inst = screen_get_by_id("my_screen");
    int obj_count = lv_group_get_obj_count(inst->encoder_group);
    ESP_LOGI(TAG, "Encoder group ready with %d elements", obj_count);
    
    return ESP_OK;
}
```

### Пример 3: Ручное добавление виджета (при необходимости)

```c
// Если нужно добавить виджет вручную после создания экрана
static void add_dynamic_button(void)
{
    // Создаем новую кнопку динамически
    lv_obj_t *new_btn = lv_btn_create(some_parent);
    
    // Добавляем в группу энкодера текущего экрана
    screen_add_to_group(NULL, new_btn);  // NULL = текущий экран
    
    // Или для конкретного экрана:
    screen_add_to_group("my_screen", new_btn);
}
```

### Пример 4: Добавление сложного виджета с иерархией

```c
// Если виджет имеет сложную иерархию интерактивных элементов
static void add_complex_widget(void)
{
    lv_obj_t *complex_widget = create_my_complex_widget();
    
    // Добавляем весь виджет с вложенными элементами
    int added = screen_add_widget_tree(NULL, complex_widget);
    ESP_LOGI(TAG, "Added %d interactive elements", added);
}
```

---

## 🔍 API функций

### Автоматические (уже работают)

```c
// При создании экрана:
esp_err_t screen_create(const char *screen_id);

// При показе экрана:
esp_err_t screen_show(const char *screen_id, void *params);
```

### Ручное управление (для сложных случаев)

```c
/**
 * @brief Добавить виджет в группу энкодера
 * @param screen_id ID экрана (NULL = текущий)
 * @param widget Виджет для добавления
 */
esp_err_t screen_add_to_group(const char *screen_id, lv_obj_t *widget);

/**
 * @brief Добавить все интерактивные элементы виджета
 * @param screen_id ID экрана (NULL = текущий)
 * @param widget Корневой виджет
 * @return Количество добавленных элементов
 */
int screen_add_widget_tree(const char *screen_id, lv_obj_t *widget);
```

---

## ⚙️ Поддерживаемые типы виджетов

Автоматически добавляются:

| Тип | Класс LVGL | Описание |
|-----|-----------|----------|
| Кнопка | `lv_button_class` | Обычная кнопка |
| Слайдер | `lv_slider_class` | Регулятор значений |
| Выпадающий список | `lv_dropdown_class` | Выбор из списка |
| Чекбокс | `lv_checkbox_class` | Галочка вкл/выкл |
| Переключатель | `lv_switch_class` | Toggle переключатель |
| Роллер | `lv_roller_class` | Вертикальный список |
| Текстовое поле | `lv_textarea_class` | Ввод текста |
| **Любой объект** | `LV_OBJ_FLAG_CLICKABLE` | С флагом кликабельности |

---

## 🎨 Стили фокуса

Виджеты автоматически применяют стили при получении фокуса.

### Пример стиля фокуса для карточки:

```c
lv_style_t style_card_focused;
lv_style_init(&style_card_focused);
lv_style_set_border_width(&style_card_focused, 2);
lv_style_set_border_color(&style_card_focused, lv_color_hex(0x00FF00));
lv_style_set_border_opa(&style_card_focused, LV_OPA_COVER);

// Применяем к виджету
lv_obj_add_style(card, &style_card_focused, LV_STATE_FOCUSED);
```

---

## 🔧 Технические детали

### Алгоритм обхода

- **Тип:** Обход в ширину (BFS - Breadth-First Search)
- **Реализация:** Итеративная (без рекурсии)
- **Очередь:** Массив на стеке, 128 элементов
- **Максимальная глубина:** 20 уровней вложенности
- **Старт:** С детей корневого объекта (сам root_obj не проверяется)
- **Защита:** От дубликатов и переполнения очереди

### Производительность

- Быстрый обход (BFS оптимален для UI деревьев)
- Нет overhead рекурсии
- Проверка дубликатов перед добавлением (O(n²), но n обычно < 20)
- Защита от переполнения очереди с ранним выходом

### Порядок операций при показе экрана

1. **Загрузка экрана** (`lv_scr_load`)
2. **Привязка encoder_group** к устройству ввода
3. **Установка фокуса** на первый элемент группы
4. **Вызов on_show callback** (для дополнительной логики)
5. **Обновление состояния** (is_visible = true)

---

## ✅ Проверка работы

### Логи при создании экрана:

```
I (123) SCREEN_LIFECYCLE: Creating screen 'main'...
I (124) SCREEN_LIFECYCLE: Auto-adding interactive elements to encoder group...
I (125) SCREEN_LIFECYCLE: Auto-added 7 interactive elements to encoder group
I (126) SCREEN_LIFECYCLE: Created screen 'main' (1/15 instances active, 7 elements in encoder group)
```

### Логи при показе экрана:

```
I (200) SCREEN_LIFECYCLE: Showing screen 'main'...
I (201) SCREEN_LIFECYCLE: Encoder group set for 'main' (7 objects)
```

### Проверка в коде:

```c
screen_instance_t *inst = screen_get_by_id("my_screen");
int count = lv_group_get_obj_count(inst->encoder_group);
ESP_LOGI(TAG, "Encoder group has %d objects", count);
```

---

## 🐛 Отладка

### Если фокус не работает:

1. **Проверьте логи** - сколько элементов добавлено в группу?
2. **Убедитесь что виджет интерактивный:**
   ```c
   lv_obj_add_flag(widget, LV_OBJ_FLAG_CLICKABLE);
   ```
3. **Проверьте, что encoder indev настроен правильно** в `lcd_ili9341`
4. **Добавьте вручную при необходимости:**
   ```c
   screen_add_to_group(NULL, my_widget);
   ```

### Если добавляются лишние элементы:

- Удалите флаг `LV_OBJ_FLAG_CLICKABLE` с контейнеров
- Или добавьте вручную только нужные элементы

---

## 📚 См. также

- [Screen Manager README](README.md) - Основная документация
- `screen_lifecycle.h` - API управления жизненным циклом
- `screen_manager.h` - Главный API

---

**Автор:** Hydroponics Monitor Team  
**Версия Screen Manager:** 1.1  
**Статус:** ✅ Production Ready

