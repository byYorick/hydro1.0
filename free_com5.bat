@echo off
chcp 65001 >nul 2>&1
REM ========================================================================
REM Fast COM5 Port Release Tool
REM ========================================================================

echo ============================================
echo    COM5 Port Release Tool
echo ============================================
echo.

echo [1/3] Finding processes using COM5...
echo.

REM Find and kill all processes using COM5
set FOUND=0

for /f "skip=1 tokens=1" %%p in ('wmic process where "commandline like '%%%%COM5%%%%' and (name='python.exe' or name='idf.py.exe')" get processid 2^>nul') do (
    if not "%%p"=="" (
        set FOUND=1
        echo Killing PID: %%p
        taskkill /F /PID %%p >nul 2>&1
        if errorlevel 1 (
            echo   [FAIL] Could not kill PID %%p
        ) else (
            echo   [OK] PID %%p terminated
        )
    )
)

if %FOUND%==0 (
    echo [OK] No processes found using COM5
    echo.
    echo COM5 is ready!
    echo.
    timeout /t 2 /nobreak >nul
    exit /b 0
)

echo.
echo [2/3] Waiting for port release...
timeout /t 2 /nobreak >nul

echo.
echo [3/3] Verifying...
set REMAINING=0
for /f "skip=1 tokens=1" %%p in ('wmic process where "commandline like '%%%%COM5%%%%' and (name='python.exe' or name='idf.py.exe')" get processid 2^>nul') do (
    if not "%%p"=="" (
        set REMAINING=1
    )
)

if %REMAINING%==1 (
    echo [WARNING] Some processes may still be running
    echo Try running this script again or reboot
) else (
    echo [OK] All processes terminated
)

echo.
echo ============================================
echo    COM5 is ready to use!
echo ============================================
echo.
timeout /t 2 /nobreak >nul
exit /b 0
