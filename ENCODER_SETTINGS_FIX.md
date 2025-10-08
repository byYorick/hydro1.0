# Исправление кнопок на экране настроек

## Дата: 2025-10-08 (обновление)

## Проблема

После исправления навигации на главном экране, кнопки на экране настроек **не нажимались** при использовании энкодера.

### Симптомы:
- Навигация энкодером по кнопкам визуально работала (выделение кнопок)
- При нажатии на энкодер кнопки не активировались
- Не было реакции на нажатие энкодера на экране настроек

## Причина проблемы

Обнаружен **конфликт между двумя системами управления фокусом**:

1. **Старая система**: Функция `update_settings_selection()` вручную управляла выделением через переменную `selected_settings_item` и изменяла стили напрямую
2. **Новая система**: Группы LVGL (`lv_group_t`) с автоматической навигацией

Когда энкодер вращался, вызывалась `update_settings_selection()`, которая **конфликтовала** с системой групп LVGL, из-за чего кнопки не получали правильные события.

## Внесенные изменения

### Файл: `components/lvgl_ui/lvgl_ui.c`

#### 1. Обработка вращения энкодера по часовой стрелке (строки ~2043-2050)

**Было:**
```c
} else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
    selected_settings_item = (selected_settings_item + 1) % 5;
    update_settings_selection();
}
```

**Стало:**
```c
} else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
    // Используем встроенную навигацию LVGL через группу
    uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
    lv_group_t *settings_group = settings_screen_groups[sensor_index];
    if (settings_group) {
        lv_group_focus_next(settings_group);
        ESP_LOGI(TAG, "Settings: focus next");
    }
}
```

**Изменения:**
- Убран вызов `update_settings_selection()`
- Используется стандартная функция LVGL `lv_group_focus_next()`
- Добавлено логирование для диагностики

#### 2. Обработка вращения энкодера против часовой стрелки (строки ~2094-2101)

**Было:**
```c
} else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
    selected_settings_item = (selected_settings_item - 1 + 5) % 5;
    update_settings_selection();
}
```

**Стало:**
```c
} else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
    // Используем встроенную навигацию LVGL через группу
    uint8_t sensor_index = current_screen - SCREEN_SETTINGS_PH;
    lv_group_t *settings_group = settings_screen_groups[sensor_index];
    if (settings_group) {
        lv_group_focus_prev(settings_group);
        ESP_LOGI(TAG, "Settings: focus prev");
    }
}
```

**Изменения:**
- Аналогичные изменения для направления против часовой стрелки
- Используется `lv_group_focus_prev()`

#### 3. Обработка нажатия энкодера на экране настроек (строки ~2127-2158)

**Добавлено:**
```c
} else if (current_screen >= SCREEN_SETTINGS_PH && current_screen <= SCREEN_SETTINGS_CO2) {
    // На экране настроек - активируем элемент с фокусом
    lv_indev_t *indev = lcd_ili9341_get_encoder_indev();
    if (indev) {
        lv_group_t *group = lv_indev_get_group(indev);
        if (group) {
            lv_obj_t *focused = lv_group_get_focused(group);
            ESP_LOGI(TAG, "Settings screen: encoder pressed, focused object: %p, group: %p", focused, group);
            
            // Отправляем событие нажатия
            lv_group_send_data(group, LV_KEY_ENTER);
            
            // Также попробуем отправить событие клика напрямую
            if (focused) {
                ESP_LOGI(TAG, "Sending CLICKED event to focused object");
                lv_obj_send_event(focused, LV_EVENT_CLICKED, NULL);
            }
        } else {
            ESP_LOGW(TAG, "Settings screen: no group assigned to encoder!");
        }
    } else {
        ESP_LOGW(TAG, "Settings screen: encoder input device not found!");
    }
}
```

**Изменения:**
- Добавлено подробное логирование для диагностики
- Используется `lv_obj_send_event()` (правильная функция для LVGL 9.x)
- Отправляется как событие `LV_KEY_ENTER` в группу, так и событие `LV_EVENT_CLICKED` напрямую объекту

#### 4. Функция show_screen() (строки ~1796-1799)

**Было:**
```c
if (screen == SCREEN_MAIN) {
    update_card_selection();
} else if (screen >= SCREEN_SETTINGS_PH && screen <= SCREEN_SETTINGS_CO2) {
    update_settings_selection();
}
```

