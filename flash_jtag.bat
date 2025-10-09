@echo off
REM Скрипт для прошивки ESP32-S3 через встроенный USB-JTAG
REM Использует OpenOCD и не требует COM порта

echo ========================================
echo  Прошивка ESP32-S3 через USB-JTAG
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

REM Проверяем наличие build
if not exist "build\hydroponics.bin" (
    echo Ошибка: build\hydroponics.bin не найден!
    echo Сначала соберите проект: idf.py build
    pause
    exit /b 1
)

echo.
echo Запуск OpenOCD для ESP32-S3 USB-JTAG...
echo.

REM Прошиваем через OpenOCD
openocd -f board/esp32s3-builtin.cfg -c "program_esp build/bootloader/bootloader.bin 0x0 verify" -c "program_esp build/partition_table/partition-table.bin 0x8000 verify" -c "program_esp build/hydroponics.bin 0x10000 verify reset exit"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo  Прошивка УСПЕШНА!
    echo ========================================
    echo.
    echo Устройство перезагружено и готово к работе.
) else (
    echo.
    echo ========================================
    echo  ОШИБКА при прошивке!
    echo ========================================
    echo.
    echo Проверьте:
    echo 1. Подключен ли ESP32-S3 через USB
    echo 2. Установлен ли драйвер USB-JTAG
    echo 3. Нет ли других программ, использующих устройство
)

echo.
pause

