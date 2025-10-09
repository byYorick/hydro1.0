@echo off
REM Быстрое чтение стека через OpenOCD

echo ========================================
echo  Чтение стека ESP32-S3
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

echo.
echo SP = 0x3FCEB1F0
echo Чтение 128 слов из стека...
echo.

openocd -f board/esp32s3-builtin.cfg ^
    -c "init" ^
    -c "reset halt" ^
    -c "echo ====== Стек вызовов ======" ^
    -c "mdw 0x3FCEB1F0 128" ^
    -c "echo ====== Регистры ======" ^
    -c "reg pc" ^
    -c "reg a0" ^
    -c "reg a1" ^
    -c "exit"

echo.
pause

