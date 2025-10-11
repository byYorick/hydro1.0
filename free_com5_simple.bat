@echo off
REM ========================================================================
REM Simple COM5 Port Release - Kill all Python/IDF processes
REM ========================================================================

echo ============================================
echo    COM5 Port Release (Simple)
echo ============================================
echo.

echo Killing all Python monitor processes...
echo.

REM Kill all python.exe processes (includes idf_monitor)
taskkill /F /IM python.exe >nul 2>&1
if errorlevel 1 (
    echo [INFO] No python.exe processes found
) else (
    echo [OK] Python processes terminated
)

REM Kill all idf.py.exe processes
taskkill /F /IM idf.py.exe >nul 2>&1
if errorlevel 1 (
    echo [INFO] No idf.py.exe processes found
) else (
    echo [OK] IDF.py processes terminated
)

echo.
echo Waiting 2 seconds for port release...
timeout /t 2 /nobreak >nul

echo.
echo ============================================
echo    COM5 should be ready now!
echo ============================================
echo.
echo Press any key to continue...
pause >nul

