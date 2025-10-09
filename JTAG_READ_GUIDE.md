# Руководство по чтению ESP32-S3 через JTAG

**Дата:** 2025-10-09  
**Задача:** Чтение памяти, регистров и flash ESP32-S3 через встроенный USB-JTAG

---

## 🎯 ЧТО МОЖНО ПРОЧИТАТЬ

✅ **RAM** - оперативная память (DRAM, IRAM)  
✅ **Flash** - содержимое флеш-памяти  
✅ **Регистры** - состояние процессора  
✅ **Периферия** - регистры устройств (I2C, SPI, GPIO, etc)  
✅ **Stack/Heap** - стек вызовов, динамическая память  

---

## 🚀 ТРИ СПОСОБА ЧТЕНИЯ

### Способ 1: Через готовый Python скрипт (РЕКОМЕНДУЕТСЯ)

```bash
python read_esp32_memory.py
```

**Меню скрипта:**
- Информация о чипе
- Чтение регистров процессора
- Чтение RAM/IRAM
- Дамп Flash
- Остановка/запуск чипа
- Произвольные команды OpenOCD

**Преимущества:**
- Интерактивное меню
- Автоматический запуск OpenOCD
- Удобный вывод результатов

---

### Способ 2: Через bat-скрипт

```bash
# Автоматическое чтение:
read_jtag.bat

# Интерактивный режим:
jtag_interactive.bat
```

`jtag_interactive.bat` запустит OpenOCD сервер, после чего вы можете подключиться через telnet:

```bash
telnet localhost 4444
```

---

### Способ 3: Вручную через OpenOCD

```bash
# 1. Запустите OpenOCD в одном терминале:
openocd -f board/esp32s3-builtin.cfg

# 2. В другом терминале подключитесь:
telnet localhost 4444

# 3. Выполняйте команды (см. ниже)
```

---

## 📋 ПОЛЕЗНЫЕ КОМАНДЫ OPENOCD

### Информация о чипе
```
> esp chip_info
```

### Чтение регистров процессора
```
> reg                    # Все регистры
> reg pc                 # Program Counter
> reg sp                 # Stack Pointer
```

### Чтение памяти

**Форматы:**
- `mdb` - byte (8 бит)
- `mdh` - halfword (16 бит)
- `mdw` - word (32 бита)

**Примеры:**
```
> mdw 0x3FC88000 32      # 32 слова из DRAM
> mdb 0x40370000 256     # 256 байт из IRAM
> mdw 0x60000000 16      # Читаем регистры GPIO
```

### Важные адреса ESP32-S3

| Область | Адрес | Описание |
|---------|-------|----------|
| **DRAM** | `0x3FC88000` - `0x3FD00000` | Оперативная память данных |
| **IRAM** | `0x40370000` - `0x403E0000` | Память инструкций |
| **ROM** | `0x40000000` - `0x40060000` | Boot ROM |
| **Flash** | `0x42000000` - ... | Маппированная Flash |
| **RTC FAST** | `0x600FE000` | RTC быстрая память |
| **RTC SLOW** | `0x50000000` | RTC медленная память |

### Периферия (примеры)

```
> mdw 0x60004000 16      # GPIO регистры
> mdw 0x60013000 16      # I2C0 регистры
> mdw 0x60024000 16      # I2C1 регистры
> mdw 0x3FF43000 16      # SPI2 регистры
```

### Дамп Flash в файл
```
> flash read_bank 0 bootloader.bin 0x0 0x10000        # Bootloader
> flash read_bank 0 partition.bin 0x8000 0x1000       # Partition table
> flash read_bank 0 app.bin 0x10000 0x200000          # Application
> flash read_bank 0 full_dump.bin 0x0 0x1000000       # Вся flash (16MB)
```

### Управление выполнением
```
> reset halt             # Остановить чип
> reset run              # Запустить чип
> halt                   # Остановить выполнение
> resume                 # Продолжить выполнение
> step                   # Выполнить одну инструкцию
```

### Точки останова
```
> bp 0x40380000 4 hw     # Hardware breakpoint
> rbp 0x40380000         # Удалить breakpoint
```

