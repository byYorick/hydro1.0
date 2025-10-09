@echo off
REM Интерактивное подключение к ESP32-S3 через JTAG
REM Запускает OpenOCD и показывает как подключиться

echo ========================================
echo  Интерактивный JTAG для ESP32-S3
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

echo.
echo Запуск OpenOCD сервера...
echo.
echo После запуска вы можете:
echo.
echo 1. Подключиться через Telnet (порт 4444):
echo    telnet localhost 4444
echo.
echo 2. Или через GDB (порт 3333):
echo    xtensa-esp32s3-elf-gdb build/hydroponics.elf
echo    (gdb) target remote localhost:3333
echo.
echo Полезные команды OpenOCD (через telnet):
echo   - reset halt           # Остановить чип
echo   - reset run            # Запустить чип
echo   - reg                  # Показать регистры
echo   - mdw 0xАДРЕС 32       # Прочитать 32 слова из памяти
echo   - mdb 0xАДРЕС 256      # Прочитать 256 байт из памяти
echo   - flash read_bank 0 file.bin 0 SIZE  # Дамп flash
echo   - esp chip_info        # Информация о чипе
echo   - halt                 # Остановить выполнение
echo   - step                 # Шаг выполнения
echo.
echo Нажмите Ctrl+C для остановки OpenOCD
echo ========================================
echo.

openocd -f board/esp32s3-builtin.cfg

