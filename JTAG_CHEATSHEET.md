# ⚡ JTAG Шпаргалка - ESP32-S3

## 🔥 ТРИ КОМАНДЫ КОТОРЫЕ ВЫ БУДЕТЕ ИСПОЛЬЗОВАТЬ

### 1️⃣ Собрать данные о баге
```bash
collect_crash_data.bat
```
→ Создаст `crash_report_*.txt`  
→ Скопируйте файл и отправьте в чат

### 2️⃣ Интерактивная работа с памятью
```bash
python read_esp32_memory.py
```
→ Меню: chip info, регистры, RAM, Flash, etc.

### 3️⃣ Прошить через JTAG
```bash
flash_jtag.bat
```
→ Быстрее UART, не требует свободного COM

---

## 📝 КАК СООБЩИТЬ О БАГЕ

```bash
# Шаг 1: Собрать данные
collect_crash_data.bat

# Шаг 2: В чат написать:
```
```
Баг: [описание в 1 предложение]

Воспроизведение:
1. [шаг]
2. [шаг]
3. КРАШ/ЗАВИСАНИЕ

[вставить содержимое crash_report_*.txt]
```

---

## 🎯 БЫСТРЫЕ КОМАНДЫ OpenOCD

```bash
# Запустить сервер
jtag_interactive.bat

# В другом терминале
telnet localhost 4444
```

```
> reset halt              # Остановить
> reset run               # Запустить
> reg                     # Регистры
> mdw 0x3FC88000 32      # RAM
> esp chip_info           # Инфо
```

---

## 🛠️ БЫСТРЫЕ КОМАНДЫ GDB

```bash
# Запустить GDB
xtensa-esp32s3-elf-gdb build/hydroponics.elf
(gdb) target remote localhost:3333
```

```gdb
bt                        # Стек вызовов
info threads              # Задачи FreeRTOS
p variable_name           # Переменная
x/32xw $sp               # Стек
b app_main               # Breakpoint
c                        # Continue
```

---

## 📊 КАРТА ПАМЯТИ (основное)

| Адрес | Что | Читать |
|-------|-----|--------|
| `0x3FC88000` | DRAM | `mdw 0x3FC88000 64` |
| `0x40370000` | IRAM | `mdw 0x40370000 64` |
| `0x40056F62` | ROM | Boot ROM код |

---

## 🆘 ЕСЛИ ЧТО-ТО НЕ РАБОТАЕТ

```bash
# Убить все процессы
taskkill /F /IM openocd.exe
taskkill /F /IM gdb.exe

# Переподключить USB
# Попробовать снова
```

---

## 📚 ПОЛНАЯ ДОКУМЕНТАЦИЯ

- **Начните здесь:** `JTAG_DEBUG_INDEX.md`
- **Как сообщать о багах:** `HOW_TO_REPORT_BUGS.md`
- **Все команды:** `JTAG_READ_GUIDE.md`

---

## 💡 СОВЕТ ДНЯ

Если не знаете что делать - просто запустите:
```bash
collect_crash_data.bat
```
И отправьте результат в чат! Я разберусь! 🚀

