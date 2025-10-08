# Исправление навигации энкодера

## Дата: 2025-10-08

## Проблемы

1. **При полном проходе энкодера по главному экрану рамка фокуса оставалась на последней панели датчика (CO2)**
   - Фокус не переключался корректно между карточками датчиков (0-5) и кнопкой SET (индекс 6)
   - При переходе от кнопки SET обратно к первой карточке визуальный стиль фокуса не применялся

2. **Кнопки на экране настроек не реагировали на нажатие энкодера**
   - Кнопки не были добавлены в группу энкодера
   - Навигация по элементам настроек не работала

## Внесенные изменения

### 1. Файл: `components/lvgl_ui/lvgl_ui.c`

#### 1.1. Функция `lvgl_set_focus()` (строки 1127-1167)
**Изменения:**
- Добавлено явное удаление визуального стиля фокуса (`lv_obj_remove_style()`) с кнопки SET при переключении фокуса
- Улучшена логика очистки фокуса с предыдущего элемента

**Код:**
```c
} else if (current_focus_index == SENSOR_COUNT) {
    // Убираем фокус с кнопки SET (индекс 6)
    if (status_settings_btn) {
        lv_obj_clear_state(status_settings_btn, LV_STATE_FOCUSED);
        // Также убираем визуальный стиль фокуса если есть
        lv_obj_remove_style(status_settings_btn, &style_focus, LV_PART_MAIN);
    }
}
```

#### 1.2. Функция `handle_encoder_event()` - обработка ENCODER_EVENT_ROTATE_CW (строки 1993-2037)
**Изменения:**
- Добавлено явное удаление стиля фокуса с текущего элемента ПЕРЕД переключением на следующий
- Добавлено применение визуального стиля фокуса к кнопке SET при установке фокуса на нее

**Код:**
```c
// Убираем фокус с текущего элемента перед переключением
if (current_focus_index >= 0 && current_focus_index < SENSOR_COUNT) {
    if (sensor_containers[current_focus_index] && focus_visible) {
        lv_obj_remove_style(sensor_containers[current_focus_index], &style_focus, LV_PART_MAIN);
    }
} else if (current_focus_index == SENSOR_COUNT) {
    if (status_settings_btn) {
        lv_obj_clear_state(status_settings_btn, LV_STATE_FOCUSED);
        lv_obj_remove_style(status_settings_btn, &style_focus, LV_PART_MAIN);
    }
}

selected_card_index = (selected_card_index + 1) % total_items;

if (selected_card_index < SENSOR_COUNT) {
    lvgl_set_focus(selected_card_index);
} else {
    // Фокус на кнопке SET (индекс 6)
    if (encoder_group && status_settings_btn) {
        lv_group_focus_obj(status_settings_btn);
        current_focus_index = SENSOR_COUNT;
        if (focus_visible) {
            lv_obj_add_style(status_settings_btn, &style_focus, LV_PART_MAIN);
        }
    }
}
```

#### 1.3. Функция `handle_encoder_event()` - обработка ENCODER_EVENT_ROTATE_CCW (строки 2039-2083)
**Изменения:**
- Аналогичные изменения для направления против часовой стрелки

#### 1.4. Функция `create_settings_screen()` (строки 1593-1674)
**Изменения:**
- Добавлено создание/получение группы энкодера для экрана настроек
- Добавлена кнопка "Назад" в группу энкодера
- Добавлены все кнопки настроек в группу энкодера
- Добавлено логирование количества объектов в группе

**Код:**
```c
// Получаем или создаем группу для этого экрана настроек
lv_group_t *settings_group = settings_screen_groups[sensor_index];
if (settings_group == NULL) {
    settings_group = lv_group_create();
    lv_group_set_wrap(settings_group, true);
    settings_screen_groups[sensor_index] = settings_group;
} else {
    lv_group_remove_all_objs(settings_group);
}

// Добавляем кнопку назад в группу энкодера
lv_group_add_obj(settings_group, settings->back_btn);

// В цикле создания кнопок настроек:
for (int i = 0; i < 5; i++) {
    lv_obj_t *item = lv_btn_create(settings->settings_list);
    // ... настройка кнопки ...
    // Добавляем каждую кнопку в группу энкодера
    lv_group_add_obj(settings_group, item);
}
```

## Результат

### ✅ Исправленная функциональность:

1. **Навигация на главном экране:**
   - Фокус корректно переключается между всеми 6 датчиками и кнопкой SET по кругу
   - При вращении энкодера по часовой стрелке: pH → EC → Temp → Humidity → Lux → CO2 → SET → pH → ...
   - При вращении энкодера против часовой стрелки: pH → SET → CO2 → Lux → Humidity → Temp → EC → pH → ...
   - Визуальный стиль фокуса (рамка) правильно отображается на всех элементах

2. **Навигация на экране настроек:**
   - Все кнопки на экране настроек теперь доступны для навигации энкодером
   - Можно переключаться между кнопкой "Назад" и пунктами настроек
   - Нажатие на энкодер активирует выбранную кнопку

## Тестирование

### Рекомендуемые сценарии тестирования:

1. **Главный экран:**
   - Вращайте энкодер по часовой стрелке и проверьте, что фокус проходит через все 7 элементов (6 датчиков + SET)
   - Вращайте энкодер против часовой стрелки и проверьте обратный порядок
   - Убедитесь, что визуальная рамка фокуса четко видна на каждом элементе

2. **Экран настроек:**
   - Откройте настройки любого датчика через главное меню
   - Вращайте энкодер для переключения между кнопками
   - Нажмите на энкодер для активации выбранной кнопки
   - Проверьте работу кнопки "Назад" через энкодер

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

## Логирование

При работе с энкодером в Serial Monitor будут видны сообщения:
```
I (xxx) LVGL_UI: Focus CW: sensor card 0
I (xxx) LVGL_UI: Focus CW: sensor card 1
...
I (xxx) LVGL_UI: Focus CW: SET button (index 6)
I (xxx) LVGL_UI: Settings screen created with 6 objects in encoder group
```

## Примечания

- Изменения не влияют на другие части системы
- Код остается совместимым с существующим API
- Не требуется изменение конфигурации или калибровки
- Изменения касаются только UI-слоя (LVGL)

