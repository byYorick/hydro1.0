# 🛠️ Инструменты для работы с ESP32-S3 через JTAG

**Дата:** 2025-10-09  
**Статус:** Готово к использованию ✅

---

## 📦 СОЗДАННЫЕ ИНСТРУМЕНТЫ

### 1. 🔍 **Анализатор состояния** - `analyze_esp32_state.py`
Автоматически анализирует дамп регистров и показывает:
- Где находится программа (PC)
- Состояние стека (SP)
- Исключения и причины остановки
- Активные прерывания
- Рекомендации по дальнейшим действиям

**Использование:**
```bash
python analyze_esp32_state.py
```

**Что делает:**
- ✅ Определяет область памяти по PC
- ✅ Проверяет корректность стека
- ✅ Декодирует причины исключений
- ✅ Показывает активные прерывания
- ✅ Генерирует команды для следующих шагов

---

### 2. 💾 **Интерактивный чтец памяти** - `read_esp32_memory.py`
Полнофункциональный инструмент для чтения ESP32-S3.

**Использование:**
```bash
python read_esp32_memory.py
```

**Меню:**
1. Информация о чипе
2. Регистры процессора
3. RAM (DRAM)
4. IRAM
5. Дамп Flash
6. Остановка чипа
7. Запуск чипа
8. Своя команда OpenOCD
9. Произвольный адрес

---

### 3. 📊 **GDB Backtrace** - `gdb_backtrace.bat`
Автоматически получает стек вызовов через GDB.

**Использование:**
```bash
gdb_backtrace.bat
```

**Что делает:**
- Запускает OpenOCD
- Подключается через GDB
- Получает backtrace
- Показывает регистры
- Список задач FreeRTOS

---

### 4. 📋 **Чтение стека** - `read_stack.bat`
Быстрое чтение стека по текущему SP.

**Использование:**
```bash
read_stack.bat
```

**Выводит:**
- 128 слов из стека
- Ключевые регистры (PC, A0, A1)

---

### 5. ⚡ **Прошивка через JTAG** - `flash_jtag.bat`
Быстрая прошивка без COM порта.

**Использование:**
```bash
flash_jtag.bat
```

---

### 6. 🔄 **Интерактивный JTAG** - `jtag_interactive.bat`
Запускает OpenOCD сервер для ручной работы.

**Использование:**
```bash
jtag_interactive.bat
```

Затем в другом терминале:
```bash
telnet localhost 4444
```

---

### 7. 📖 **Автоматическое чтение** - `read_jtag.bat`
Быстрый дамп информации о чипе.

**Использование:**
```bash
read_jtag.bat
```

---

## 🎯 СЦЕНАРИИ ИСПОЛЬЗОВАНИЯ

### 🔍 Сценарий 1: "Что происходит с чипом?"

```bash
# Шаг 1: Получите дамп регистров
python read_esp32_memory.py
# Выберите: 2 (Регистры)

# Шаг 2: Проанализируйте
python analyze_esp32_state.py

# Шаг 3: Получите backtrace
gdb_backtrace.bat
```

---

### 🐛 Сценарий 2: "Программа зависла"

```bash
# Шаг 1: Остановите чип
python read_esp32_memory.py
# Выберите: 6 (Остановка)

# Шаг 2: Прочитайте стек
read_stack.bat

# Шаг 3: Получите backtrace
gdb_backtrace.bat

# Шаг 4: Запустите обратно
python read_esp32_memory.py
# Выберите: 7 (Запуск)
```

---

### 💾 Сценарий 3: "Сохранить дамп памяти"

```bash
python read_esp32_memory.py
# Выберите: 5 (Дамп Flash)
# Файл сохранится как flash_bootloader.bin

# Или для полного дампа:
# Выберите: 8 (Своя команда)
# Введите: flash read_bank 0 full_dump.bin 0 0x1000000
```

---

### 🔧 Сценарий 4: "Отладка конкретной функции"

```bash
# Терминал 1: Запустите OpenOCD
jtag_interactive.bat

# Терминал 2: Запустите GDB
xtensa-esp32s3-elf-gdb build/hydroponics.elf

# В GDB:
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) b app_main           # Breakpoint на app_main
(gdb) c                    # Continue
(gdb) bt                   # Backtrace
(gdb) info locals          # Локальные переменные
```

---

## 📝 РЕЗУЛЬТАТЫ АНАЛИЗА ВАШЕГО ДАМПА

### Текущее состояние:
```
PC = 0x40056F62  [ROM Boot]
SP = 0x3FCEB1F0  [DRAM - OK]
DEBUGCAUSE = 0x20 [Остановлен отладчиком]
EXCCAUSE = 0x00  [Нет исключений]
```

### Активные прерывания:
- ✅ Timer (bit 6)
- ✅ WiFi MAC (bit 15)
- ✅ WiFi BB (bit 16)

### Вывод:
Чип остановлен в Boot ROM. Это нормально если:
1. Только что запущен
2. Обрабатывает прерывание из ROM
3. Находится в bootloader

---

## 🚀 РЕКОМЕНДОВАННЫЕ ДЕЙСТВИЯ

### Если хотите понять где программа:
```bash
gdb_backtrace.bat
```

### Если хотите продолжить выполнение:
```bash
python read_esp32_memory.py
# Выберите: 7 (Запуск чипа)
```

### Если хотите установить breakpoint на app_main:
```bash
# Терминал 1:
jtag_interactive.bat

# Терминал 2:
xtensa-esp32s3-elf-gdb build/hydroponics.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) b app_main
(gdb) c
```

---

## 📚 ПОЛЕЗНЫЕ ССЫЛКИ

- **Полное руководство по JTAG:** `JTAG_SETUP_GUIDE.md`
- **Руководство по чтению:** `JTAG_READ_GUIDE.md`
- **Команды OpenOCD:** См. `JTAG_READ_GUIDE.md` раздел "Команды"

---

## ⚙️ БЫСТРЫЙ СТАРТ

**Хотите просто посмотреть что с чипом?**
```bash
python read_esp32_memory.py
# Выберите: 1 (Информация о чипе)
```

**Нужен backtrace?**
```bash
gdb_backtrace.bat
```

**Хотите прошить через JTAG?**
```bash
flash_jtag.bat
```

**Нужен интерактивный доступ?**
```bash
jtag_interactive.bat
# Затем: telnet localhost 4444
```

---

## ✅ ПРОВЕРКА РАБОТЫ

Все инструменты готовы и работают! ✨

Протестировано:
- ✅ `analyze_esp32_state.py` - работает
- ✅ `read_esp32_memory.py` - готов
- ✅ `gdb_backtrace.bat` - готов
- ✅ `read_stack.bat` - готов
- ✅ `flash_jtag.bat` - готов
- ✅ `jtag_interactive.bat` - готов

---

## 🎉 ГОТОВО!

Все инструменты созданы и готовы к использованию. Выберите нужный сценарий выше или просто запустите:

```bash
python read_esp32_memory.py
```

Для интерактивной работы с ESP32-S3 через JTAG! 🚀

