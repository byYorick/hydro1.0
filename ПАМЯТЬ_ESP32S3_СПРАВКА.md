# 📊 Память ESP32-S3: Полный справочник

## 🎯 Краткая сводка

ESP32-S3 имеет **сложную многоуровневую архитектуру памяти**:

| Тип памяти | Размер | Скорость | Назначение |
|------------|--------|----------|------------|
| **SRAM (внутренняя)** | ~512 KB | ⚡ Очень быстро | Критичные данные/код |
| **PSRAM (внешняя)** | 2-8 MB | 🐌 Медленно | Буферы, кэши |
| **Flash (внешняя)** | 4-16 MB | 📖 Только чтение | Прошивка, константы |
| **RTC Memory** | 16 KB | ⚡ Быстро | Deep Sleep данные |

---

## 🧠 1. SRAM (Внутренняя статическая память) - ~512 KB

### Разделение SRAM:

```
┌─────────────────────────────────────┐
│   IRAM (Instruction RAM)            │  ~200 KB
│   - Код ISR (прерывания)            │
│   - WiFi/BT драйверы                │
│   - Критичный код (.iram)           │
├─────────────────────────────────────┤
│   DRAM (Data RAM)                   │  ~200 KB ⚠️ ПРОБЛЕМА ЗДЕСЬ!
│   - Heap (malloc)                   │
│   - Stack (стеки задач)             │
│   - .data (инициализированные)      │
│   - .bss (неинициализированные)     │
├─────────────────────────────────────┤
│   Cache для Flash                   │  ~64 KB
│   - Кэш кода                        │
│   - Кэш данных (32KB в вашем cfg)   │
└─────────────────────────────────────┘
```

### ⚠️ DRAM - самый критичный ресурс!

**Что занимает DRAM:**
1. **WiFi драйвер** - ~10-15 KB (RX/TX буферы)
2. **Стеки FreeRTOS задач** - ~5-10 KB на задачу
3. **Глобальные переменные** (.bss, .data)
4. **Heap** (malloc/free)

**В вашем проекте DRAM переполнен на ~10 KB** из-за:
- WiFi драйвер требует ~10 KB буферов
- Много глобальных массивов (например, `g_scanned_networks[16][33]` = 528 байт)

---

## 💾 2. PSRAM (Внешняя PSRAM/SPIRAM) - 2-8 MB

### Ваши настройки (sdkconfig):

```ini
CONFIG_SPIRAM=y                           # ✅ Включено
CONFIG_SPIRAM_MODE_OCT=y                  # ✅ 8-битная шина (быстрее)
CONFIG_SPIRAM_SPEED_80M=y                 # ✅ 80 MHz
CONFIG_SPIRAM_USE_MALLOC=y                # ✅ Автоматически для malloc
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y  # ✅ BSS в PSRAM
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y    # ✅ WiFi буферы в PSRAM
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768  # Резерв 32KB DRAM
```

### Что хранится в PSRAM:

```
┌─────────────────────────────────────┐
│   Heap (большие malloc)             │  ~2-8 MB
│   - LVGL буферы (2x 320x240x2)      │  ~300 KB
│   - Большие массивы                 │
│   - Если > 16KB → автоматически     │
├─────────────────────────────────────┤
│   BSS сегмент (если включено)       │  ~10-50 KB
│   - Глобальные переменные           │
│   - Статические массивы             │
│   - WiFi буферы (если влезут)       │
└─────────────────────────────────────┘
```

### ⚡ Скорость PSRAM:

- **SRAM**: ~240 МГц, 1 цикл
- **PSRAM (OCT 80MHz)**: ~80 МГц, 4-8 циклов
- **Разница**: ~3-8x медленнее

**Правило:** Критичные данные (WiFi, ISR) → DRAM, буферы UI → PSRAM

---

## 📀 3. Flash память - 4-16 MB

### Ваша таблица разделов (`partitions.csv`):

```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     ,        0x6000     # 24 KB - NVS хранилище
phy_init, data, phy,     ,        0x1000     # 4 KB - WiFi калибровка
factory,  app,  factory, ,        4M         # 4 MB - приложение (было 2M)
```

**Всего доступно: ~4 MB для прошивки**

### Что хранится во Flash:

```
┌─────────────────────────────────────┐
│   .text (код программы)             │  ~1-2 MB
│   - Все функции                     │
│   - Библиотеки (LVGL, ESP-IDF)      │
├─────────────────────────────────────┤
│   .rodata (константы)               │  ~500 KB
│   - Строки ("Hello")                │
│   - const переменные                │
│   - Таблицы, шрифты                 │
├─────────────────────────────────────┤
│   NVS (энергонезависимое)           │  24 KB
│   - WiFi credentials                │
│   - Настройки пользователя          │
└─────────────────────────────────────┘
```

---

## 🔥 4. Проблема DRAM overflow в вашем проекте

### Текущая ситуация:

```
DRAM доступно:     ~200 KB (328 KB - резервы)
DRAM занято:       ~210 KB (переполнение 10 KB)
```

### Что съедает DRAM:

| Компонент | Размер | Решение |
|-----------|--------|---------|
| WiFi драйвер (RX/TX буферы) | ~10 KB | ✅ Уменьшены буферы |
| WiFi driver (BSS сегмент) | ~10 KB | ✅ Переносим в PSRAM |
| Стеки FreeRTOS (~10 задач) | ~50 KB | ⚠️ Оптимизировать |
| Глобальные массивы | ~20 KB | ✅ Динамическое выделение |
| Heap фрагментация | ~20 KB | Резерв |

### ✅ Уже сделано:

