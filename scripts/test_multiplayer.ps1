# scripts/test_multiplayer.ps1
# Multi-Client Verification Helper Script for Tempest Multiplayer Bridge
param(
    [string]$Toolchain = "windows-clang",
    [int]$Port = 7777,
    [int]$ClientCount = 2
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$BinDir = Join-Path $RootDir "bin\Debug\$Toolchain"
$ServerExe = Join-Path $BinDir "tempest-server.exe"
$RunnerExe = Join-Path $BinDir "runner.exe"

if (!(Test-Path $ServerExe)) {
    Write-Error "Server executable not found at: $ServerExe. Please build the project first."
}

if (!(Test-Path $RunnerExe)) {
    Write-Error "Runner executable not found at: $RunnerExe. Please build the project first."
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Tempest Multiplayer Verification Suite" -ForegroundColor Cyan
Write-Host " Toolchain:    $Toolchain" -ForegroundColor Gray
Write-Host " Server:       $ServerExe" -ForegroundColor Gray
Write-Host " Runner:       $RunnerExe" -ForegroundColor Gray
Write-Host " Connect Target: 127.0.0.1:$Port" -ForegroundColor Gray
Write-Host " Client Count: $ClientCount" -ForegroundColor Gray
Write-Host "==========================================================" -ForegroundColor Cyan

# 0. Pre-clean any orphaned server or runner processes
Get-Process -Name "tempest-server", "runner" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Terminating orphaned process $($_.ProcessName) (PID: $($_.Id))..." -ForegroundColor Yellow
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Milliseconds 200

$SpawnedProcesses = @()

try {
    # 1. Start dedicated server
    Write-Host "[1/3] Launching dedicated server on port $Port..." -ForegroundColor Green
    $ServerProc = Start-Process -FilePath $ServerExe -ArgumentList "--port=$Port" -WorkingDirectory $RootDir -PassThru
    $SpawnedProcesses += $ServerProc

    # Allow server socket to bind
    Start-Sleep -Milliseconds 600

    if ($ServerProc.HasExited) {
        throw "Server exited unexpectedly with exit code: $($ServerProc.ExitCode)"
    }

    # 2. Start graphical clients
    for ($i = 1; $i -le $ClientCount; $i++) {
        Write-Host "[2/3] Launching graphical client #$i (connecting to 127.0.0.1:$Port)..." -ForegroundColor Green
        $ClientProc = Start-Process -FilePath $RunnerExe -ArgumentList "--connect=127.0.0.1:$Port" -WorkingDirectory $RootDir -PassThru
        $SpawnedProcesses += $ClientProc
        Start-Sleep -Milliseconds 400
    }

    Write-Host "==========================================================" -ForegroundColor Yellow
    Write-Host " All instances running!" -ForegroundColor Yellow
    Write-Host " Controls:" -ForegroundColor White
    Write-Host "   - WASD: Move (relative to camera yaw)" -ForegroundColor Gray
    Write-Host "   - Mouse: Look around (pitch & yaw)" -ForegroundColor Gray
    Write-Host "   - Space: Jump" -ForegroundColor Gray
    Write-Host "   - Shift: Sprint" -ForegroundColor Gray
    Write-Host "   - Esc: Exit client" -ForegroundColor Gray
    Write-Host "" -ForegroundColor White
    Write-Host " Visuals:" -ForegroundColor White
    Write-Host "   - Local player: Orange accent" -ForegroundColor Gray
    Write-Host "   - Remote player: Cyan accent" -ForegroundColor Gray
    Write-Host "   - Floor: 40m x 40m sandbox plane" -ForegroundColor Gray
    Write-Host "   - Step Obstacle: 2m x 0.2m x 2m stair block at (0, 0.1, 5.0)" -ForegroundColor Gray
    Write-Host "==========================================================" -ForegroundColor Yellow
    Write-Host "Press [Enter] or Ctrl+C to terminate all sessions and exit..." -ForegroundColor Cyan

    [void][System.Console]::ReadLine()
}
finally {
    Write-Host "[3/3] Terminating spawned processes..." -ForegroundColor DarkYellow
    foreach ($proc in $SpawnedProcesses) {
        if ($null -ne $proc -and !$proc.HasExited) {
            try {
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            } catch {
                # Ignore process termination errors
            }
        }
    }
    Write-Host "Done." -ForegroundColor Green
}
