# 🔧 ESP32-S3 JTAG Debugging - Главный индекс

**Дата:** 2025-10-09  
**Статус:** ✅ Готово к использованию

---

## 🎯 БЫСТРЫЙ СТАРТ

### 😱 У МЕНЯ БАГ! ЧТО ДЕЛАТЬ?

```bash
collect_crash_data.bat
```

Затем отправьте файл `crash_report_*.txt` в чат с описанием проблемы.

**Подробнее:** [`HOW_TO_REPORT_BUGS.md`](HOW_TO_REPORT_BUGS.md)

---

## 📚 ДОКУМЕНТАЦИЯ

### 📖 Руководства (читать по порядку)

1. **[JTAG_SETUP_GUIDE.md](JTAG_SETUP_GUIDE.md)** - Первый запуск
   - Настройка JTAG
   - Подключение ESP32-S3
   - Установка драйверов
   - Первая прошивка

2. **[JTAG_READ_GUIDE.md](JTAG_READ_GUIDE.md)** - Чтение данных
   - Как читать память
   - Команды OpenOCD
   - Карта памяти ESP32-S3
   - Примеры чтения

3. **[HOW_TO_REPORT_BUGS.md](HOW_TO_REPORT_BUGS.md)** - Отчеты о багах
   - Как собрать данные
   - Что отправлять AI
   - Примеры хороших отчетов
   - Типы багов

4. **[BUG_REPORT_TEMPLATE.md](BUG_REPORT_TEMPLATE.md)** - Шаблон отчета
   - Готовый шаблон
   - Чеклист
   - Примеры

5. **[JTAG_TOOLS_SUMMARY.md](JTAG_TOOLS_SUMMARY.md)** - Обзор инструментов
   - Все доступные инструменты
   - Сценарии использования
   - Быстрая памятка

---

## 🛠️ ИНСТРУМЕНТЫ

### 🔥 Для отладки багов (используйте первыми)

| Инструмент | Команда | Назначение |
|------------|---------|------------|
| **Сборщик данных** | `collect_crash_data.bat` | Собирает ВСЁ для анализа бага |
| **Анализатор краша** | `python analyze_crash.py` | Анализирует собранные данные |
| **Backtrace** | `gdb_backtrace.bat` | Получает стек вызовов |
| **Чтение стека** | `read_stack.bat` | Читает содержимое стека |

### 💾 Для чтения памяти

| Инструмент | Команда | Назначение |
|------------|---------|------------|
| **Интерактивный чтец** | `python read_esp32_memory.py` | Полное меню для работы с памятью |
| **Анализатор состояния** | `python analyze_esp32_state.py` | Анализирует дамп регистров |
| **Быстрое чтение** | `read_jtag.bat` | Информация о чипе |

### ⚡ Для прошивки и управления

| Инструмент | Команда | Назначение |
|------------|---------|------------|
| **Прошивка JTAG** | `flash_jtag.bat` | Быстрая прошивка без COM |
| **Интерактивный JTAG** | `jtag_interactive.bat` | OpenOCD сервер |

---

## 🎓 СЦЕНАРИИ ИСПОЛЬЗОВАНИЯ

### 🐛 Сценарий 1: У меня краш/зависание

```bash
# 1. Собрать данные
collect_crash_data.bat

# 2. Проанализировать
python analyze_crash.py

# 3. Отправить в чат
# Скопировать crash_report_*.txt + описание проблемы
```

**Что писать в чат:**
```
Баг: [краткое описание]

Воспроизведение:
1. [шаг 1]
2. [шаг 2]
3. КРАШ

[содержимое crash_report.txt]
```

---

### 🔍 Сценарий 2: Хочу посмотреть что делает программа

```bash
# Интерактивное меню
python read_esp32_memory.py

# Выберите:
# 1 - Информация о чипе
# 2 - Регистры (где программа сейчас)
# 3 - RAM
# 4 - IRAM
```

---

### 📊 Сценарий 3: Нужен детальный backtrace

```bash
# Автоматический backtrace
gdb_backtrace.bat

# ИЛИ ручной через GDB:
# Терминал 1:
jtag_interactive.bat

# Терминал 2:
xtensa-esp32s3-elf-gdb build/hydroponics.elf
(gdb) target remote localhost:3333
(gdb) bt
(gdb) info threads
(gdb) thread apply all bt
```

---

### 💾 Сценарий 4: Сохранить дамп Flash

```bash
python read_esp32_memory.py
# Выберите: 5 (Дамп Flash)

# Или для полного дампа:
# Выберите: 8 (Своя команда)
# Введите: flash read_bank 0 full_dump.bin 0 0x1000000
```

---

### ⚡ Сценарий 5: Прошить через JTAG (COM порт занят)

```bash
flash_jtag.bat

# Быстрее чем UART и не требует свободного COM порта!
```

---

## 📋 ТИПИЧНЫЕ ПРОБЛЕМЫ

### ❓ "Не могу подключиться по JTAG"

**Проверьте:**
1. ESP32-S3 подключен через USB
2. В Диспетчере устройств видно "USB JTAG/serial debug unit"
3. Нет других программ использующих OpenOCD/GDB

**Решение:**
```bash
# Убить все процессы
taskkill /F /IM openocd.exe
taskkill /F /IM gdb.exe

# Попробовать снова
python read_esp32_memory.py
```

---

### ❓ "Backtrace показывает ?? вместо функций"

