# Bare-Metal AMP TMR Flight Computer

[![Target: ARM Cortex-A15](https://img.shields.io/badge/Target-ARM%20Cortex--A15-blue.svg)](https://developer.arm.com/)
[![Emulation: QEMU vexpress-a15](https://img.shields.io/badge/Emulation-QEMU%20vexpress--a15-green.svg)](https://www.qemu.org/)
[![Architecture: SIFT TMR AMP](https://img.shields.io/badge/Architecture-SIFT%20TMR%20AMP-orange.svg)]()
[![Tests: 7/7 Passing](https://img.shields.io/badge/Tests-7%2F7%20Passing-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-purple.svg)](LICENSE)

A safety-critical **Software-Implemented Fault Tolerance (SIFT)** architecture using **Triple Modular Redundancy (TMR)** on a bare-metal quad-core **ARM Cortex-A15** processor (`vexpress-a15`). 

The system utilizes **Asymmetric Multiprocessing (AMP)** to assign deterministic, isolated flight control tasks across 4 physical CPU cores without operating system overhead, scheduling jitter, or cache-line contention.

---

## ⚡ Quickstart (Under 30 Seconds)

Clone the repository and run the automated quickstart for your platform:

### 🪟 Windows (Native)
```cmd
quickstart.bat
```
*Validates toolchain, compiles the ARMv7-A binary, executes 4-core QEMU emulation, displays the architecture matrix, and offers interactive visualizer options.*

### 🐧 Linux / macOS / WSL
```bash
chmod +x quickstart.sh
./quickstart.sh
```
*Requires `gcc-arm-none-eabi`, `qemu-system-arm`, and `python3` (`sudo apt-get install -y gcc-arm-none-eabi qemu-system-arm python3`).*

---

## 🧠 System Architecture & Execution Flow

```
+-------------------------------------------------------------------------------+
| Core 0: Master Arbiter (System Partition: 0x80000000 - 0x80FFFFFF)            |
|   • Boot initialization, zero .bss, QEMU holding pen wake (0x1C010030)         |
|   • Raw sensor data ingest & replicated distribution across isolated zones    |
|   • Inter-Processor Event (IPI) dispatch via SEV instruction                  |
|   • Spin-lock polling on volatile core_done[4] with hardware watchdog timer   |
|   • 2-out-of-3 (2oo3) Bounded Majority Voter (|Δ| <= 5 us)                    |
|   • Actuator command dispatch / Predefined Fail-Safe (-9999 us)               |
+-------------------------------------------------------------------------------+
       │                                │                                │
       ▼                                ▼                                ▼
+--------------------+   +--------------------+   +--------------------+
| Core 1 (Node 1)    |   | Core 2 (Node 2)    |   | Core 3 (Node 3)    |
| Zone 1 Partition   |   | Zone 2 Partition   |   | Zone 3 Partition   |
| In : 0x81000000    |   | In : 0x82000000    |   | In : 0x83000000    |
| Out: 0x81000004    |   | Out: 0x82000004    |   | Out: 0x83000004    |
| Stack: [sp - 4KB]  |   | Stack: [sp - 8KB]  |   | Stack: [sp - 12KB] |
| WFE Mailbox Poll   |   | WFE Mailbox Poll   |   | WFE Mailbox Poll   |
+--------------------+   +--------------------+   +--------------------+
```

### 1. Physical Spatial Memory Partitioning
Physical RAM (128MB: `0x80000000` to `0x88000000`) is strictly partitioned so redundant compute nodes share **zero operational variables**:

| Partition | Base Address Range | Assigned Core | Purpose |
|---|---|---|---|
| **System / Text** | `0x80000000 - 0x80FFFFFF` | Core 0 | Boot code, vector table, constants, `.bss`, Arbiter |
| **Zone 1 Input** | `0x81000000` | Core 1 | Isolated sensor data for Node 1 |
| **Zone 1 Output** | `0x81000004` | Core 1 | Isolated calculated PWM command for Node 1 |
| **Zone 2 Input** | `0x82000000` | Core 2 | Isolated sensor data for Node 2 |
| **Zone 2 Output** | `0x82000004` | Core 2 | Isolated calculated PWM command for Node 2 |
| **Zone 3 Input** | `0x83000000` | Core 3 | Isolated sensor data for Node 3 |
| **Zone 3 Output** | `0x83000004` | Core 3 | Isolated calculated PWM command for Node 3 |

### 2. Dynamic 4KB Stack Isolation
A contiguous 16KB stack block (`_stack_bottom` to `_stack_top`) is partitioned dynamically at boot based on the Core ID:

```text
sp = _stack_top - (core_id * 4096)
```

Each core operates within a private, collision-free 4KB stack boundary.

### 3. 2oo3 Bounded Majority Voter (|Δ| ≤ 5 µs)
- **Unanimous Consensus**: `|y₁ - y₂| ≤ 5`, `|y₂ - y₃| ≤ 5`, `|y₁ - y₃| ≤ 5`. Output is `(y₁ + y₂ + y₃) / 3`.
- **Outlier Masking**: If a cosmic ray Single Event Upset (SEU) flips bits in one node, the arbiter averages the two agreeing nodes and masks the corrupted outlier.
- **Fail-Safe Mode**: If no two cores agree within `|Δ| ≤ 5` (Total Disagreement), the voter commands **`-9999 µs`**, commanding actuators to a safe/power-off state.
- **Hardware Watchdog**: If a core hangs, Core 0 trips its watchdog counter and safely enters fail-safe **`-9999 µs`**.

---

## 🧪 Automated Fault Tolerance Test Suite

The automated test suite runs during boot and validates all 7 fault-injection scenarios:

| Test | Scenario | Injected Condition | Deltas (µs) | Voter Verdict | Commanded PWM |
|---|---|---|---|---|---|
| **Test 1** | Nominal Synchronous | Level attitude (`0 ddeg/s`) | `0, 0, 0` | UNANIMOUS | `1500 us` (PASS) |
| **Test 2** | Bounded Sensor Noise | Variance within `|Δ| ≤ 5` | `5, 4, 1` | UNANIMOUS (Averaged) | `1536 us` (PASS) |
| **Test 3** | Node 1 SEU Bit-Flip | Bit 9 flipped (`+512 µs`) | `485, 0, 485` | NODE 1 MASKED | `1515 us` (PASS) |
| **Test 4** | Node 2 SEU Bit-Flip | Bit 8 flipped (`-256 µs`) | `256, 256, 0` | NODE 2 MASKED | `1476 us` (PASS) |
| **Test 5** | Node 3 SEU Bit-Flip | Bit 10 flipped (`+1024 µs`) | `0, 545, 545` | NODE 3 MASKED | `1545 us` (PASS) |
| **Test 6** | Total Disagreement | Multi-channel corruption | `200, 340, 140` | FAIL-SAFE ENGAGED | `-9999 us` (SAFE) |
| **Test 7** | Core 2 Hardware Hang | Infinite loop / watchdog | `Timeout: 0x04` | WATCHDOG EXPIRED | `-9999 us` (SAFE) |

---

## 📊 Live Desktop Telemetry Monitor

Launch the desktop GUI monitor featuring real-time PL011 UART log streaming and live animated 2oo3 voter bar charts:

```cmd
python live_monitor.py
```

* **Live Telemetry Stream:** Reads raw serial telemetry directly from the PL011 UART (`0x1C090000`).
* **Dynamic Voter Graph:** Compares outputs from Node 1 (Core 1), Node 2 (Core 2), and Node 3 (Core 3) against the 2oo3 consensus command in real time.
* **Core Status Indicators:** Real-time health indicators showing online and fault status for Cores 0 through 3.

---

## 🛠️ Manual Build & Run Instructions

### Build with GNU Make:
```bash
make all
```

### Build with CMake:
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake ..
cmake --build .
```

### Run Directly in QEMU:
```bash
qemu-system-arm -M vexpress-a15 -cpu cortex-a15 -smp 4 -m 128M -nographic \
    -kernel tmr_flight_computer.elf -accel tcg,thread=multi
```
*(To exit QEMU, press `Ctrl + A` then `X`)*

---

## 📁 Repository Structure

```
.
├── src/
│   ├── startup.S               # Multicore assembly boot, MPIDR, FPEXC.EN, stack calculation
│   ├── main.c                  # Core 0 Arbiter, sensor distribution, test suite, continuous loop
│   ├── amp.h / amp.c           # Inter-core mailbox, secondary entry, core_done synchronization
│   ├── voter.h / voter.c       # 2oo3 Bounded Majority Voter (|Δ| <= 5) with 64-bit safe_diff
│   ├── flight_control.h / .c   # Deterministic flight control algorithm & SEU fault injector
│   ├── memory_map.h            # Physical memory addresses (Zones 1-3, System/Text, UART)
│   ├── uart.h / uart.c         # ARM PL011 UART console telemetry driver at 0x1C090000
│   └── types.h                 # Fixed-width types, DMB/DSB/ISB/SEV/WFE architectural barriers
├── linker.ld                   # Linker script defining 16MB System, 16KB stack, and Zone origins
├── live_monitor.py             # Native desktop Tkinter GUI monitor with real-time plots
├── quickstart.bat              # One-click review script for Windows
├── quickstart.sh               # One-click review script for Linux/macOS/WSL
├── test_flight.ps1             # Automated regression test runner
├── Makefile                    # Standard GNU Makefile
├── CMakeLists.txt              # CMake build configuration
├── toolchain-arm-none-eabi.cmake # CMake cross-compilation toolchain file
├── .gitignore                  # Git ignore rules for build artifacts
└── LICENSE                     # MIT Open Source License
```

---

## 📜 License
Released under the [MIT License](LICENSE).
