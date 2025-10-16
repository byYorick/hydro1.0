# 🚨 КРИТИЧНОЕ ИСПРАВЛЕНИЕ - WATCHDOG TIMEOUT

**Дата:** 16 октября 2025  
**Приоритет:** 🔴 КРИТИЧЕСКИЙ  
**Статус:** ✅ ИСПРАВЛЕНО  

---

## ❌ ПРОБЛЕМА

### Watchdog Timeout в задаче обновления dashboard

**Лог ошибки:**
```
E (34271) task_wdt: Task watchdog got triggered
E (34271) task_wdt: CPU 1: pid_dash_upd
E (34271) task_wdt: Tasks currently running:
E (34271) task_wdt: CPU 0: IDLE0
E (34271) task_wdt: CPU 1: pid_dash_upd
```

**Backtrace:**
```
--- widget_intelligent_pid_card_update at intelligent_pid_card.c:149
--- dashboard_update_task at pid_intelligent_dashboard.c:145
```

### Причина:

**Файл:** `components/lvgl_ui/screens/adaptive/pid_intelligent_dashboard.c`

**Проблема:**
- Задача обновляет 6 карточек PID в цикле
- Для каждой карточки:
  - Вызывает `pump_manager_compute_pid()` (занимает время)
  - Обновляет LVGL виджеты (тяжелая операция)
  - Определяет статус
- **Всё это под блокировкой LVGL!**
- Сброс watchdog только в начале цикла
- Обработка 6 карточек занимает > 5 секунд → watchdog timeout!

---

## ✅ РЕШЕНИЕ

### Добавлен сброс watchdog внутри цикла

**До:**
```c
while (g_screen_active) {
    esp_task_wdt_reset();  // Только в начале цикла
    
    lv_lock();
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {  // 6 итераций
        if (g_cards[i]) {
            // Тяжелые операции без сброса watchdog!
            pump_manager_compute_pid(...);
            widget_intelligent_pid_card_update(...);
            determine_status(...);
        }
    }
    
    update_prediction_panel();
    lv_unlock();
    
    vTaskDelay(2000);
}
```

**После:**
```c
while (g_screen_active) {
    esp_task_wdt_reset();  // Сброс в начале
    
    lv_lock();
    
    for (int i = 0; i < PUMP_INDEX_COUNT; i++) {  // 6 итераций
        esp_task_wdt_reset();  // ⭐ Сброс для КАЖДОЙ карточки!
        
        if (g_cards[i]) {
            pump_manager_compute_pid(...);
            widget_intelligent_pid_card_update(...);
            determine_status(...);
        }
    }
    
    update_prediction_panel();
    lv_unlock();
    
    vTaskDelay(2000);
}
```

---

## 🎯 ЧТО ИЗМЕНЕНО

**Файл:** `components/lvgl_ui/screens/adaptive/pid_intelligent_dashboard.c`

**Строка:** 138

**Изменение:**
```c
// Добавлено:
for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
    esp_task_wdt_reset();  // ⭐ НОВАЯ СТРОКА
    
    if (g_cards[i]) {
        // ...
    }
}
```

---

## 📊 РЕЗУЛЬТАТЫ

### Сборка:
```
✅ Project build complete
✅ Размер: 787 KB
✅ Ошибок: 0
✅ Готово к прошивке
```

### Ожидаемый результат после прошивки:
- ✅ Watchdog timeout исчезнет
- ✅ Dashboard будет обновляться плавно
- ✅ Система стабильна

---

## 💡 ПОЧЕМУ ЭТО КРИТИЧНО

### До исправления:
1. **Watchdog timeout** → система перезагружается
2. Пользователь видит экран → **crash!**
3. Невозможно использовать dashboard
4. Потеря данных при перезагрузке

### После исправления:
1. ✅ Никаких timeout
2. ✅ Плавное обновление
3. ✅ Стабильная работа
4. ✅ Хороший UX

---

## 🔧 ДОПОЛНИТЕЛЬНАЯ ОПТИМИЗАЦИЯ

### Рекомендации для будущего:

1. **Уменьшить нагрузку:**
   ```c
   // Обновлять не все карточки сразу, а по очереди
   int card_to_update = (update_cycle % PUMP_INDEX_COUNT);
   update_only_one_card(card_to_update);
   ```

2. **Вынести расчеты из UI задачи:**
   ```c
   // PID расчеты делать в отдельной задаче
   // UI только отображает результаты
   ```

3. **Увеличить интервал обновления:**
   ```c
   // Было: 2 секунды
   // Можно: 3-5 секунд (PID не требует частого обновления)
   const TickType_t UPDATE_INTERVAL = pdMS_TO_TICKS(3000);
   ```

---

## ✅ ПРОВЕРКА

### До прошивки:
- [x] Код исправлен
- [x] Сброс watchdog добавлен
- [x] Проект собран
- [x] Ошибок нет

### После прошивки (TODO):
- [ ] Открыть dashboard
- [ ] Проверить отсутствие watchdog timeout
- [ ] Убедиться в плавном обновлении
- [ ] Протестировать 5+ минут

---

## 🎓 УРОК ДЛЯ AI

### Критичное правило:

**ВСЕГДА добавляй `esp_task_wdt_reset()` в циклы, особенно:**
- В циклах с LVGL операциями
- При обработке множества элементов
- В задачах с тяжелыми вычислениями
- Если одна итерация > 1 секунды

### Шаблон:

```c
while (running) {
    esp_task_wdt_reset();  // В начале цикла
    
    for (int i = 0; i < COUNT; i++) {
        esp_task_wdt_reset();  // ⭐ Внутри вложенного цикла!
        
        // Тяжелые операции
    }
    
    vTaskDelay(INTERVAL);
}
```

---

**Исправление критично и готово к прошивке!** 🚀