**Причины:**
1. Нет символов отладки в .elf
2. Стек поврежден
3. Программа в раннем boot stage

**Решение:**
```bash
# Пересоберите с отладочными символами:
idf.py menuconfig
# Compiler options -> Optimization Level -> Debug (-Og)
# Component config -> ESP System Settings -> Panic handler -> Debug mode

idf.py build
```

---

### ❓ "crash_report.txt слишком большой"

**Решение:**
```bash
# Используйте анализатор
python analyze_crash.py crash_report_*.txt

# Отправьте только SUMMARY секцию
```

---

### ❓ "Программа работает но неправильно"

**Для логических багов:**
1. Добавьте ESP_LOGI() в подозрительные места
2. Соберите логи: `idf.py monitor > logs.txt`
3. Отправьте логи + описание что ожидали vs что получили

**Или если можете остановить в момент бага:**
```bash
collect_crash_data.bat
```

---

## 🗺️ КАРТА ПАМЯТИ ESP32-S3

| Область | Адрес | Размер | Назначение |
|---------|-------|--------|------------|
| **ROM** | `0x40000000` | 384KB | Boot ROM |
| **IRAM** | `0x40370000` | 512KB | Код (executable) |
| **DRAM** | `0x3FC88000` | 512KB | Данные + Stack |
| **Flash** | `0x42000000` | 16MB | Memory-mapped Flash |
| **RTC Fast** | `0x600FE000` | 8KB | RTC память |
| **RTC Slow** | `0x50000000` | 8KB | RTC ULP |

---

## 🎯 ЧЕКЛИСТ: ЧТО ОТПРАВЛЯТЬ ДЛЯ АНАЛИЗА

### Минимум ✅
- [ ] PC (Program Counter)
- [ ] Описание проблемы
- [ ] Что делали перед багом

### Хорошо ✅✅
- [ ] Весь `crash_report.txt` от `collect_crash_data.bat`
- [ ] Шаги воспроизведения
- [ ] Как часто происходит (всегда / иногда / один раз)

### Идеально ✅✅✅
- [ ] Crash report + анализ (`analyze_crash.py`)
- [ ] Детальные шаги воспроизведения
- [ ] Логи перед крашем
- [ ] Что меняли в коде недавно
- [ ] Предположения о причине (если есть)

---

## 🚀 КОМАНДЫ НА КАЖДЫЙ ДЕНЬ

### Базовые команды OpenOCD (через telnet)

```bash
# Подключение
telnet localhost 4444

# Основные команды
> reset halt              # Остановить чип
> reset run               # Запустить чип
> reg                     # Регистры
> mdw 0xADDRESS 32       # Читать память (words)
> esp chip_info           # Информация о чипе
```

### Базовые команды GDB

```bash
# Подключение
xtensa-esp32s3-elf-gdb build/hydroponics.elf
(gdb) target remote localhost:3333

# Основные команды
(gdb) bt                  # Backtrace
(gdb) info threads        # Задачи FreeRTOS
(gdb) p variable          # Печать переменной
(gdb) x/32xw 0xADDRESS   # Дамп памяти
(gdb) b app_main          # Breakpoint
(gdb) c                   # Continue
```

---

## 📞 ПОЛУЧИТЬ ПОМОЩЬ

### В чате с AI напишите:

**Для анализа бага:**
```
У меня баг: [описание]

Воспроизведение: [шаги]

[вставить crash_report.txt]
```

**Для вопроса по JTAG:**
```
Не могу [что]: [описание проблемы]

Пробовал: [что делали]

Вывод: [что получили]
```

**Для вопроса по коду:**
```
Не понимаю почему [проблема]

Файл: [путь к файлу]
Строка: [номер]

Код:
[фрагмент кода]

Ожидаю: [что должно быть]
Получаю: [что на самом деле]
```

---

## ✨ РЕЗЮМЕ

### Для отладки багов:
```bash
collect_crash_data.bat
```
Отправьте результат в чат!

### Для чтения памяти:
```bash
python read_esp32_memory.py
```

### Для прошивки:
```bash
flash_jtag.bat
```

---

## 📚 ВСЕ ФАЙЛЫ ПРОЕКТА

### Документация
- `JTAG_DEBUG_INDEX.md` ← вы здесь
- `JTAG_SETUP_GUIDE.md` - настройка
- `JTAG_READ_GUIDE.md` - чтение памяти
- `JTAG_TOOLS_SUMMARY.md` - обзор инструментов
- `HOW_TO_REPORT_BUGS.md` - как сообщать о багах
- `BUG_REPORT_TEMPLATE.md` - шаблон отчета

### Скрипты сбора данных
- `collect_crash_data.bat` - автосбор для анализа
- `read_jtag.bat` - быстрое чтение
- `read_stack.bat` - чтение стека
- `gdb_backtrace.bat` - автоматический backtrace

### Python инструменты
- `read_esp32_memory.py` - интерактивное чтение
- `analyze_esp32_state.py` - анализ состояния
- `analyze_crash.py` - анализ crash report

### Утилиты
- `flash_jtag.bat` - прошивка через JTAG
- `jtag_interactive.bat` - интерактивный OpenOCD

---

## 🎉 ГОТОВО К ИСПОЛЬЗОВАНИЮ!

Все инструменты готовы и протестированы! ✅

**Начните с:** [`HOW_TO_REPORT_BUGS.md`](HOW_TO_REPORT_BUGS.md) 🚀