1. **Уменьшены WiFi буферы** (sdkconfig):
   ```ini
   CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4     # было 10
   CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8    # было 32
   CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=8    # было 32
   ```

2. **Включен PSRAM для BSS**:
   ```ini
   CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
   CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
   ```

3. **Динамическое выделение** вместо статических массивов:
   ```c
   // Было: char g_scanned_networks[16][33];  // 528 байт DRAM
   // Стало: char **g_scanned_networks = NULL;  // 4 байта DRAM
   ```

---

## 💡 Решения проблемы DRAM

### 🎯 Рекомендуемое решение: Ленивая инициализация WiFi

**Проблема:** WiFi драйвер занимает ~10 KB даже если не используется.

**Решение:** Инициализировать только при открытии WiFi экрана:

```c
// wifi_settings_screen.c

void wifi_settings_screen_on_show(lv_obj_t *screen) {
    // Инициализируем WiFi только сейчас
    network_manager_init();
    network_manager_load_and_connect();
    
    // ... остальная логика экрана
}

void wifi_settings_screen_on_hide(lv_obj_t *screen) {
    // Освобождаем ~10 KB DRAM
    network_manager_deinit();
    
    // ... очистка UI
}
```

**Выгода:** 
- ✅ Экономия ~10 KB DRAM когда WiFi не нужен
- ✅ Можно использовать WiFi когда нужно
- ✅ Проект соберется и будет работать

### 🔧 Альтернативные решения:

#### 1. Уменьшить стеки задач
```c
// system_config.h
#define SENSOR_TASK_STACK_SIZE    (3 * 1024)  // было 4096
#define UI_TASK_STACK_SIZE        (4 * 1024)  // было 6144
```

#### 2. Отключить ненужные компоненты
```ini
# sdkconfig
CONFIG_BT_ENABLED=n              # Если Bluetooth не нужен
CONFIG_ESP_CONSOLE_UART=n        # Если консоль не нужна
```

#### 3. Перенести большие структуры в PSRAM
```c
// Вместо:
static uint8_t big_buffer[10000];

// Использовать:
static uint8_t *big_buffer;
void init() {
    big_buffer = heap_caps_malloc(10000, MALLOC_CAP_SPIRAM);
}
```

---

## 📈 Мониторинг использования памяти

### Команды для проверки:

```bash
# Анализ размера секций
idf.py size-components

# Детальная карта памяти
xtensa-esp32s3-elf-size -A build/hydro1.0.elf

# Размер по компонентам
idf.py size
```

### Лимиты памяти:

```
DRAM:
  .data size:    <30 KB  (инициализированные)
  .bss  size:    <50 KB  (неинициализированные)
  Total DRAM:    <200 KB (с учетом heap/stack)

IRAM:
  .text size:    <150 KB (код в IRAM)

Flash:
  Used:          <3.5 MB (из 4 MB)
  Reserve:       0.5 MB  (для обновлений)
```

---

## 🎯 Рекомендации для вашего проекта

### ✅ Что уже хорошо:

1. PSRAM включен и настроен (OCT 80MHz)
2. BSS сегмент в PSRAM
3. WiFi буферы оптимизированы
4. Flash увеличен до 4MB

### ⚠️ Что нужно сделать:

1. **Реализовать ленивую инициализацию WiFi** (высокий приоритет)
2. Проверить размеры стеков задач
3. Убедиться, что большие буферы выделяются в PSRAM
4. Мониторить heap fragmentation

### 📝 Правила работы с памятью:

```c
// ✅ ПРАВИЛЬНО:
// 1. Маленькие структуры (<1KB) → DRAM (быстро)
static sensor_data_t sensor_cache;

// 2. Большие буферы → PSRAM
uint8_t *lvgl_buf = heap_caps_malloc(76800, MALLOC_CAP_SPIRAM);

// 3. Критичный код → IRAM
void IRAM_ATTR critical_isr(void) { }

// 4. Константы → Flash (rodata)
static const char* TAG = "SENSOR";

// ❌ НЕПРАВИЛЬНО:
// Огромные статические массивы в DRAM
static uint8_t huge_buffer[50000];  // Переполнит DRAM!
```

---

## 🔍 Отладка проблем с памятью

### Если получаете "DRAM overflow":

1. **Проверьте размер .bss/.data**:
   ```bash
   xtensa-esp32s3-elf-nm -S -C build/hydro1.0.elf | grep -i " b " | sort -k 2
   ```

2. **Найдите большие переменные**:
   ```bash
   xtensa-esp32s3-elf-nm -S --size-sort build/hydro1.0.elf | grep " [bBdD] "
   ```

3. **Проверьте использование компонентов**:
   ```bash
   idf.py size-components
   ```

### Если получаете "Heap allocation failed":

1. Проверьте доступный heap:
   ```c
   ESP_LOGI("MEM", "Free DRAM: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
   ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
   ```

2. Используйте PSRAM для больших выделений:
   ```c
   void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
   ```

---

## 📚 Дополнительные ресурсы

- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ESP-IDF Memory Types](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/memory-types.html)
- [PSRAM Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html)

---

## ✅ Итого для вашего проекта

| Параметр | Значение | Статус |
|----------|----------|--------|
| DRAM использовано | ~210 KB / 200 KB | ❌ Переполнение 10KB |
| PSRAM доступно | ~8 MB | ✅ Много свободно |
| Flash использовано | ~2.5 MB / 4 MB | ✅ Достаточно |
| **Решение** | Ленивая init WiFi | ⭐ Рекомендуется |

**Вывод:** Реализуйте ленивую инициализацию WiFi - это освободит нужные 10 KB DRAM!