---

## 🔍 ПРАКТИЧЕСКИЕ ПРИМЕРЫ

### 1. Проверить что устройство работает
```bash
python read_esp32_memory.py
# Выбрать: 1 (Информация о чипе)
```

### 2. Посмотреть стек вызовов при краше
```bash
# В telnet к OpenOCD:
> reset halt
> reg pc                 # Адрес текущей инструкции
> reg sp                 # Указатель стека
> mdw [SP] 64           # Дамп стека
```

### 3. Прочитать переменную из RAM
```bash
# Если знаете адрес переменной (из .map файла):
> mdw 0x3FC88ABC 1      # Читаем 1 слово
```

### 4. Дамп всей Flash для анализа
```bash
python read_esp32_memory.py
# Выбрать: 5 (Дамп Flash)
# Или вручную:
> flash read_bank 0 full_flash.bin 0 0x1000000
```

### 5. Отладка зависания программы
```bash
# 1. Если программа зависла:
> halt                   # Останавливаем

# 2. Смотрим где застряла:
> reg pc                 # Текущий адрес инструкции

# 3. Смотрим стек:
> reg sp
> mdw [адрес SP] 64

# 4. Запускаем обратно:
> resume
```

---

## 🐛 ОТЛАДКА ЧЕРЕЗ GDB + JTAG

### Запуск GDB сессии

```bash
# Терминал 1: Запустите OpenOCD
openocd -f board/esp32s3-builtin.cfg

# Терминал 2: Запустите GDB
xtensa-esp32s3-elf-gdb build/hydroponics.elf

# В GDB:
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) b app_main          # Breakpoint на app_main
(gdb) c                   # Continue
```

### Полезные команды GDB
```gdb
(gdb) bt                  # Backtrace - стек вызовов
(gdb) info locals         # Локальные переменные
(gdb) p variable_name     # Печать переменной
(gdb) x/32xw 0x3FC88000   # Дамп памяти
(gdb) info threads        # Список задач FreeRTOS
(gdb) thread 2            # Переключение на задачу
```

---

## 📊 АНАЛИЗ ДАМПА ПАМЯТИ

После получения дампа можно анализировать:

### 1. Через hexdump (Linux/Git Bash)
```bash
hexdump -C flash_dump.bin | less
```

### 2. Через Python
```python
with open('flash_dump.bin', 'rb') as f:
    data = f.read()
    # Ищем строки
    for i in range(len(data) - 3):
        if data[i:i+4] == b'WiFi':
            print(f"Found at offset: 0x{i:X}")
```

### 3. Через objdump (для .elf)
```bash
xtensa-esp32s3-elf-objdump -S build/hydroponics.elf > disassembly.txt
```

---

## ⚠️ ВАЖНЫЕ ЗАМЕЧАНИЯ

1. **Не читайте во время записи** - остановите чип (`reset halt`)
2. **Flash mapping** - адреса 0x42000000+ это MMU-маппированная flash
3. **Alignment** - некоторые адреса требуют выравнивания (word-aligned)
4. **Cache** - данные в кеше могут отличаться от RAM
5. **RTC память** - сохраняется при deep sleep

---

## 🎯 РЕЗЮМЕ

**У вас есть три способа:**

1. **Быстро и просто:** `python read_esp32_memory.py`
2. **Пакетное чтение:** `read_jtag.bat`
3. **Интерактивная отладка:** `jtag_interactive.bat` + telnet

**Основные команды:**
- `esp chip_info` - инфо о чипе
- `mdw 0xADDRESS 32` - читать память
- `flash read_bank 0 file.bin 0 SIZE` - дамп flash
- `reg` - регистры процессора

**Для отладки:**
- OpenOCD + GDB = полноценная отладка
- Точки останова, шаги, стек вызовов

---

## 📞 НУЖНА ПОМОЩЬ?

Если нужно прочитать что-то конкретное:
1. Запустите `python read_esp32_memory.py`
2. Выберите опцию 9 (произвольный адрес)
3. Или опцию 8 (своя команда OpenOCD)

Или просто скажите что нужно прочитать, и я помогу! 🚀

