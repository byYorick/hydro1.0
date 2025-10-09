@echo off
REM Автоматическое получение backtrace через GDB

echo ========================================
echo  GDB Backtrace для ESP32-S3
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

echo.
echo Запуск OpenOCD в фоне...
start /B openocd -f board/esp32s3-builtin.cfg

echo Ожидание 3 секунды...
timeout /t 3 /nobreak > nul

echo.
echo Подключение через GDB...
echo.

REM Создаем файл с командами GDB
echo target remote localhost:3333 > gdb_commands.tmp
echo info threads >> gdb_commands.tmp
echo bt >> gdb_commands.tmp
echo info reg >> gdb_commands.tmp
echo quit >> gdb_commands.tmp

REM Запускаем GDB с командами
xtensa-esp32s3-elf-gdb -batch -x gdb_commands.tmp build/hydroponics.elf

echo.
echo ========================================
echo  Готово!
echo ========================================
echo.

REM Убираем временный файл
if exist gdb_commands.tmp del gdb_commands.tmp

REM Останавливаем OpenOCD
taskkill /F /IM openocd.exe > nul 2>&1

pause

