@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo   Launching Bare-Metal AMP TMR Flight Computer in QEMU (vexpress-a15)
echo ==============================================================================

set "QEMU_EXE="

if exist "D:\tools\qemu\qemu-system-arm.exe" (
    set "QEMU_EXE=D:\tools\qemu\qemu-system-arm.exe"
) else if exist "C:\Program Files\qemu\qemu-system-arm.exe" (
    set "QEMU_EXE=C:\Program Files\qemu\qemu-system-arm.exe"
) else (
    where qemu-system-arm.exe >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        set "QEMU_EXE=qemu-system-arm.exe"
    ) else (
        echo [ERROR] qemu-system-arm.exe not found!
        echo [INFO] Running setup_tools.ps1 to install QEMU on D:\tools...
        powershell.exe -ExecutionPolicy Bypass -File "%~dp0setup_tools.ps1"
        if exist "D:\tools\qemu\qemu-system-arm.exe" (
            set "QEMU_EXE=D:\tools\qemu\qemu-system-arm.exe"
        ) else (
            echo [ERROR] QEMU setup failed. Exiting.
            exit /b 1
        )
    )
)

if not exist "tmr_flight_computer.elf" (
    echo [INFO] Binary not found. Triggering build first...
    call "%~dp0build.bat"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Build failed. Cannot launch emulator.
        exit /b 1
    )
)

echo [INFO] Starting QEMU with 4 ARM Cortex-A15 Cores, 128MB RAM, vexpress-a15...
echo [INFO] Press Ctrl+A then X to exit QEMU.
echo ------------------------------------------------------------------------------

"%QEMU_EXE%" -M vexpress-a15 -cpu cortex-a15 -smp 4 -m 128M -nographic -kernel tmr_flight_computer.elf -accel tcg,thread=multi
