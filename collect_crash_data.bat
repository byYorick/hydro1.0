@echo off
REM Автоматический сбор данных для анализа багов через JTAG
REM Создает полный отчет в файле crash_report.txt

echo ========================================
echo  Сбор данных для анализа бага
echo ========================================
echo.

REM Инициализируем ESP-IDF окружение
call C:\Espressif\idf_cmd_init.bat esp-idf-29323a3f5a0574597d6dbaa0af20c775

REM Имя файла отчета с датой и временем
set REPORT_FILE=crash_report_%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt
set REPORT_FILE=%REPORT_FILE: =0%

echo Отчет будет сохранен в: %REPORT_FILE%
echo.

REM Создаем заголовок отчета
echo ============================================================ > %REPORT_FILE%
echo   ESP32-S3 CRASH REPORT >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo Дата: %date% %time% >> %REPORT_FILE%
echo Проект: Hydroponics System >> %REPORT_FILE%
echo Устройство: ESP32-S3 >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo. >> %REPORT_FILE%

echo [1/7] Запуск OpenOCD...
start /B openocd -f board/esp32s3-builtin.cfg > openocd.log 2>&1
timeout /t 3 /nobreak > nul

echo [2/7] Сбор регистров процессора...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   1. REGISTERS DUMP >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
xtensa-esp32s3-elf-gdb -batch -ex "target remote localhost:3333" -ex "info reg" build/hydroponics.elf >> %REPORT_FILE% 2>&1

echo [3/7] Получение backtrace...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   2. BACKTRACE (CALL STACK) >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
xtensa-esp32s3-elf-gdb -batch -ex "target remote localhost:3333" -ex "bt" build/hydroponics.elf >> %REPORT_FILE% 2>&1

echo [4/7] Получение списка задач FreeRTOS...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   3. FREERTOS TASKS >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
xtensa-esp32s3-elf-gdb -batch -ex "target remote localhost:3333" -ex "info threads" build/hydroponics.elf >> %REPORT_FILE% 2>&1

echo [5/7] Чтение стека...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   4. STACK MEMORY DUMP >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
xtensa-esp32s3-elf-gdb -batch -ex "target remote localhost:3333" -ex "x/128xw $sp" build/hydroponics.elf >> %REPORT_FILE% 2>&1

echo [6/7] Дамп критических областей памяти...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   5. MEMORY REGIONS >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo [DRAM Region 0x3FC88000] >> %REPORT_FILE%
xtensa-esp32s3-elf-gdb -batch -ex "target remote localhost:3333" -ex "x/64xw 0x3FC88000" build/hydroponics.elf >> %REPORT_FILE% 2>&1

echo [7/7] Получение информации о чипе...
echo. >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
echo   6. CHIP INFO >> %REPORT_FILE%
echo ============================================================ >> %REPORT_FILE%
openocd -f board/esp32s3-builtin.cfg -c "init" -c "esp chip_info" -c "exit" >> %REPORT_FILE% 2>&1

REM Останавливаем OpenOCD
taskkill /F /IM openocd.exe > nul 2>&1

echo.
echo ============================================================
echo   Сбор данных завершен!
echo ============================================================
echo.
echo Файл отчета: %REPORT_FILE%
echo.
echo Теперь вы можете:
echo 1. Открыть файл и прочитать
echo 2. Скопировать содержимое и отправить в чат
echo 3. Прикрепить файл к отчету
echo.
echo Для автоматического анализа запустите:
echo    python analyze_crash.py %REPORT_FILE%
echo.
pause

