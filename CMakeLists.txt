cmake_minimum_required(VERSION 3.15)
project(tmr_flight_computer C ASM)

set(CMAKE_C_STANDARD 99)

set(ARCH_FLAGS "-mcpu=cortex-a15 -marm -mfpu=neon-vfpv4 -mfloat-abi=softfp")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAGS} -O2 -Wall -Wextra -ffreestanding -nostdlib -g")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${ARCH_FLAGS} -g")

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set(CMAKE_EXE_LINKER_FLAGS "-T \"${LINKER_SCRIPT}\" -nostdlib -Wl,--build-id=none")

include_directories(src)

set(SRCS
    src/startup.S
    src/main.c
    src/uart.c
    src/amp.c
    src/voter.c
    src/flight_control.c
)

add_executable(tmr_flight_computer.elf ${SRCS})

# Generate raw binary and print size
add_custom_command(TARGET tmr_flight_computer.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary tmr_flight_computer.elf tmr_flight_computer.bin
    COMMAND ${CMAKE_SIZE} tmr_flight_computer.elf
    COMMENT "Building raw binary and printing section sizes"
)
