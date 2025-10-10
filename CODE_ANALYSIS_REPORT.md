# Отчет об анализе кода PID-системы

**Дата:** 2025-10-10  
**Статус:** Критические баги найдены

---

## 🔴 КРИТИЧЕСКИЕ ПРОБЛЕМЫ

### 1. **Утечка памяти в UI экранах** (CRITICAL)
**Файлы:** Все `*_screen.c` файлы

**Проблема:**
- При создании экранов создаются LVGL объекты
- НЕТ функций cleanup/destroy для освобождения памяти при удалении экранов
- При многократной навигации между экранами происходит утечка памяти

**Решение:**
Добавить cleanup функции в каждый экран:
```c
void pid_detail_screen_destroy(lv_obj_t *screen) {
    if (screen) {
        lv_obj_del(screen);
    }
}
```

---

### 2. **Race Condition в pump_manager_compute_and_execute** (CRITICAL)
**Файл:** `pump_manager.c:369-482`

**Проблема:**
```c
// Line 457: Мьютекс отпущен ПЕРЕД запуском насоса!
xSemaphoreGive(g_pump_mutexes[pump_idx]);

// Line 460: Насос запускается БЕЗ защиты мьютекса
esp_err_t result = run_pump_with_retry(pump_idx, duration_ms);

// Line 464: Снова берется мьютекс для обновления статистики
if (xSemaphoreTake(g_pump_mutexes[pump_idx], pdMS_TO_TICKS(1000)) == pdTRUE)
```

**Последствия:**
- Между строками 457-464 другой поток может изменить g_pump_stats
- Возможна гонка при обновлении daily_volume_ml
- Статистика может быть некорректной

**Решение:**
```c
// Не отпускать мьютекс до завершения обновления статистики
// ИЛИ использовать атомарные операции для счетчиков
```

---

### 3. **Deadlock риск в pump_manager_task** (HIGH)
**Файл:** `pump_manager.c:223-237`

**Проблема:**
```c
for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
    if (xSemaphoreTake(g_pump_mutexes[i], pdMS_TO_TICKS(1000)) == pdTRUE) {
        // ...
        xSemaphoreGive(g_pump_mutexes[i]);
    }
}
```

Если один мьютекс занят > 1 сек, loop продолжается без обработки!

**Решение:**
- Логировать failed takes
- Использовать более длинный timeout или попытки retry

---

### 4. **Неинициализированный PID integral limits** (HIGH)
**Файл:** `pump_manager.c:91-135`

**Проблема:**
```c
// Line 106-109: integral ограничивается output_max
if (pid->integral > pid->output_max) {
    pid->integral = pid->output_max;
}
```

НО должен использоваться `g_pid_configs[pump_idx].integral_max` из конфигурации!

**Решение:**
```c
// Использовать правильный лимит из конфига
float integral_limit = /* получить из g_pid_configs */;
if (pid->integral > integral_limit) {
    pid->integral = integral_limit;
}
```

---

### 5. **Утечка мьютексов при ошибке инициализации** (MEDIUM)
**Файл:** `pump_manager.c:245-326`

**Проблема:**
```c
// Line 254-260: Создаются мьютексы
for (int i = 0; i < PUMP_INDEX_COUNT; i++) {
    g_pump_mutexes[i] = xSemaphoreCreateMutex();
    if (g_pump_mutexes[i] == NULL) {
        ESP_LOGE(TAG, "Не удалось создать мьютекс для насоса %d", i);
        return ESP_ERR_NO_MEM; // <-- УТЕЧКА! Не освобождены предыдущие мьютексы
    }
}
```

**Решение:**
```c
// При ошибке освободить все созданные мьютексы
for (int j = 0; j < i; j++) {
    vSemaphoreDelete(g_pump_mutexes[j]);
}
```

---

### 6. **Неправильное использование PID integral в compute_pid_internal** (HIGH)
**Файл:** `pump_manager.c:91-135`

**Проблема:**
- PID не использует `g_pid_configs[pump_idx]` параметры:
  - `deadband` не учитывается
  - `integral_max` игнорируется (используется output_max)
  - `auto_reset_integral` не реализован
  - `use_derivative_filter` не реализован

**Решение:**
Передать `pump_idx` в `compute_pid_internal` и использовать все параметры из конфига

---

### 7. **Неиспользуемые переменные и функции** (LOW - но предупреждения компилятора)

**Файлы с warnings:**
- `pumps_manual_screen.c:31`: `g_selected_pump` unused
- `pumps_manual_screen.c:40`: `on_confirm_pump_start` unused
- `pid_graph_screen.c`: множество unused переменных
- `ph_ec_controller.c:60`: `run_pump_with_interface` unused
- `system_tasks.c:467,511`: unused functions

**Решение:**
Удалить или пометить как `__attribute__((unused))`

---

### 8. **Отсутствие защиты от повторного вызова pump_manager_init** (MEDIUM)

**Файл:** `pump_manager.c:245`

**Проблема:**
```c
if (g_initialized) {
    ESP_LOGW(TAG, "pump_manager уже инициализирован");
    return ESP_OK; // Но мьютексы и задача уже созданы!
}
```

Если вызвать дважды, мьютексы и задача НЕ удаляются, но новые НЕ создаются.

---

### 9. **Отсутствие защиты от Division by Zero** (LOW)

**Файл:** `pump_manager.c:442`

```c
uint32_t duration_ms = (uint32_t)((output.output / flow_rate) * 1000.0f);
```

Если `flow_rate == 0`, будет деление на 0!

**Решение:**
```c
if (flow_rate < 0.001f) flow_rate = 0.001f; // защита
```

---

### 10. **Неполная реализация PID порогов activation/deactivation** (HIGH)

**Файл:** `pump_manager.c:369-482`

**Проблема:**
- `activation_threshold` и `deactivation_threshold` НЕ ИСПОЛЬЗУЮТСЯ в `pump_manager_compute_and_execute`!
- PID срабатывает всегда, когда `error >= deadband`
- Пороги определены в конфиге, но игнорируются в логике

**Решение:**
Добавить проверки порогов перед запуском PID

---

## 🟡 РЕКОМЕНДАЦИИ

1. **Добавить cleanup функции для всех UI экранов**
2. **Исправить race condition с мьютексами**
3. **Использовать все параметры PID из конфига**
4. **Добавить защиту от division by zero**
5. **Реализовать пороги activation/deactivation**
6. **Освободить ресурсы при ошибках инициализации**
7. **Удалить неиспользуемые переменные**
8. **Добавить логирование failed mutex takes**

---

## 📊 Статистика

- **Критических проблем:** 2
- **Высокий приоритет:** 4
- **Средний приоритет:** 2
- **Низкий приоритет:** 2

**ИТОГО:** 10 найденных проблем


