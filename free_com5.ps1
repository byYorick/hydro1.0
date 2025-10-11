# ========================================================================
# Script to free COM5 port
# Usage: .\free_com5.ps1 [-Force]
# ========================================================================

param(
    [switch]$Force
)

$ErrorActionPreference = "SilentlyContinue"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   COM5 Port Release Tool" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Find processes using COM5
Write-Host "[1/3] Searching for processes using COM5..." -ForegroundColor Yellow

$processes = Get-WmiObject Win32_Process | Where-Object {
    $_.CommandLine -like "*COM5*" -and 
    ($_.Name -eq "python.exe" -or 
     $_.Name -eq "idf.py.exe" -or 
     $_.Name -eq "idf_monitor.exe" -or
     $_.CommandLine -like "*idf_monitor*" -or
     $_.CommandLine -like "*idf.py*")
}

if ($processes.Count -eq 0) {
    Write-Host "OK: No processes using COM5 found" -ForegroundColor Green
    Write-Host ""
    exit 0
}

# Step 2: Display found processes
Write-Host "Found processes: $($processes.Count)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Process list:" -ForegroundColor White

$processIds = @()
foreach ($proc in $processes) {
    $processIds += $proc.ProcessId
    
    $cmdLine = $proc.CommandLine
    if ($cmdLine.Length -gt 80) {
        $cmdLine = $cmdLine.Substring(0, 77) + "..."
    }
    
    Write-Host "  PID: $($proc.ProcessId) | $($proc.Name)" -ForegroundColor Cyan
    Write-Host "    Command: $cmdLine" -ForegroundColor Gray
    Write-Host ""
}

# Step 3: Confirmation
if (-not $Force) {
    Write-Host ""
    Write-Host "[2/3] Kill all these processes?" -ForegroundColor Yellow
    $confirmation = Read-Host "Enter 'y' to confirm, any other key to cancel"
    
    if ($confirmation -ne 'y' -and $confirmation -ne 'Y') {
        Write-Host "CANCELLED by user" -ForegroundColor Red
        Write-Host ""
        exit 1
    }
} else {
    Write-Host "[2/3] Force mode: killing without confirmation..." -ForegroundColor Yellow
}

# Step 4: Kill processes using taskkill
Write-Host ""
Write-Host "[3/3] Terminating processes..." -ForegroundColor Yellow

# Build PID list for taskkill
$pidArgs = ""
foreach ($pid in $processIds) {
    $pidArgs += "/PID $pid "
}

# Execute taskkill
$taskkillCmd = "taskkill /F $pidArgs"
Write-Host "Executing: $taskkillCmd" -ForegroundColor Gray
$result = & cmd /c $taskkillCmd 2>&1

# Parse result
$successCount = 0
$failCount = 0
foreach ($line in $result) {
    if ($line -match "SUCCESS" -or $line -match "�ᯥ譮") {
        $successCount++
        Write-Host "  OK: $line" -ForegroundColor Green
    } elseif ($line -match "ERROR" -or $line -match "�訡��") {
        $failCount++
        Write-Host "  FAIL: $line" -ForegroundColor Red
    }
}

# Summary
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   Results:" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Successfully terminated: $successCount" -ForegroundColor Green
if ($failCount -gt 0) {
    Write-Host "  Failed: $failCount" -ForegroundColor Red
}
Write-Host ""

Write-Host "Waiting 1 second for port release..." -ForegroundColor Gray
Start-Sleep -Seconds 1

Write-Host "OK: COM5 is ready to use!" -ForegroundColor Green
Write-Host ""

exit 0
