# 🔧 АЛГОРИТМ СБОРКИ, ПРОШИВКИ И МОНИТОРИНГА

**Документ:** Описание процесса работы с ESP-IDF проектом  
**Дата:** 16 октября 2025

---

## 📋 ОГЛАВЛЕНИЕ

1. [Подготовка окружения](#1-подготовка-окружения)
2. [Процесс сборки](#2-процесс-сборки)
3. [Освобождение COM порта](#3-освобождение-com-порта)
4. [Прошивка устройства](#4-прошивка-устройства)
5. [Мониторинг и анализ логов](#5-мониторинг-и-анализ-логов)
6. [Обработка ошибок](#6-обработка-ошибок)
7. [Оптимизация процесса](#7-оптимизация-процесса)

---

## 1. ПОДГОТОВКА ОКРУЖЕНИЯ

### 1.1. Инициализация ESP-IDF

```batch
cmd /c "C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005 >nul 2>&1 && cd c:\esp\hydro\hydro1.0 && <КОМАНДА>"
```

**Параметры:**
- `idf_cmd_init.bat` - скрипт инициализации ESP-IDF окружения
- `esp-idf-1dcc643656a1439837fdf6ab63363005` - уникальный ID версии ESP-IDF
- `>nul 2>&1` - подавление вывода инициализации для чистоты логов
- `cd c:\esp\hydro\hydro1.0` - переход в директорию проекта

**Что делает:**
- Активирует Python virtual environment (`idf5.5_py3.11_env`)
- Устанавливает переменные окружения (PATH, IDF_PATH)
- Подготавливает toolchain (xtensa-esp-elf-gcc)
- Делает доступными команды `idf.py`, `esptool.py`

---

## 2. ПРОЦЕСС СБОРКИ

### 2.1. Команда сборки

```bash
idf.py build
```

### 2.2. Этапы сборки (автоматические)

#### Этап 1: CMake конфигурация
```
-- Re-running CMake...
-- Building ESP-IDF components for target esp32s3
-- Project sdkconfig file C:/esp/hydro/hydro1.0/sdkconfig
-- Configuring done (10.3s)
-- Generating done (1.5s)
```

**Анализ:**
- ✅ Успех: конфигурация завершена за ~12 секунд
- ⚠️ Warnings: игнорируем устаревшие kconfig символы
- ❌ Ошибка: проверяем наличие всех зависимостей

#### Этап 2: Компиляция компонентов
```
[1/44] Building C object esp-idf/adaptive_pid/...
[2/44] Building C object esp-idf/lvgl_ui/...
...
```

**Мониторинг:**
- Считаем количество объектов: `[X/Y]` где Y = общее количество
- Проверяем warnings:
  - `-Wunused-variable` → некритично, но желательно исправить
  - `-Wunused-function` → некритично
  - Implicit declaration → **КРИТИЧНО**, нужно исправить

#### Этап 3: Линковка
```
[40/44] Linking CXX executable hydroponics.elf
```

**Проверка:**
- ✅ Успех: `.elf` создан
- ❌ Undefined reference → отсутствует функция/компонент

#### Этап 4: Создание прошивки
```
[41/44] Generating binary image from built executable
esptool.py v4.10.dev2
Creating esp32s3 image...
Successfully created esp32s3 image.
Generated C:/esp/hydro/hydro1.0/build/hydroponics.bin
```

**Анализ размера:**
```
hydroponics.bin binary size 0xc0610 bytes. 
Smallest app partition is 0x100000 bytes. 
0x3f9f0 bytes (25%) free.
```

**Критерии:**
- ✅ Размер < 1 MB (0x100000)
- ✅ Свободно ≥ 20% → хороший запас
- ⚠️ Свободно < 10% → нужно оптимизировать
- ❌ Не влезает → увеличить partition или удалить код

### 2.3. Анализ вывода сборки

**Успешная сборка:**
```
Project build complete. To flash, run:
 idf.py flash
```

**Неудачная сборка:**
```
ninja: build stopped: subcommand failed.
ninja failed with exit code 1
```

**Действия при ошибке:**
1. Читаем лог: `build/log/idf_py_stderr_output_XXXX`
2. Ищем первую ошибку (не warning!)
3. Исправляем (missing include, undefined, syntax error)
4. Повторяем сборку

---

## 3. ОСВОБОЖДЕНИЕ COM ПОРТА

### 3.1. Проблема

COM порт может быть занят предыдущими процессами:
- `idf.py monitor` (не завершился)
- `esptool.py` (зависший)
- Другие serial терминалы

### 3.2. Команда освобождения

```batch
c:\esp\hydro\hydro1.0\free_com5.bat
```

### 3.3. Что делает скрипт

```batch
# 1. Находит процессы, использующие COM5
handle.exe COM5

# 2. Убивает процессы по PID
taskkill /PID <PID> /F

# 3. Ждёт освобождения порта
timeout /t 2

# 4. Проверяет доступность
mode COM5
```

**Анализ вывода:**
```
Killing PID: 1364
  [OK] PID 1364 terminated    ✅ Успех
  [FAIL] Could not kill       ⚠️ Процесс уже завершён или нет прав
  
COM5 is ready to use!         ✅ Порт свободен
```

### 3.4. Альтернатива (если bat не работает)

```python
python c:\esp\hydro\hydro1.0\free_com_port.py COM5
```

---

## 4. ПРОШИВКА УСТРОЙСТВА

### 4.1. Команда прошивки

```bash
idf.py -p COM5 flash
```

### 4.2. Этапы прошивки

#### Этап 1: Подключение к устройству
```
Serial port COM5
Connecting....
Chip is ESP32-S3 (QFN56) (revision v0.2)
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
```

**Проверка:**
- ✅ `Connecting....` → нажимаем BOOT на плате (если не auto-reset)
- ✅ `Chip is ESP32-S3` → правильный чип
- ✅ `PSRAM 8MB` → память доступна

**Проблемы:**
- ❌ `Could not open COM5` → порт занят (см. раздел 3)
- ❌ `Timed out waiting for packet header` → проблема с подключением
  - Решение: переподключить USB, нажать BOOT

#### Этап 2: Загрузка stub
```
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 460800
Changed.
```

**Анализ:**
- ✅ `Changed.` → скорость увеличена до 460800 (быстрая прошивка)

#### Этап 3: Стирание flash
```
Flash will be erased from 0x00000000 to 0x00006fff...
Flash will be erased from 0x00010000 to 0x000d0fff...
Flash will be erased from 0x00008000 to 0x00008fff...
```

**Что стирается:**
- `0x0` - bootloader
- `0x8000` - partition table
- `0x10000` - приложение

#### Этап 4: Запись прошивки
```
Writing at 0x00010000... (3 %)
Writing at 0x0001f7c8... (7 %)
...
Writing at 0x000cc6f7... (100 %)
Wrote 787984 bytes (436036 compressed) at 0x00010000 in 10.0 seconds
```

**Мониторинг:**
- Прогресс: 0% → 100%
- Скорость: ~628 kbit/s (нормально для 460800 baud)
- Время: ~10 секунд для 788 KB

**Проблемы:**
- ❌ `Failed to write` → проблема с USB/питанием
- ❌ `Hash of data verified` НЕ появился → повреждённая прошивка

#### Этап 5: Перезагрузка
```
Leaving...
Hard resetting via RTS pin...
Done
```

**Результат:**
- ✅ `Done` → прошивка успешна, устройство перезагружено

---

## 5. МОНИТОРИНГ И АНАЛИЗ ЛОГОВ

### 5.1. Команда мониторинга

```bash
idf.py -p COM5 monitor
```

или комбинированная:

```bash
idf.py -p COM5 flash monitor
```

### 5.2. Структура логов

#### Формат лога ESP-IDF
```
<УРОВЕНЬ> (<ВРЕМЯ_МС>) <ТЭГ>: <СООБЩЕНИЕ>
```

**Примеры:**
```
I (1234) MAIN: Система запущена          # INFO
W (2345) SENSOR: Датчик не отвечает      # WARNING
E (3456) I2C: Ошибка чтения              # ERROR
D (4567) LVGL: Обновление UI              # DEBUG
```

### 5.3. Анализ критических событий

#### 5.3.1. Watchdog Timeout (КРИТИЧНО!)
```
E (90310) task_wdt: Task watchdog got triggered
E (90310) task_wdt: CPU 1: autotune_upd
E (90310) task_wdt: Print CPU 1 backtrace

Backtrace: 0x40378326:0x3FC994D0 0x42024DDA:0x600FF570 ...
```

**Действия:**
1. Читаем backtrace → находим функцию
2. Проверяем есть ли `esp_task_wdt_reset()` в цикле
3. Увеличиваем стек задачи если нужно
4. Исправляем и пересобираем

**Пример исправления:**
```c
while (running) {
    esp_task_wdt_reset();  // ДОБАВИТЬ!
    // ... работа задачи ...
    vTaskDelay(100);
}
```

#### 5.3.2. Stack Overflow
```
***ERROR*** A stack overflow in task autotune_upd has been detected.
```

**Действия:**
1. Увеличить размер стека: `xTaskCreate(..., 4096, ...)` → `8192`
2. Добавить диагностику:
```c
UBaseType_t stack = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Свободный стек: %lu байт", stack * 4);
```

#### 5.3.3. Memory Leak (Утечка памяти)
```
I (60000) heap: Free heap: 150000 bytes
I (120000) heap: Free heap: 145000 bytes  # Уменьшается!
I (180000) heap: Free heap: 140000 bytes
```

**Действия:**
1. Проверить все `malloc()` → есть ли `free()`?
2. Проверить LVGL объекты → есть ли `LV_EVENT_DELETE` callbacks?
3. Добавить периодический лог памяти:
```c
ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
```

#### 5.3.4. Assertion Failed
```
assert failed: pvTaskIncrementMutexHeldCount tasks.c:4920
```

**Действия:**
1. Читаем файл и строку (tasks.c:4920)
2. Проверяем использование мьютексов/семафоров
3. Проверяем `lv_lock()` / `lv_unlock()` пары

### 5.4. Анализ производительности

#### CPU Load
```
I (10000) SYSTEM: CPU0: 45%, CPU1: 32%
```

**Критерии:**
- ✅ < 60% → нормально
- ⚠️ 60-80% → высокая нагрузка
- ❌ > 80% → критично, оптимизировать

#### FreeRTOS Task Stats
```
I (10000) TASK_STATS:
  Name          State  Prio  Stack  CPU
  IDLE0         R      0     1024   50%
  display_upd   B      6     4096   15%
  sensor_task   B      5     8192   10%
```

**Проверка:**
- `Stack` → если < 500 байт свободно → увеличить
- `CPU` → если одна задача > 40% → оптимизировать
- `State: R` (Running) слишком долго → бесконечный цикл?

---

## 6. ОБРАБОТКА ОШИБОК

### 6.1. Ошибки компиляции

#### Пример 1: Missing header
```
fatal error: adaptive_pid.h: No such file or directory
```

**Решение:**
```cmake
# В CMakeLists.txt добавить:
PRIV_REQUIRES adaptive_pid
```

#### Пример 2: Undefined reference
```
undefined reference to `adaptive_pid_init'
```

**Решение:**
1. Проверить наличие функции в `.c` файле
2. Проверить зависимости в CMakeLists.txt
3. Проверить правильность прототипа в `.h`

#### Пример 3: Type mismatch
```
error: conflicting types for 'on_hide'
```

**Решение:**
1. Проверить сигнатуру функции
2. Сравнить с typedef в `.h` файле
3. Исправить аргументы

### 6.2. Ошибки прошивки

#### Пример 1: Port busy
```
A fatal error occurred: Could not open COM5, the port is busy
```

**Решение:**
```batch
c:\esp\hydro\hydro1.0\free_com5.bat
```

#### Пример 2: Flash verification failed
```
Hash of data NOT verified.
```

**Решение:**
1. Переподключить USB
2. Проверить питание (5V, достаточный ток)
3. Попробовать другой USB порт
4. Уменьшить baud rate: `idf.py -p COM5 -b 115200 flash`

---

## 7. ОПТИМИЗАЦИЯ ПРОЦЕССА

### 7.1. Комбинированные команды

**Последовательное выполнение:**
```bash
# Сборка → Прошивка → Монитор (одной командой)
idf.py build && idf.py -p COM5 flash monitor
```

**Только изменённые файлы:**
```bash
# Ninja автоматически пересобирает только изменённые
idf.py build  # Быстрая инкрементальная сборка
```

**Полная пересборка (если что-то сломалось):**
```bash
idf.py fullclean
idf.py build
```

### 7.2. Фоновый мониторинг

```bash
cmd /c "... && idf.py -p COM5 flash monitor"
# Запустить в background для длительного мониторинга
```

**Использование:**
- Непрерывный мониторинг логов
- Автоматический перезапуск при crash
- Перехват UART вывода в реальном времени

---

## 8. МОЙ АЛГОРИТМ РАБОТЫ

### 8.1. Стандартный цикл разработки

```
┌─────────────────────────────────────┐
│ 1. Редактирование кода              │
│    - Изменяю .c/.h файлы            │
│    - Добавляю функции/исправления   │
└─────────────────┬───────────────────┘
                  ↓
┌─────────────────────────────────────┐
│ 2. Сборка проекта                   │
│    cmd /c "init.bat && idf.py build"│
│                                      │
│    Анализирую вывод:                │
│    ✅ "build complete" → OK          │
│    ❌ Ошибки → исправляю → повтор   │
└─────────────────┬───────────────────┘
                  ↓
┌─────────────────────────────────────┐
│ 3. Освобождение COM порта           │
│    free_com5.bat                    │
│                                      │
│    Проверяю:                        │
│    ✅ "ready to use" → OK            │
│    ⚠️ Warnings → игнорирую обычно   │
└─────────────────┬───────────────────┘
                  ↓
┌─────────────────────────────────────┐
│ 4. Прошивка устройства              │
│    idf.py -p COM5 flash             │
│                                      │
│    Мониторю:                        │
│    - Connecting → ждём              │
│    - Writing (0-100%) → прогресс    │
│    - Done → успех                   │
└─────────────────┬───────────────────┘
                  ↓
┌─────────────────────────────────────┐
│ 5. Мониторинг (опционально)         │
│    idf.py -p COM5 monitor           │
│                                      │
│    Ищу в логах:                     │
│    - E (XXXX) → ошибки              │
│    - task_wdt → watchdog            │
│    - assert → критичные баги        │
│    - Free heap → утечки памяти      │
└─────────────────┬───────────────────┘
                  ↓
┌─────────────────────────────────────┐
│ 6. Анализ и исправление             │
│    Если есть проблемы:              │
│    - Читаю backtrace                │
│    - Нахожу проблемный файл/строку  │
│    - Исправляю код                  │
│    - Возврат к шагу 2               │
└─────────────────────────────────────┘
```

### 8.2. Быстрая итерация (только код изменён)

```bash
# Всё в одной команде для скорости
cmd /c "C:\Espressif\idf_cmd_init.bat ... && cd ... && idf.py build && idf.py -p COM5 flash"
```

**Преимущества:**
- 1 команда вместо 3-4
- Автоматическая остановка при ошибке
- Экономия времени (~30 сек → 15 сек)

### 8.3. Проверка после прошивки

**Обязательно проверяю в первых 10 секундах логов:**

```
I (298) cpu_start: App "hydroponics" version: 44d2a9d-dirty
I (305) esp_psram: PSRAM initialized, available size: 8192 KB
I (312) main_task: Started on CPU0
I (320) main_task: Calling app_main()
I (325) MAIN: ========================================
I (330) MAIN:    Hydro System Starting
I (335) MAIN: ========================================
```

**Критерии успеха:**
- ✅ Версия приложения отображена
- ✅ PSRAM инициализирована
- ✅ app_main() вызвана
- ✅ Компоненты инициализируются без ошибок

**Критерии провала:**
- ❌ Перезагрузка в цикле (boot loop)
- ❌ Panic/Abort сразу после старта
- ❌ Watchdog при инициализации

---

## 9. ЧЕКЛИСТ КАЧЕСТВА ПРОШИВКИ

### Перед прошивкой:
- [ ] Сборка без ошибок
- [ ] Warnings только некритичные
- [ ] Размер < partition size
- [ ] Свободно ≥ 20% flash

### После прошивки:
- [ ] Устройство загружается (не boot loop)
- [ ] Нет watchdog timeout в первые 60 сек
- [ ] LVGL UI отображается
- [ ] Энкодер реагирует
- [ ] Датчики читаются (нет постоянных ошибок I2C)

### Длительное тестирование (5+ минут):
- [ ] Нет утечек памяти (Free heap стабильно)
- [ ] Нет watchdog timeout при работе UI
- [ ] Переключение экранов работает
- [ ] Нет crash при навигации
- [ ] Уведомления отображаются корректно

---

## 10. ТИПИЧНЫЕ ПРОБЛЕМЫ И РЕШЕНИЯ

| Проблема | Симптом | Решение |
|----------|---------|---------|
| Утечка памяти | Free heap уменьшается | Добавить `LV_EVENT_DELETE` callbacks с `free()` |
| Watchdog timeout | `task_wdt triggered` | Добавить `esp_task_wdt_reset()` в циклы |
| Stack overflow | `stack overflow detected` | Увеличить размер стека задачи (4096 → 8192) |
| Двойной фокус | Две рамки на экране | Убрать ручное `set_focused()`, доверить screen_manager |
| Нет фокуса | Элемент не выделяется | Использовать `lv_btn_create`, добавить `style_card_focused` |
| COM порт занят | `port is busy` | Запустить `free_com5.bat` |
| LVGL crash | Undefined behavior | Проверить `lv_lock()` / `lv_unlock()` в задачах |

---

## 11. КОМАНДЫ БЫСТРОГО ДОСТУПА

```bash
# Полный цикл (сборка + прошивка)
cmd /c "C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005 >nul 2>&1 && cd c:\esp\hydro\hydro1.0 && idf.py build && idf.py -p COM5 flash"

# Только сборка (проверка ошибок)
cmd /c "C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005 >nul 2>&1 && cd c:\esp\hydro\hydro1.0 && idf.py build"

# Мониторинг (после прошивки)
cmd /c "C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005 >nul 2>&1 && cd c:\esp\hydro\hydro1.0 && idf.py -p COM5 monitor"

# Очистка сборки (если что-то сломалось)
cmd /c "C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005 >nul 2>&1 && cd c:\esp\hydro\hydro1.0 && idf.py fullclean && idf.py build"

# Освобождение порта
c:\esp\hydro\hydro1.0\free_com5.bat
```

---

## 12. МОНИТОРИНГ СПЕЦИФИЧНЫХ КОМПОНЕНТОВ

### 12.1. LVGL UI

**Ключевые логи:**
```
I (X) SCREEN_MANAGER: Showing screen 'pid_intelligent_dashboard'
I (X) SCREEN_LIFECYCLE: Creating screen instance
I (X) PID_DETAIL: Детальный экран показан для pH Down
```

**Проблемы:**
- Экран не показывается → проверить регистрацию
- Двойное создание → проверить `destroy_on_hide`
- Зависание при переходе → добавить `esp_task_wdt_reset()`

### 12.2. Adaptive PID

**Ключевые логи:**
```
I (X) ADAPTIVE_PID: История обновлена для насоса 0
I (X) ADAPTIVE_PID: Предсказание: текущее=7.2, через 30 сек=6.8
I (X) ADAPTIVE_PID: Буферная ёмкость выучена: 15.5L
```

**Проблемы:**
- Не обучается → проверить условия в `adaptive_pid.c`
- Предсказание NaN → проверить историю (достаточно точек?)
- NVS не сохраняется → проверить `adaptive_pid_save_to_nvs()`

### 12.3. Датчики

**Ключевые логи:**
```
I (X) SHT3X: Temperature=24.5°C, Humidity=65.3%
W (X) TREMA_PH: CRC ошибка, повтор...
E (X) I2C: I2C transaction failed
```

**Проблемы:**
- CRC errors → плохое подключение, помехи
- I2C failed → проверить pull-up резисторы, скорость шины
- Timeout → увеличить задержки в драйвере

---

## 13. ИНСТРУМЕНТЫ ДИАГНОСТИКИ

### 13.1. Встроенные в код

```c
// Диагностика памяти
ESP_LOGI(TAG, "Free heap: %lu, largest block: %lu", 
         esp_get_free_heap_size(), 
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

// Диагностика стека
UBaseType_t stack = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Минимальный свободный стек: %lu байт", stack * 4);

// Диагностика LVGL памяти
lv_mem_monitor_t mon;
lv_mem_monitor(&mon);
ESP_LOGI(TAG, "LVGL: использовано %lu, свободно %lu", 
         mon.total_size - mon.free_size, mon.free_size);

// Диагностика задач
char task_list[1024];
vTaskList(task_list);
ESP_LOGI(TAG, "Tasks:\n%s", task_list);
```

### 13.2. ESP-IDF инструменты

```bash
# Анализ размера компонентов
idf.py size-components

# Анализ размера файлов
idf.py size-files

# Menuconfig (изменение настроек)
idf.py menuconfig
```

---

## 14. BEST PRACTICES

### 14.1. Перед каждой прошивкой

1. ✅ Сборка успешна (exit code 0)
2. ✅ Нет критичных warnings
3. ✅ COM порт освобождён
4. ✅ Устройство подключено

### 14.2. После каждой прошивки

1. ✅ Устройство загрузилось (нет boot loop)
2. ✅ Первые 10 сек без ошибок
3. ✅ UI отображается
4. ✅ Базовая функциональность работает

### 14.3. При добавлении новой функции

1. ✅ Добавить компонент в `CMakeLists.txt`
2. ✅ Проверить зависимости (REQUIRES/PRIV_REQUIRES)
3. ✅ Добавить диагностические логи
4. ✅ Протестировать на утечки памяти
5. ✅ Добавить `esp_task_wdt_reset()` в циклы
6. ✅ Проверить размер стека задачи

---

**Этот алгоритм гарантирует стабильную сборку, прошивку и диагностику проекта ESP-IDF!** 🚀✨

