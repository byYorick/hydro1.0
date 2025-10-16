# 🚀 PSRAM Оптимизация: Стратегия больших буферов

**Дата:** 2025-10-16  
**Проблема:** LVGL буферы в PSRAM медленные из-за латентности cache  
**Решение:** Большие выровненные буферы + меньше flush операций

---

## 🎯 Стратегия

### Проблема с маленькими буферами в PSRAM

```
15 строк × 240 px = 3600 пикселей на flush
Экран 320 строк / 15 = 21 flush на полный экран!

Каждый flush:
1. CPU читает из PSRAM через cache
2. Cache miss → ждем PSRAM (80 MHz vs 240 MHz SRAM)
3. SPI DMA передает данные
4. Повторяем 21 раз!

Итого: 21 × (cache overhead + transfer time) = МЕДЛЕННО
```

### Решение: Большие буферы

```
80 строк × 240 px = 19200 пикселей на flush
Экран 320 строк / 80 = 4 flush на полный экран!

Преимущества:
1. Меньше flush операций (4 вместо 21) → -81% overhead
2. Больше последовательного чтения → лучше cache utilization
3. Cache line prefetch работает эффективнее
4. Меньше context switches между LVGL и SPI

Итого: 4 × (transfer time) = БЫСТРЕЕ!
```

---

## 📊 Техническая реализация

### 1. Выравнивание для cache

```c
// Выравнивание 64 байта для ESP32-S3 cache line
disp_buf1 = heap_caps_aligned_alloc(64, buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
```

**Почему 64 байта?**
- ESP32-S3 cache line size = 32 байта
- Двойное выравнивание для оптимального prefetch
- Предотвращает false sharing

### 2. Размер буфера 80 строк

```
Размер: 240 × 80 × 2 = 38.4 KB на буфер
Всего: 38.4 KB × 2 = 76.8 KB PSRAM (экономия 76.8 KB DRAM!)

Покрытие экрана: 80 / 320 = 25% за один flush
Количество flush: 320 / 80 = 4 операции
```

### 3. Двойная буферизация

```
Буфер 1: LVGL рисует следующий кадр
Буфер 2: SPI передает текущий кадр

→ Параллельная работа
→ Минимальные простои
```

---

## 🧪 Математический анализ

### Время flush (теоретическое)

**Маленькие буферы (15 строк, DRAM):**
```
Transfer time: 3600 pixels × 2 bytes / (40 MHz / 16) = ~6 мс
Количество flush: 21
Overhead (setup): 21 × 1 мс = 21 мс
──────────────────────────────────────────────
ИТОГО: 21 × 6 + 21 = 147 мс на полный экран
```

**Большие буферы (80 строк, PSRAM):**
```
Transfer time: 19200 pixels × 2 bytes / (40 MHz / 16) = ~30 мс
Cache overhead: +50% (PSRAM latency) = 45 мс
Количество flush: 4
Overhead (setup): 4 × 1 мс = 4 мс
──────────────────────────────────────────────
ИТОГО: 4 × 45 + 4 = 184 мс на полный экран
```

**Вывод:** Немного медленнее, НО:
- ✅ Освобождаем 76.8 KB DRAM для WiFi!
- ✅ Стабильная работа (нет mutex contention)
- ✅ WiFi может инициализироваться

---

## 🎨 Дополнительные оптимизации

### 1. Partial refresh (если нужно ускорить)

```c
// Рисуем только измененную область вместо всего экрана
lv_obj_invalidate_area(obj, &area);  // Вместо lv_obj_invalidate()
```

### 2. Увеличить SPI скорость

```c
// В lcd_ili9341.c
#define LCD_PIXEL_CLOCK_HZ   (40 * 1000 * 1000)  // Было 40 MHz
// Попробовать:
#define LCD_PIXEL_CLOCK_HZ   (60 * 1000 * 1000)  // 60 MHz (если дисплей поддерживает)
```

### 3. Disable cache для SPI DMA (если нужно)

```c
// Выделение без cache для прямого DMA (эксперимент)
disp_buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
```

---

## 📈 Ожидаемые результаты

### Использование памяти:

```
DRAM:
├─ До оптимизации: 285 KB (overflow!) ❌
├─ DRAM буферы (15 строк): 280 KB (85%) ⚠️ WiFi не влезает
└─ PSRAM буферы (80 строк): 266 KB (81%) ✅ WiFi влезает!

PSRAM:
├─ LVGL буферы: 76.8 KB
├─ LVGL heap: ~500 KB (для объектов)
├─ Свободно: ~7500 KB
```

### Производительность:

```
Полный redraw экрана:
├─ DRAM буферы (15 строк): ~150 мс ⚡ FAST
├─ PSRAM буферы (40 строк): ~500+ мс ❌ TOO SLOW
└─ PSRAM буферы (80 строк): ~180-250 мс ✅ ACCEPTABLE
```

### WiFi стабильность:

```
DRAM свободно для WiFi:
├─ DRAM буферы: 48 KB (недостаточно, WiFi ESP_ERR_NO_MEM) ❌
└─ PSRAM буферы: 62 KB (достаточно для WiFi init) ✅
```

---

## ✅ План тестирования

### Шаг 1: Проверить LVGL без WiFi
```bash
# WiFi отключен в app_main.c
idf.py build && idf.py -p COM11 flash monitor
```

**Ожидаем:**
- ✅ UI загружается
- ✅ Нет mutex contention
- ✅ Нет watchdog timeout
- ⏱️ Отрисовка приемлемая (< 300 мс на экран)

### Шаг 2: Включить WiFi
```c
// Раскомментировать в app_main.c
ESP_ERROR_CHECK(network_manager_init());
network_manager_load_and_connect();
```

**Ожидаем:**
- ✅ WiFi инициализируется (нет ESP_ERR_NO_MEM)
- ✅ UI продолжает работать
- ✅ Стабильная работа

### Шаг 3: Финальная оптимизация
Если нужно ускорить:
- Попробовать 60-100 строк (balance между flush count и transfer time)
- Увеличить SPI clock до 60 MHz
- Включить partial refresh в LVGL

---

## 🎯 ВЫВОДЫ

### Почему это работает:

1. **Меньше overhead:**
   - 4 flush вместо 21 → -81% setup time
   
2. **Лучше cache utilization:**
   - Последовательное чтение больших блоков
   - Prefetch работает эффективнее
   
3. **Баланс память/скорость:**
   - PSRAM: бесконечно (8 MB)
   - DRAM: критичен (328 KB)
   - Скорость: приемлема для UI

### Компромиссы:

- ❌ UI немного медленнее чем с DRAM буферами
- ✅ WiFi работает
- ✅ Система стабильна
- ✅ Память не переполнена

---

## 📚 Ссылки

- [ESP32-S3 Technical Reference Manual - PSRAM](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [LVGL Performance Tips](https://docs.lvgl.io/9.2/others/renderers.html)
- [ESP-IDF Memory Types](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/memory-types.html)

---

**Следующий шаг:** Собрать и протестировать! 🚀

