@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo   Building Bare-Metal AMP TMR Flight Computer (ARM Cortex-A15)
echo ==============================================================================

:: Check D:\tools\bin or D:\tools\arm-toolchain\bin
if exist "D:\tools\bin\arm-none-eabi-gcc.exe" (
    set "PATH=D:\tools\bin;%PATH%"
    echo [INFO] Using ARM GNU Toolchain from D:\tools\bin
) else if exist "D:\tools\arm-toolchain\bin\arm-none-eabi-gcc.exe" (
    set "PATH=D:\tools\arm-toolchain\bin;%PATH%"
    echo [INFO] Using ARM GNU Toolchain from D:\tools\arm-toolchain\bin
) else (
    where arm-none-eabi-gcc.exe >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] arm-none-eabi-gcc.exe not found in D:\tools\bin or system PATH!
        echo [INFO] Running setup_tools.ps1 to download and configure toolchain...
        powershell.exe -ExecutionPolicy Bypass -File "%~dp0setup_tools.ps1"
        if exist "D:\tools\bin\arm-none-eabi-gcc.exe" (
            set "PATH=D:\tools\bin;%PATH%"
        ) else if exist "D:\tools\arm-toolchain\bin\arm-none-eabi-gcc.exe" (
            set "PATH=D:\tools\arm-toolchain\bin;%PATH%"
        ) else (
            echo [ERROR] Toolchain installation failed. Exiting.
            exit /b 1
        )
    )
)

:: Test if make.exe is available
where make.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [INFO] Invoking make...
    make.exe all
    if %ERRORLEVEL% EQU 0 (
        echo [SUCCESS] Build completed successfully: tmr_flight_computer.elf
        exit /b 0
    ) else (
        echo [ERROR] Make failed.
        exit /b 1
    )
)

:: Fallback: Direct compilation via arm-none-eabi-gcc
echo [INFO] Building directly via arm-none-eabi-gcc...
if not exist "build" mkdir "build"

set "ARCH_FLAGS=-mcpu=cortex-a15 -marm -mfpu=neon-vfpv4 -mfloat-abi=softfp"
set "CFLAGS=%ARCH_FLAGS% -O2 -Wall -Wextra -ffreestanding -nostdlib -Isrc -g"

echo [AS] src\startup.S
arm-none-eabi-gcc %CFLAGS% -c src\startup.S -o build\startup.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\uart.c
arm-none-eabi-gcc %CFLAGS% -c src\uart.c -o build\uart.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\amp.c
arm-none-eabi-gcc %CFLAGS% -c src\amp.c -o build\amp.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\voter.c
arm-none-eabi-gcc %CFLAGS% -c src\voter.c -o build\voter.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\flight_control.c
arm-none-eabi-gcc %CFLAGS% -c src\flight_control.c -o build\flight_control.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\failsafe.c
arm-none-eabi-gcc %CFLAGS% -c src\failsafe.c -o build\failsafe.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\node_health.c
arm-none-eabi-gcc %CFLAGS% -c src\node_health.c -o build\node_health.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\supervision.c
arm-none-eabi-gcc %CFLAGS% -c src\supervision.c -o build\supervision.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\post.c
arm-none-eabi-gcc %CFLAGS% -c src\post.c -o build\post.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\stack_monitor.c
arm-none-eabi-gcc %CFLAGS% -c src\stack_monitor.c -o build\stack_monitor.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [CC] src\main.c
arm-none-eabi-gcc %CFLAGS% -c src\main.c -o build\main.o
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [LD] tmr_flight_computer.elf
arm-none-eabi-gcc %CFLAGS% -T linker.ld -nostdlib -Wl,--build-id=none -Wl,--no-warn-rwx-segments build\startup.o build\uart.o build\amp.o build\voter.o build\flight_control.o build\failsafe.o build\node_health.o build\supervision.o build\post.o build\stack_monitor.o build\main.o -o tmr_flight_computer.elf
if %ERRORLEVEL% NEQ 0 exit /b 1

echo [OBJCOPY] tmr_flight_computer.bin
arm-none-eabi-objcopy -O binary tmr_flight_computer.elf tmr_flight_computer.bin

echo [SIZE] tmr_flight_computer.elf
arm-none-eabi-size tmr_flight_computer.elf

echo [SUCCESS] Build completed successfully: tmr_flight_computer.elf
exit /b 0