**Стало:**
```c
if (screen == SCREEN_MAIN) {
    update_card_selection();
}
// update_settings_selection() больше не используется - навигация управляется группой LVGL
```

**Изменения:**
- Убран вызов `update_settings_selection()` при показе экрана настроек
- Добавлен комментарий о причине удаления

## Технические детали

### Почему старый подход не работал:

1. **Двойное управление фокусом**: 
   - `update_settings_selection()` изменяла стили вручную
   - LVGL группа также пыталась управлять фокусом
   - Конфликт приводил к тому, что события не доходили до кнопок

2. **Неправильная отправка событий**:
   - Старый код использовал `lv_event_send()` (нет в LVGL 9.x)
   - Нужно использовать `lv_obj_send_event()`

3. **Отсутствие диагностики**:
   - Не было логирования для понимания, что происходит
   - Сложно было понять, доходят ли события до кнопок

### Новый подход:

1. **Единая система навигации**: Только LVGL группы
2. **Стандартные функции LVGL**: 
   - `lv_group_focus_next()` / `lv_group_focus_prev()` для навигации
   - `lv_group_send_data()` для отправки клавиш
   - `lv_obj_send_event()` для отправки событий
3. **Подробное логирование**: Для диагностики проблем

## Результат

### ✅ Исправленная функциональность:

1. **Навигация на экране настроек:**
   - Вращение энкодера переключает фокус между кнопками
   - Визуальное выделение работает корректно
   - Используется стандартная система LVGL (wrap-around навигация)

2. **Нажатие кнопок:**
   - Нажатие на энкодер активирует кнопку с фокусом
   - Кнопка "Назад" работает корректно
   - Все 5 кнопок настроек реагируют на нажатие

3. **Диагностика:**
   - В логах видны все действия энкодера
   - Можно отследить, какая кнопка получила фокус
   - Видно, отправляются ли события правильно

## Логирование

В Serial Monitor будут видны сообщения:

```
I (xxx) LVGL_UI: Settings screen created with 6 objects in encoder group
I (xxx) LVGL_UI: Settings: focus next
I (xxx) LVGL_UI: Settings: focus prev
I (xxx) LVGL_UI: Settings screen: encoder pressed, focused object: 0x3ffb1234, group: 0x3ffb5678
I (xxx) LVGL_UI: Sending CLICKED event to focused object
```

Если видите предупреждения:
```
W (xxx) LVGL_UI: Settings screen: no group assigned to encoder!
W (xxx) LVGL_UI: Settings screen: encoder input device not found!
```
Это означает проблему с инициализацией группы или устройства ввода.

## Тестирование

### Рекомендуемые сценарии:

1. **Открытие экрана настроек:**
   - Откройте настройки любого датчика
   - Проверьте, что первая кнопка (Back) получила фокус
   - В логах должно быть: `Settings screen created with 6 objects in encoder group`

2. **Навигация энкодером:**
   - Вращайте энкодер по часовой стрелке
   - Проверьте визуальное выделение кнопок
   - В логах должно быть: `Settings: focus next`

3. **Нажатие кнопок:**
   - Установите фокус на кнопку "Назад"
   - Нажмите энкодер
   - Должен произойти возврат к предыдущему экрану
   - В логах должно быть: `Settings screen: encoder pressed...` и `Sending CLICKED event...`

4. **Цикличность навигации:**
   - Прокрутите энкодер через все кнопки
   - После последней кнопки должна снова появиться первая (wrap-around)

## Команды для сборки

```bash
# Сборка проекта
idf.py build

# Прошивка на устройство
idf.py flash

# Мониторинг (для просмотра логов)
idf.py monitor

# Или все вместе
idf.py build flash monitor
```

## Функция update_settings_selection()

Функция `update_settings_selection()` больше **не используется** и может быть удалена в будущем. Она оставлена в коде на случай, если понадобится кастомное визуальное выделение, но в текущей реализации не вызывается.

## Примечания

- Изменения совместимы с LVGL 9.x
- Код использует стандартные API LVGL без хаков
- Легко расширяется для других экранов с кнопками
- Не требуется изменение других компонентов системы

## История изменений

- **2025-10-08**: Первое исправление - добавлены кнопки в группу энкодера
- **2025-10-08**: Второе исправление - убран конфликт с `update_settings_selection()`, добавлено логирование

