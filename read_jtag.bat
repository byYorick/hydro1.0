@echo off
REM Скрипт для чтения ESP32-S3 через USB-JTAG
REM Показывает информацию о состоянии чипа и памяти

echo ========================================
echo  Чтение ESP32-S3 через USB-JTAG
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

echo.
echo Подключение к ESP32-S3 через OpenOCD...
echo.

REM Создаем временный файл с командами OpenOCD
echo target remote localhost:3333 > jtag_commands.tmp
echo info reg >> jtag_commands.tmp
echo x/32xw 0x3FC88000 >> jtag_commands.tmp
echo monitor reset halt >> jtag_commands.tmp
echo continue >> jtag_commands.tmp
echo quit >> jtag_commands.tmp

REM Запускаем OpenOCD в фоне
start /B openocd -f board/esp32s3-builtin.cfg

REM Ждем 3 секунды для инициализации OpenOCD
timeout /t 3 /nobreak > nul

echo.
echo Чтение информации о чипе...
echo.

REM Подключаемся через telnet к OpenOCD (порт 4444)
echo Выполнение команд через OpenOCD:
echo.

REM Используем OpenOCD напрямую для чтения
openocd -f board/esp32s3-builtin.cfg ^
    -c "init" ^
    -c "reset halt" ^
    -c "echo ====== Информация о чипе ======" ^
    -c "esp chip_info" ^
    -c "echo ====== Чтение RAM (0x3FC88000) ======" ^
    -c "mdw 0x3FC88000 32" ^
    -c "echo ====== Чтение Flash (0x0) ======" ^
    -c "flash read_bank 0 flash_dump.bin 0 0x1000" ^
    -c "echo Flash bootloader сохранен в flash_dump.bin" ^
    -c "reset run" ^
    -c "exit"

if exist "jtag_commands.tmp" del jtag_commands.tmp

echo.
echo ========================================
echo  Чтение завершено!
echo ========================================
echo.
pause

