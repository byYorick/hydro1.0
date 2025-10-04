# Скрипт для завершения процессов, использующих COM3 порт
Write-Host "Поиск процессов, использующих COM3 порт..." -ForegroundColor Green

# Функция для завершения процесса по PID
function Stop-ProcessByPID {
    param([int]$PID)
    try {
        $process = Get-Process -Id $PID -ErrorAction Stop
        Write-Host "Завершение процесса: $($process.ProcessName) (PID: $PID)" -ForegroundColor Yellow
        Stop-Process -Id $PID -Force
        Write-Host "Процесс $PID успешно завершен" -ForegroundColor Green
    }
    catch {
        Write-Host "Ошибка при завершении процесса $PID : $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Поиск процессов через netstat
Write-Host "`nПоиск через netstat..." -ForegroundColor Cyan
$netstatOutput = netstat -ano | Select-String ":COM3"
if ($netstatOutput) {
    foreach ($line in $netstatOutput) {
        $parts = $line.ToString().Split() | Where-Object { $_ -ne "" }
        if ($parts.Length -ge 5) {
            $pid = $parts[-1]
            if ($pid -match '^\d+$') {
                Stop-ProcessByPID -PID [int]$pid
            }
        }
    }
} else {
    Write-Host "Процессы, использующие COM3, не найдены через netstat" -ForegroundColor Yellow
}

# Поиск процессов через Get-NetTCPConnection (PowerShell 5.1+)
Write-Host "`nПоиск через Get-NetTCPConnection..." -ForegroundColor Cyan
try {
    $connections = Get-NetTCPConnection | Where-Object { $_.LocalAddress -like "*COM3*" -or $_.LocalAddress -like "*COM3*" }
    foreach ($conn in $connections) {
        if ($conn.OwningProcess) {
            Stop-ProcessByPID -PID $conn.OwningProcess
        }
    }
} catch {
    Write-Host "Get-NetTCPConnection недоступен в этой версии PowerShell" -ForegroundColor Yellow
}

# Поиск процессов, которые могут использовать COM порты
Write-Host "`nПоиск процессов, связанных с COM портами..." -ForegroundColor Cyan
$comProcesses = Get-Process | Where-Object { 
    $_.ProcessName -like "*serial*" -or 
    $_.ProcessName -like "*com*" -or 
    $_.ProcessName -like "*esp*" -or
    $_.ProcessName -like "*idf*" -or
    $_.ProcessName -like "*monitor*" -or
    $_.ProcessName -like "*putty*" -or
    $_.ProcessName -like "*teraterm*"
}

foreach ($proc in $comProcesses) {
    Write-Host "Найден потенциально связанный процесс: $($proc.ProcessName) (PID: $($proc.Id))" -ForegroundColor Yellow
    $response = Read-Host "Завершить этот процесс? (y/n)"
    if ($response -eq "y" -or $response -eq "Y") {
        Stop-ProcessByPID -PID $proc.Id
    }
}

# Финальная проверка
Write-Host "`nФинальная проверка COM3..." -ForegroundColor Cyan
$finalCheck = netstat -ano | Select-String ":COM3"
if ($finalCheck) {
    Write-Host "COM3 все еще занят:" -ForegroundColor Red
    $finalCheck | ForEach-Object { Write-Host $_.ToString() -ForegroundColor Red }
} else {
    Write-Host "COM3 порт свободен!" -ForegroundColor Green
}

Write-Host "`nСкрипт завершен. Нажмите любую клавишу для выхода..." -ForegroundColor Green
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
