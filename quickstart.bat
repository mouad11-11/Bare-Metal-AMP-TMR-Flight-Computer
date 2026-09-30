@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo   Bare-Metal AMP TMR Flight Computer - Quick Start Review Runner
echo ==============================================================================
echo.

:: Step 1: Toolchain Validation
echo [1/4] Checking toolchain prerequisites...
call "%~dp0build.bat"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed! Please ensure Arm GNU Toolchain is installed.
    pause
    exit /b 1
)

:: Step 2: Automated Multi-Core QEMU Emulation
echo.
echo [2/4] Executing 4-core bare-metal emulation in QEMU vexpress-a15...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0test_flight.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] QEMU simulation encountered an error.
    pause
    exit /b 1
)

:: Step 3: Interactive Telemetry Monitor
echo.
echo [3/3] Telemetry ^& Monitoring:
echo   [1] Launch Live Desktop GUI Monitor (Tkinter)
echo   [2] Exit
echo.
set /p CHOICE="Select an option (1-2, default 1): "
if "%CHOICE%"=="" set CHOICE=1

if "%CHOICE%"=="1" (
    where python.exe >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo [INFO] Launching live desktop monitor...
        start python.exe "%~dp0live_monitor.py"
    ) else (
        echo [ERROR] Python is required to run the desktop GUI.
    )
)

echo.
echo [SUCCESS] Review complete. Thank you for evaluating this project!
echo ==============================================================================
