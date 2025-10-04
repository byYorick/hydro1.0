@echo off
echo Поиск процессов, использующих COM3 порт...

REM Получаем список процессов, использующих COM3
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :COM3') do (
    echo Найден процесс с PID: %%a
    echo Завершение процесса...
    taskkill /PID %%a /F
    if errorlevel 1 (
        echo Ошибка при завершении процесса %%a
    ) else (
        echo Процесс %%a успешно завершен
    )
)

REM Дополнительная проверка через wmic
echo.
echo Дополнительная проверка через wmic...
for /f "tokens=2" %%a in ('wmic process where "CommandLine like '%%COM3%%'" get ProcessId /value ^| findstr ProcessId') do (
    echo Найден процесс с PID: %%a
    taskkill /PID %%a /F
    if errorlevel 1 (
        echo Ошибка при завершении процесса %%a
    ) else (
        echo Процесс %%a успешно завершен
    )
)

REM Проверяем, свободен ли порт
echo.
echo Проверка статуса COM3...
netstat -ano | findstr :COM3
if errorlevel 1 (
    echo COM3 порт свободен!
) else (
    echo COM3 порт все еще занят
)

pause
