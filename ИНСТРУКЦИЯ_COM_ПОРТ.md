# Автоматическое освобождение COM порта

## Проблема
При прошивке часто появляется ошибка:
```
A fatal error occurred: Could not open COM10, the port is busy or doesn't exist.
```

## Решение

### 1. Автоматический скрипт `free_com_port.py`

**Использование:**
```cmd
python free_com_port.py COM10
```

**Что делает:**
- Находит все процессы, использующие COM10
- Завершает их автоматически (без запросов!)
- Проверяет, освободился ли порт
- Возвращает код 0 при успехе

### 2. VS Code задачи

Добавлены новые задачи в `.vscode/tasks.json`:

#### **Free COM Port** 
Освобождает COM10
```
Ctrl+Shift+P → Tasks: Run Task → Free COM Port
```

#### **Free COM and Flash** 
Освобождает порт и прошивает
```
Ctrl+Shift+P → Tasks: Run Task → Free COM and Flash
```

#### **Free COM and Flash Monitor** (рекомендуется!)
Освобождает порт, прошивает и запускает монитор
```
Ctrl+Shift+P → Tasks: Run Task → Free COM and Flash Monitor
```

### 3. Ручной запуск

**Через командную строку:**
```cmd
# Освободить порт
  ?

# Прошить
idf.py -p COM10 flash monitor
```

**Одной командой:**
```cmd
python free_com_port.py COM10 && idf.py -p COM10 flash monitor
```

## Настройка для другого COM порта

Если у вас **не COM10**, замените в:

1. **free_com_port.py** - параметр при вызове
2. **.vscode/tasks.json** - строки с `COM10`

**Пример для COM3:**
```json
"args": [
    "${workspaceFolder}/free_com_port.py",
    "COM3"  // ← здесь
]
```

## Требования

- Python 3.x
- Модуль `psutil` (уже установлен в ESP-IDF)

## Что завершает скрипт

Автоматически находит и завершает:
- `python.exe` с `monitor`, `esptool`
- `idf.py monitor`
- PuTTY, TeraTerm
- Arduino IDE
- Любые процессы с указанным COM портом в командной строке

**Безопасно:** Завершает только связанные с портом процессы

## Быстрый старт

**Прошивка с автоматическим освобождением порта:**
```cmd
python free_com_port.py COM10 && idf.py -p COM10 flash monitor
```

Или используйте VS Code задачу:
```
Ctrl+Shift+P → Free COM and Flash Monitor
```

✅ **Готово!** Больше не нужно вручную закрывать программы.

