# Bare-Metal AMP TMR Flight Computer

[![Target: ARM Cortex-A15](https://img.shields.io/badge/Target-ARM%20Cortex--A15-blue.svg)](https://developer.arm.com/)
[![Emulation: QEMU vexpress-a15](https://img.shields.io/badge/Emulation-QEMU%20vexpress--a15-green.svg)](https://www.qemu.org/)

[![CI](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/actions/workflows/ci.yml/badge.svg)](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-purple.svg)](LICENSE)

A safety-critical **flight computer architecture** implementing **Software-Implemented Fault Tolerance (SIFT)** using **Triple Modular Redundancy (TMR)** on a bare-metal quad-core **ARM Cortex-A15** processor (`vexpress-a15`), engineered for high reliability.

The system utilizes **Asymmetric Multiprocessing (AMP)** to statically pin isolated flight control tasks across 4 physical CPU cores without operating system overhead, targeting deterministic worst-case execution time (WCET), hardware spatial isolation, and zero dynamic memory allocation.

---

## 🚀 Engineering Evolution Scorecard (v1.0 Prototype vs. v2.0 Hardened)

This repository showcases the rigorous engineering evolution of a flight-critical avionics system, progressing from an educational prototype into a production-grade hardened fault-tolerant architecture.

| Engineering Domain | Baseline Prototype (`v1.0-prototype`) | Hardened Architecture (`v2.0-hardened` / `main`) | Verification Proof |
|---|---|---|:---:|
| **Reliability Goal** | Educational demonstrator | **Production-Grade Fault-Tolerant** | [Traceability Matrix](docs/safety/TRACEABILITY.md) |
| **Voter Mechanism** | Raw arithmetic mean (susceptible to outliers) | **2oo3 Median Voter + Chain Ambiguity Resolution** | `T-VOTE-001..009` (100% Branch) |
| **Health Latching** | Stateless (faulty core re-enters next frame) | **Leaky-Bucket ($N=3, M=100$) Permanent Latch-Out** | `T-HLTH-001..004` (100% Branch) |
| **Execution Supervision**| Unbounded spin-waits (can freeze CPU) | **Deadline Supervision (500k cycles) + CFI Tokens** | `T-SUP-001..003` (100% Branch) |
| **Exception Handling**| 7 empty loops / hang on trap | **Complete ARMv7-A 8-Vector Table + Context Logging** | `T-SAFE-001..003` (100% Branch) |
| **Pre-Flight Diagnostics**| None (boots directly into flight loop) | **Full POST Suite (CPU Registers, RAM March C-, CRC32)** | `T-POST-001..005` (100% Branch) |
| **Memory Protection** | Flat memory model (all cores RWX) | **ARMv7-A Short-Descriptor MMU + XN Partitions** | `T-MMU-001..003` (100% Branch) |
| **Inter-Core IPC** | Volatile shared memory (race conditions) | **Double-Buffered CRC32 Mailboxes + Sequences** | `T-MBOX-001..004` (100% Branch) |
| **Arbiter Redundancy**| Core 0 is Single Point of Failure (SPOF) | **Dual-Rail Software Lockstep + Inverted Arithmetic** | `T-LOCK-001..003` (100% Branch) |
| **Dynamic Memory** | Zero `malloc` | **Zero `malloc` (Statically mapped partitions)** | Deterministic Static Allocation |
| **Branch Coverage** | Untested on host (0%) | **100.00% Branch Coverage across 11 Test Suites** | `make test-host` (gcov) |
| **Fault Injection** | Manual 7-frame demo | **113-Vector Automated Fault-Injection Campaign** | `make test-fi` (0 Failures) |

> 📌 **Repository Branch Navigation**:
> - **Active Flagship (`main`)**: The hardened fault-tolerant architecture with all safety mechanisms and verification suites.
> - **Baseline Archive ([`v1.0-prototype`](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/tree/v1.0-prototype))**: The original educational baseline prototype (commit `b9975e9`), preserved permanently for historical comparison.
> - **Comparison Documentation**: See [`docs/REVIEW_DOCUMENTATION.md`](docs/REVIEW_DOCUMENTATION.md) for the full 700+ line technical breakdown and side-by-side analysis.
> - **Architectural Schematics**: See [`docs/SCHEMATICS_AND_BLOCK_DIAGRAMS.md`](docs/SCHEMATICS_AND_BLOCK_DIAGRAMS.md) for side-by-side ASCII and block diagrams.


## ⚡ Quickstart 

Clone the repository and run the automated quickstart for your platform:

### 🪟 Windows 
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
qemu-system-arm -M vexpress-a15 -cpu cortex-a15 -smp 4 -nographic \
    -kernel tmr_flight_computer.elf -accel tcg,thread=multi
```
*(To exit QEMU, press `Ctrl + A` then `X`)*

---

---

## 🛡️ Limitations and Threat Model

This project is an **architectural and algorithmic demonstrator** developed under rigorous software engineering principles for safety-critical flight avionics. 

### Covered Threats & Fault Mitigation
- **Transient Single Event Upsets (SEUs)**: Single-bit flips in compute node ALU registers, memory words, and mailbox payloads are transparently masked by the 2oo3 median voter.
- **Node Execution Hangs**: Detected by Core 0 hardware spin-counter watchdogs and isolated within 500,000 cycles.
- **Spatial Memory Corruption**: Prevented by ARMv7-A Short-Descriptor MMU tables enforcing isolated read-only code partitions and execute-never (`XN`) data pages, augmented by 4-word stack canaries (`0xDEADBEEF`).
- **Communication Inconsistencies**: Multi-rail inverted fields and IEEE 802.3 CRC32 checksums reject stale and corrupt mailbox packets.
- **Master Arbiter ALU Glitches**: Mitigated via Core 0 Dual-Rail Software Lockstep and continuous voter self-monitoring.

### Unmodeled Effects & Architectural Boundaries
- **QEMU Simulation Platform**: QEMU uses Dynamic Binary Translation (TCG). Instruction timing, cache hit/miss penalties, bus contention, and clock jitter are not cycle-accurate.
- **Physical Silicon Sharing**: All four virtual cores share a single virtual CPU package and power rail. True flight qualification requires physical separation across multiple silicon packages or lockstep Cortex-R5F cores.
- For complete details, see [`docs/FAULT_MODEL.md`](docs/FAULT_MODEL.md) and [`docs/safety/LIMITATIONS.md`](docs/safety/LIMITATIONS.md).

---

## 🧪 Comprehensive Verification & Test Commands

The project features a multi-tiered verification pipeline combining host unit tests, bare-metal QEMU simulation, and automated fault injection:

| Command | Subsystem Verified | Coverage / Pass Criteria |
|---|---|---|
| `make test-host` | Native C Host Unit Harness (Voter, Health, Failsafe, Supervision, POST, Math, MMU, Lockstep, Mailbox, PMU, Diversity) | **100% Branch Coverage**, MC/DC truth tables |
| `make test-fi` | Automated End-to-End Fault-Injection Campaign (113 vectors across 7 categories) | **0 Undetected Erroneous Outputs**, 11/11 QEMU assertions |
| `python tools/check_traceability.py` | Requirements Life-Cycle Bi-Directional Traceability Audit | **16/16 Safety Requirements Verified**, zero orphans |
| `python tests/check_uart_output.py` | Live QEMU Bare-Metal UART Telemetry Assertion Suite | **11/11 Telemetry Verdict Assertions Passing** |
| `make all` | Cross-compilation for ARM Cortex-A15 bare-metal target | Clean build, **0 warnings** (`-Wall -Wextra -Werror`) |

---

## 📁 Repository Structure

```
.
├── src/
│   ├── startup.S               # Multicore assembly boot, exception vectors, MPIDR, stack init
│   ├── main.c                  # Core 0 Arbiter, sensor distribution, test suite, continuous loop
│   ├── config.h                # Centralized safety parameters, bounds, and timing thresholds
│   ├── types.h                 # Fixed-width types, DMB/DSB/ISB/SEV/WFE architectural barriers
│   ├── memory_map.h            # Physical memory map (Zones 1-3, System/Text, UART, Holding Pen)
│   ├── safe_math.h             # Saturating 32-bit integer arithmetic library
│   ├── voter.h / voter.c       # Hardened 2oo3 Bounded Majority Voter with median consensus
│   ├── node_health.h / .c      # Leaky-bucket fault counters and permanent node latch-out
│   ├── failsafe.h / .c         # Deterministic fail-safe command state machine and reason codes
│   ├── supervision.h / .c      # Frame deadline supervision, heartbeats, and CFI signatures
│   ├── post.h / .c             # Power-On Self-Test (CPU registers, March C- RAM, CRC32, voter)
│   ├── stack_monitor.h / .c    # Stack boundary canaries and peak watermarking diagnostics
│   ├── mmu.h / mmu.c           # ARMv7-A Short-Descriptor MMU tables & spatial core isolation
│   ├── lockstep.h / lockstep.c # Core 0 Dual-Rail Software Lockstep and voter self-monitoring
│   ├── mailbox.h / mailbox.c   # Double-buffered CRC32 inter-core mailbox protocol
│   ├── pmu.h / pmu.c           # Cortex-A15 Performance Monitor Unit (PMU) WCET profiler
│   ├── flight_control.h / .c   # Flight control algorithms (Primary & Diverse Q15) + SEU injector
│   └── uart.h / uart.c         # ARM PL011 UART console telemetry driver at 0x1C090000
├── docs/
│   ├── FAULT_MODEL.md          # Comprehensive fault model, assumptions, and threat bounds
│   ├── CODE_AUDIT.md           # Deep architectural audit of initial baseline implementation
│   ├── DISCREPANCIES.md        # Audit discrepancies and design reconciliations
│   ├── IPC_PROTOCOL.md         # Inter-Processor Communication protocol and barrier rules
│   ├── MEMORY_PROTECTION.md    # Spatial MMU translation tables, access permissions, and XN
│   ├── CODING_GUIDELINES.md    # Defensive C coding guidelines deviation catalog with rationale
│   ├── COVERAGE.md             # 100% Statement, Branch, and MC/DC structural coverage report
│   ├── FI_REPORT.md            # Exhaustive 113-vector fault injection campaign report
│   ├── PORTING_TO_SAFETY_MCU.md# Porting roadmap to lockstep silicon (Cortex-R5F, TMS570, AURIX)
│   └── safety/                 # Flight Safety Documentation Set
│       ├── SAFETY_PLAN.md      # Software Safety Plan (SSP)
│       ├── HAZARD_ANALYSIS.md  # System Hazard Analysis and Risk Assessment (HARA)
│       ├── SAFETY_REQUIREMENTS.md # Formal numbered requirements (SR-001..SR-016)
│       ├── FMEA.md             # Subsystem Failure Modes, Effects, and Criticality Analysis
│       ├── FTA.md              # Fault Tree Analysis with Mermaid Top Event logic diagram
│       ├── COMMON_CAUSE_ANALYSIS.md # Common Cause Analysis (CCA) and independence defense
│       ├── ASSUMPTIONS_OF_USE.md # Assumptions of Use and operational envelope
│       ├── LIMITATIONS.md      # Honest technical limitations and simulation boundaries
│       └── TRACEABILITY.md     # Bi-directional traceability matrix (Hazards -> Reqs -> Tests)
├── tests/
│   ├── host/                   # Native C host unit test harnesses (100% branch coverage)
│   ├── fi/                     # Automated fault-injection campaign harness and runner
│   └── check_uart_output.py    # QEMU UART simulation output validator (11 assertions)
├── tools/
│   └── check_traceability.py   # Automated requirements traceability matrix validator
├── linker.ld                   # Linker script defining System partition, stacks, and Zone origins
├── Makefile / CMakeLists.txt   # Dual synchronized build systems
├── quickstart.bat / .sh        # Turnkey launchers for Windows and Linux/WSL
└── live_monitor.py             # Desktop Tkinter GUI telemetry monitor with animated voter plots
```

---

## 📜 License
Released under the [MIT License](LICENSE).
