# ==============================================================================
# Bare-Metal AMP TMR Flight Computer Makefile
# Target: ARM Cortex-A15 (vexpress-a15)
# ==============================================================================

# Search for ARM toolchain: D:\tools\bin, D:\tools\arm-toolchain\bin, or system PATH
ifneq ($(wildcard D:/tools/bin/arm-none-eabi-gcc.exe),)
    CROSS_COMPILE ?= D:/tools/bin/arm-none-eabi-
else ifneq ($(wildcard D:/tools/arm-toolchain/bin/arm-none-eabi-gcc.exe),)
    CROSS_COMPILE ?= D:/tools/arm-toolchain/bin/arm-none-eabi-
else
    CROSS_COMPILE ?= arm-none-eabi-
endif

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size

TARGET_ELF := tmr_flight_computer.elf
TARGET_BIN := tmr_flight_computer.bin

SRC_DIR := src
BUILD_DIR := build

C_SRCS  := $(wildcard $(SRC_DIR)/*.c)
S_SRCS  := $(wildcard $(SRC_DIR)/*.S)
C_OBJS  := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
S_OBJS  := $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(S_SRCS))
OBJS    := $(S_OBJS) $(C_OBJS)

ARCH_FLAGS := -mcpu=cortex-a15 -marm -mfpu=neon-vfpv4 -mfloat-abi=softfp
CFLAGS     := $(ARCH_FLAGS) -O2 -Wall -Wextra -ffreestanding -nostdlib -I$(SRC_DIR) -g
ASFLAGS    := $(ARCH_FLAGS) -I$(SRC_DIR) -g
LDFLAGS    := -T linker.ld -nostdlib -Wl,--build-id=none -Wl,--no-warn-rwx-segments

# Host Unit Testing & Coverage
ifneq ($(wildcard D:/tools/w64devkit/bin/gcc.exe),)
    HOST_CC   ?= D:/tools/w64devkit/bin/gcc.exe
    HOST_GCOV ?= D:/tools/w64devkit/bin/gcov.exe
else
    HOST_CC   ?= gcc
    HOST_GCOV ?= gcov
endif

TEST_HOST_BIN  := tests/host/test_voter.exe
TEST_FS_BIN    := tests/host/test_failsafe.exe
TEST_STACK_BIN := tests/host/test_stack_monitor.exe
TEST_SUP_BIN   := tests/host/test_supervision.exe
TEST_POST_BIN  := tests/host/test_post.exe
TEST_MATH_BIN  := tests/host/test_safe_math.exe
TEST_MMU_BIN   := tests/host/test_mmu.exe
TEST_LOCK_BIN  := tests/host/test_lockstep.exe
TEST_MBOX_BIN  := tests/host/test_mailbox.exe
TEST_PMU_BIN   := tests/host/test_pmu.exe
TEST_DIV_BIN   := tests/host/test_diversity.exe

.PHONY: all clean dump run test-host test-fi

ifeq ($(OS),Windows_NT)
    MKDIR_BUILD    := @if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    CLEAN_COVERAGE := @if exist "tests\host\*.gcda" del /f /q "tests\host\*.gcda" & if exist "*.gcov" del /f /q "*.gcov"
    define CLEAN_ALL
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
	@if exist "$(TARGET_ELF)" del /f /q "$(TARGET_ELF)"
	@if exist "$(TARGET_BIN)" del /f /q "$(TARGET_BIN)"
	@if exist "tmr_flight_computer.asm" del /f /q "tmr_flight_computer.asm"
	@if exist "tests\host\*.exe" del /f /q "tests\host\*.exe"
	@if exist "tests\host\*.gc*" del /f /q "tests\host\*.gc*"
	@if exist "*.gcov" del /f /q "*.gcov"
    endef
else
    MKDIR_BUILD    := mkdir -p $(BUILD_DIR)
    CLEAN_COVERAGE := rm -f tests/host/*.gcda *.gcov
    define CLEAN_ALL
	rm -rf $(BUILD_DIR) $(TARGET_ELF) $(TARGET_BIN) tmr_flight_computer.asm tests/host/*.exe tests/host/test_* tests/host/*.gc* *.gcov
    endef
endif

all: $(TARGET_ELF) $(TARGET_BIN)

$(BUILD_DIR):
	$(MKDIR_BUILD)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@echo [AS] $<
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo [CC] $<
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(OBJS) linker.ld
	@echo [LD] $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@
	@echo [SIZE] $@
	$(SIZE) $@

$(TARGET_BIN): $(TARGET_ELF)
	@echo [OBJCOPY] $@
	$(OBJCOPY) -O binary $< $@

dump: $(TARGET_ELF)
	$(OBJDUMP) -d $< > tmr_flight_computer.asm

test-host:
	$(CLEAN_COVERAGE)
	@echo [HOST-CC] tests/host/test_voter.c + src/voter.c + src/node_health.c + src/failsafe.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/voter.c $(SRC_DIR)/node_health.c $(SRC_DIR)/failsafe.c tests/host/test_voter.c -o $(TEST_HOST_BIN)
	@echo [HOST-RUN] $(TEST_HOST_BIN)
	$(TEST_HOST_BIN)
	@echo [HOST-COVERAGE] voter.c
	$(HOST_GCOV) -b -c tests/host/test_voter-voter.gcno
	@echo [HOST-COVERAGE] node_health.c
	$(HOST_GCOV) -b -c tests/host/test_voter-node_health.gcno
	@echo [HOST-CC] tests/host/test_failsafe.c + src/failsafe.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/failsafe.c tests/host/test_failsafe.c -o $(TEST_FS_BIN)
	@echo [HOST-RUN] $(TEST_FS_BIN)
	$(TEST_FS_BIN)
	@echo [HOST-COVERAGE] failsafe.c
	$(HOST_GCOV) -b -c tests/host/test_failsafe-failsafe.gcno
	@echo [HOST-CC] tests/host/test_stack_monitor.c + src/stack_monitor.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/stack_monitor.c tests/host/test_stack_monitor.c -o $(TEST_STACK_BIN)
	@echo [HOST-RUN] $(TEST_STACK_BIN)
	$(TEST_STACK_BIN)
	@echo [HOST-COVERAGE] stack_monitor.c
	$(HOST_GCOV) -b -c tests/host/test_stack_monitor-stack_monitor.gcno
	@echo [HOST-CC] tests/host/test_supervision.c + src/supervision.c + src/node_health.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/supervision.c $(SRC_DIR)/node_health.c tests/host/test_supervision.c -o $(TEST_SUP_BIN)
	@echo [HOST-RUN] $(TEST_SUP_BIN)
	$(TEST_SUP_BIN)
	@echo [HOST-COVERAGE] supervision.c
	$(HOST_GCOV) -b -c tests/host/test_supervision-supervision.gcno
	@echo [HOST-CC] tests/host/test_post.c + src/post.c + src/voter.c + src/node_health.c + src/failsafe.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/post.c $(SRC_DIR)/voter.c $(SRC_DIR)/node_health.c $(SRC_DIR)/failsafe.c tests/host/test_post.c -o $(TEST_POST_BIN)
	@echo [HOST-RUN] $(TEST_POST_BIN)
	$(TEST_POST_BIN)
	@echo [HOST-COVERAGE] post.c
	$(HOST_GCOV) -b -c tests/host/test_post-post.gcno
	@echo [HOST-CC] tests/host/test_safe_math.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage tests/host/test_safe_math.c -o $(TEST_MATH_BIN)
	@echo [HOST-RUN] $(TEST_MATH_BIN)
	$(TEST_MATH_BIN)
	@echo [HOST-COVERAGE] safe_math.h
	$(HOST_GCOV) -b -c tests/host/test_safe_math.gcno
	@echo [HOST-CC] tests/host/test_mmu.c + src/mmu.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/mmu.c tests/host/test_mmu.c -o $(TEST_MMU_BIN)
	@echo [HOST-RUN] $(TEST_MMU_BIN)
	$(TEST_MMU_BIN)
	@echo [HOST-COVERAGE] mmu.c
	$(HOST_GCOV) -b -c tests/host/test_mmu-mmu.gcno
	@echo [HOST-CC] tests/host/test_lockstep.c + src/lockstep.c + src/voter.c + src/node_health.c + src/failsafe.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/lockstep.c $(SRC_DIR)/voter.c $(SRC_DIR)/node_health.c $(SRC_DIR)/failsafe.c tests/host/test_lockstep.c -o $(TEST_LOCK_BIN)
	@echo [HOST-RUN] $(TEST_LOCK_BIN)
	$(TEST_LOCK_BIN)
	@echo [HOST-COVERAGE] lockstep.c
	$(HOST_GCOV) -b -c tests/host/test_lockstep-lockstep.gcno
	@echo [HOST-CC] tests/host/test_mailbox.c + src/mailbox.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/mailbox.c tests/host/test_mailbox.c -o $(TEST_MBOX_BIN)
	@echo [HOST-RUN] $(TEST_MBOX_BIN)
	$(TEST_MBOX_BIN)
	@echo [HOST-COVERAGE] mailbox.c
	$(HOST_GCOV) -b -c tests/host/test_mailbox-mailbox.gcno
	@echo [HOST-CC] tests/host/test_pmu.c + src/pmu.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/pmu.c tests/host/test_pmu.c -o $(TEST_PMU_BIN)
	@echo [HOST-RUN] $(TEST_PMU_BIN)
	$(TEST_PMU_BIN)
	@echo [HOST-COVERAGE] pmu.c
	$(HOST_GCOV) -b -c tests/host/test_pmu-pmu.gcno
	@echo [HOST-CC] tests/host/test_diversity.c + src/flight_control.c
	$(HOST_CC) -Wall -Wextra -Werror -I$(SRC_DIR) --coverage $(SRC_DIR)/flight_control.c tests/host/test_diversity.c -o $(TEST_DIV_BIN)
	@echo [HOST-RUN] $(TEST_DIV_BIN)
	$(TEST_DIV_BIN)
	@echo [HOST-COVERAGE] flight_control.c
	$(HOST_GCOV) -b -c tests/host/test_diversity-flight_control.gcno

test-fi: $(TARGET_ELF)
	@echo [FI-CAMPAIGN] Executing automated fault-injection campaign...
	python tests/fi/run_fault_campaign.py

clean:
	$(CLEAN_ALL)
	@echo Cleaned build artifacts.

