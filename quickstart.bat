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

:: Step 3: Terminal Architecture & Voter Breakdown
echo.
echo [3/4] Generating system architecture and voter decision matrix...
where python.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    python.exe "%~dp0visualize_terminal.py"
    python.exe "%~dp0visualize_system.py"
) else (
    echo [INFO] Python not found in PATH; skipping graph plot generation.
)

:: Step 4: Optional Interactive Viewer
echo.
echo [4/4] Review options:
echo   [1] Launch Live Desktop GUI Monitor (Tkinter)
echo   [2] Open HTML Interactive Visual Dashboard (Browser)
echo   [3] Open High-Resolution System Plot (Image)
echo   [4] Exit
echo.
set /p CHOICE="Select an option (1-4, default 4): "

if "%CHOICE%"=="1" (
    where python.exe >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        start python.exe "%~dp0live_monitor.py"
    ) else (
        echo [ERROR] Python is required to run the desktop GUI.
    )
) else if "%CHOICE%"=="2" (
    start "" "%~dp0flight_computer_dashboard.html"
) else if "%CHOICE%"=="3" (
    if exist "%~dp0tmr_system_visualization.png" (
        start "" "%~dp0tmr_system_visualization.png"
    ) else (
        echo [INFO] tmr_system_visualization.png not found.
    )
)

echo.
echo [SUCCESS] Review complete. Thank you for evaluating this project!
echo ==============================================================================
