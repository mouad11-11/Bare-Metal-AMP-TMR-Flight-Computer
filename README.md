# Bare-Metal AMP TMR Flight Computer

[![Target: ARM Cortex-A15](https://img.shields.io/badge/Target-ARM%20Cortex--A15-blue.svg)](https://developer.arm.com/) [![Emulation: QEMU vexpress-a15](https://img.shields.io/badge/Emulation-QEMU%20vexpress--a15-green.svg)](https://www.qemu.org/) [![CI](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/actions/workflows/ci.yml/badge.svg)](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/actions/workflows/ci.yml) [![License: MIT](https://img.shields.io/badge/License-MIT-purple.svg)](LICENSE)

A bare-metal **flight computer architectural demonstrator** implementing **Software-Implemented Fault Tolerance (SIFT)** using **Triple Modular Redundancy (TMR)** on a quad-core **ARM Cortex-A15** (`vexpress-a15` target in QEMU).

The system uses **Asymmetric Multiprocessing (AMP)** to pin dedicated flight control tasks across 4 CPU cores without an operating system. Core 0 acts as the Master Arbiter, ingesting sensor data, dispatching frames, executing a 2-out-of-3 (2oo3) voter, and commanding actuators. Cores 1, 2, and 3 serve as isolated, redundant compute nodes calculating control laws independently.

---

## 🚀 Engineering Evolution Scorecard (v1.0 Prototype vs. v2.0 Hardened)

This repository demonstrates the engineering progression of a fault-tolerant multiprocessing architecture, advancing from a basic educational baseline into a hardened software-implemented fault tolerance implementation.

| Engineering Domain | Baseline Prototype (`v1.0-prototype`) | Hardened Architecture (`v2.0-hardened` / `main`) | Verification Proof |
|---|---|---|:---:|
| **Architecture Scope** | Educational demonstrator | **Hardened Fault-Tolerant Demonstrator (SIFT)** | [Traceability Matrix](docs/safety/TRACEABILITY.md) |
| **Voter Mechanism** | Raw arithmetic mean (susceptible to outliers) | **2oo3 Median Voter (`median3`) + Chain Ambiguity + Rate Limiter** | `T-VOTE-001..009` (236,807 assertions) |
| **Health Latching** | Stateless (faulty core re-enters next frame) | **Leaky-Bucket ($N = 3, M = 100$) Permanent Latch-Out** | `T-HLTH-001..004` (Tested in unit & FI suites) |
| **Execution Supervision**| Unbounded spin-waits (can freeze CPU) | **Software Watchdog (500k cycle timeout) + 5-State CFI Checkpoints** | `T-SUP-001..004` (State machine verified) |
| **Exception Handling**| 7 empty loops / hang on trap | **Full ARMv7-A 8-Vector Table + 2KB Banked Stacks + Fault Logs** | `T-FS-004` (Exception recording verified) |
| **Pre-Flight Diagnostics**| None (boots directly into flight loop) | **POST Suite (CPU Walking Patterns, RAM March C-, CRC32, Voter)** | `T-POST-001..006` (All diagnostics passing) |
| **Spatial Partitioning**| Single flat memory space | **Dedicated RAM Zones (`0x81M`, `0x82M`, `0x83M`) + Pre-computed MMU Tables** | `T-MMU-001..005` (Permissions verified) |
| **Inter-Core IPC** | Volatile shared memory (unprotected) | **Double-Buffered CRC32 Mailboxes + Frame Sequence Tokens** | `T-MBOX-001..004` (Corruption rejection verified) |
| **Arbiter Redundancy**| Core 0 single point of failure | **Dual-Rail Software Lockstep (Independent Algebraic Checking)** | `T-LOCK-001..003` (Glitch detection verified) |
| **Dynamic Memory** | Zero dynamic allocation | **Zero dynamic allocation (`malloc` prohibited, static linkage)** | Linker map inspection |
| **Host Unit Testing** | None (0 assertions) | **11 Native C Test Suites (246,094 assertions executed, 0 failed)** | `make test-host` (All 11 suites pass) |
| **Fault Injection** | Manual 7-frame demo | **113-Vector Automated FI Campaign (222 assertions verified, 0 failures)** | `make test-fi` (Native & QEMU passing) |

> 📌 **Repository Branch Navigation**:
> - **Active Flagship (`main`)**: The hardened fault-tolerant architecture with all safety mechanisms and verification suites.
> - **Baseline Archive ([`v1.0-prototype`](https://github.com/mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer/tree/v1.0-prototype))**: The original educational baseline prototype (commit `b9975e9`), preserved permanently for historical comparison.
> - **Comparison Documentation**: See [`docs/REVIEW_DOCUMENTATION.md`](docs/REVIEW_DOCUMENTATION.md) for the technical breakdown and side-by-side analysis.
> - **Architectural Schematics**: See [`docs/SCHEMATICS_AND_BLOCK_DIAGRAMS.md`](docs/SCHEMATICS_AND_BLOCK_DIAGRAMS.md) for side-by-side block diagrams.

---

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

```text
+-------------------------------------------------------------------------------+
| Core 0: Master Arbiter (System Partition: 0x80000000 - 0x80FFFFFF, 16MB)       |
|   • Boot initialization, zero .bss, QEMU holding pen wake (0x1C010030)         |
|   • Power-On Self-Test (CPU registers, March C- RAM, CRC32, golden voter)     |
|   • Raw sensor data ingest & distribution across isolated zones               |
|   • Inter-Processor Event (IPI) dispatch via SEV instruction                  |
|   • Spin-lock polling on volatile core_done[4] with 500k cycle timeout        |
|   • 2-out-of-3 (2oo3) Bounded Majority Voter (|Δ| <= 5 us)                    |
|   • Core 0 Dual-Rail Software Lockstep voter cross-check                      |
|   • Actuator command dispatch / Predefined Fail-Safe (-9999 us)               |
+-------------------------------------------------------------------------------+
       │                                │                                │
       ▼                                ▼                                ▼
+--------------------+   +--------------------+   +--------------------+
| Core 1 (Node 1)    |   | Core 2 (Node 2)    |   | Core 3 (Node 3)    |
| Zone 1 Partition   |   | Zone 2 Partition   |   | Zone 3 Partition   |
| In : 0x81000000    |   | In : 0x82000000    |   | In : 0x83000000    |
| Out: 0x81000004    |   | Out: 0x82000004    |   | Out: 0x83000004    |
| Stack: 8KB Core 1  |   | Stack: 8KB Core 2  |   | Stack: 8KB Core 3  |
| WFE Mailbox Poll   |   | WFE Mailbox Poll   |   | WFE Mailbox Poll   |
+--------------------+   +--------------------+   +--------------------+
```

### 1. Spatial Memory Partitioning
Memory regions are statically mapped in `linker.ld` so compute nodes operate in distinct address ranges:

| Partition | Base Address Range | Assigned Core | Purpose |
|---|---|---|---|
| **System / Text** | `0x80000000 - 0x80FFFFFF` (16MB) | Core 0 | Boot code, vector table, constants, `.bss`, Arbiter |
| **Zone 1 Input** | `0x81000000` (4KB section) | Core 1 | Isolated sensor data buffer for Node 1 |
| **Zone 1 Output** | `0x81000004` | Core 1 | Calculated actuator PWM output for Node 1 |
| **Zone 2 Input** | `0x82000000` (4KB section) | Core 2 | Isolated sensor data buffer for Node 2 |
| **Zone 2 Output** | `0x82000004` | Core 2 | Calculated actuator PWM output for Node 2 |
| **Zone 3 Input** | `0x83000000` (4KB section) | Core 3 | Isolated sensor data buffer for Node 3 |
| **Zone 3 Output** | `0x83000004` | Core 3 | Calculated actuator PWM output for Node 3 |

Additionally, ARMv7-A Short-Descriptor Level-1 translation tables are constructed in RAM (`mmu_init_tables()`), establishing 1MB section permissions (distinguishing Device peripherals, Normal RWX system RAM, and Execute-Never `XN` buffers) with software permission auditing (`mmu_check_permission()`).

### 2. Stack Sizing & Boundary Monitoring
A contiguous 32KB stack block (`_stack_bottom` to `_stack_top` in `linker.ld`) is partitioned dynamically at boot based on the Core ID:

```text
sp = _stack_top - (core_id * 8192)
```

Each core receives an 8KB stack allocation:
- **6KB for Supervisor (SVC) mode**: Primary execution stack.
- **2KB for banked exception modes**: Dedicated sub-stacks for Abort, Undefined, IRQ, and FIQ modes.
- **Stack canaries & watermarking**: `stack_monitor.c` places 4-word `0xDEADBEEF` canaries at partition and SVC stack boundaries and paints `0xA5A5A5A5` watermarks to monitor peak stack consumption and headroom.

### 3. Voter Consensus Logic & Plausibility
- **Unanimous Consensus**: If $|y_1 - y_2| \le 5$, $|y_2 - y_3| \le 5$, and $|y_1 - y_3| \le 5$, all three nodes agree. Output is the median value (`median3(y1, y2, y3)`).
- **Outlier Masking**: If one node deviates by $> 5\ \mu\text{s}$ (due to a transient bit-flip or algorithmic divergence), the voter averages the two agreeing nodes and masks the outlier.
- **Chain Ambiguity Resolution**: If two overlapping pairs agree (e.g., $|y_1 - y_2| \le 5$ and $|y_2 - y_3| \le 5$, but $|y_1 - y_3| > 5$), the median node is chosen and the outlier is attributed to the pair with the greater difference.
- **Degraded 2oo2 Mode**: If a node has latched out due to persistent faults, the system can arbitrate between the remaining two healthy nodes if $|y_a - y_b| \le 5\ \mu\text{s}$.
- **Rate Limiting**: Actuator commands are rate-limited to $|\Delta| \le 200\ \mu\text{s}$ per frame (`PWM_MAX_STEP_US`) to prevent physical actuator step transients.
- **Fail-Safe Command**: On total disagreement (no two nodes within $5\ \mu\text{s}$) or frame timeout, the voter commands `FAIL_SAFE_VALUE` (`-9999 µs`) to drive actuators to a safe/power-off state.

---

## 🧪 Bare-Metal Boot Verification Sequence

When the ARM binary executes in QEMU, Core 0 runs POST, synchronizes secondary cores, and executes a 7-frame validation sequence:

| Frame / Step | Scenario | Condition Evaluated | Deltas (µs) | Voter Verdict | Commanded PWM |
|---|---|---|---|---|---|
| **POST** | Pre-Flight Diagnostics | Registers, March C- RAM, CRC32, voter | N/A | ALL PASS | System Healthy |
| **Sync** | Secondary Core Wake | Holding pen wake & GIC SGI release | N/A | NODES 1..3 ONLINE | Ready |
| **Frame 1** | Nominal Synchronous | Level attitude sensor reading (`0 ddeg/s`) | `0, 0, 0` | UNANIMOUS | `1500 us` (PASS) |
| **Frame 2** | Bounded Estimator Noise| Sensor variance within $|Δ| \le 5\ \mu\text{s}$ | `5, 4, 1` | UNANIMOUS (Median) | `1536 us` (PASS) |
| **Frame 3** | Node 1 Bit-Flip | Synthetic single-bit flip on Node 1 | `485, 0, 485` | NODE 1 MASKED | `1515 us` (PASS) |
| **Frame 4** | Node 2 Bit-Flip | Synthetic single-bit flip on Node 2 | `256, 256, 0` | NODE 2 MASKED | `1476 us` (PASS) |
| **Frame 5** | Node 3 Bit-Flip | Synthetic single-bit flip on Node 3 | `0, 545, 545` | NODE 3 MASKED | `1545 us` (PASS) |
| **Frame 6** | Total Disagreement | Multi-channel corruption | `200, 340, 140`| FAIL-SAFE ACTIVATED | `-9999 us` (SAFE) |
| **Loop** | Trajectory Execution | 11 simulated dynamic pitch rate steps | Nominal | TRACKING | 11 frames pass |
| **Frame 7** | Core 2 Simulated Hang | Core 2 enters infinite loop; watchdog trips | Timeout | FAIL-SAFE ACTIVATED | `-9999 us` (SAFE) |
| **Diag** | Stack & Timing Audit | Canary integrity check & PMU cycle report | N/A | ALL CANARIES INTACT | Mission Standby |

---

## 📊 Live Desktop Telemetry Monitor

A desktop GUI monitor connects to the bare-metal output, displaying real-time UART logs and animated voter consensus graphs:

```cmd
python live_monitor.py
```

* **Live Telemetry Stream:** Reads raw serial telemetry directly from the PL011 UART (`0x1C090000`).
* **Dynamic Voter Graph:** Compares outputs from Node 1 (Core 1), Node 2 (Core 2), and Node 3 (Core 3) against the 2oo3 consensus command in real time.
* **Core Status Indicators:** Displays online status, latch-out states, and watchdog health across Cores 0 through 3.

---

## 🛠️ Build & Execution Instructions

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

## 🛡️ Technical Limitations & Threat Model

This project is an **architectural and algorithmic software demonstrator** developed to explore software-implemented fault tolerance patterns on multicore ARM processors.

### Covered Threats & Software Mitigations
- **Single-Bit Transient Errors (SEUs)**: Arithmetic bit-flips in compute node outputs or mailbox buffers are masked by the 2oo3 median voter.
- **Node Execution Timeouts**: Detected by Core 0's software cycle-countdown watchdog (500,000 cycle timeout) and handled via node isolation.
- **Control-Flow Corruption**: Secondary cores report monotonic CFI checkpoints (`INIT` → `READ_INPUT` → `COMPUTE` → `WRITE_OUTPUT` → `CANARY_CHECK` → `COMPLETE`). Non-monotonic transitions trigger supervisory faults.
- **Stack Boundary Breaches**: Monitored by 4-word `0xDEADBEEF` canaries audited before and after each frame dispatch.
- **Corrupted Inter-Core Payloads**: Double-buffered mailbox structures validate IEEE 802.3 CRC32 checksums and sequential frame tokens before data ingest.
- **Arbiter ALU Divergence**: Core 0 runs dual-rail software lockstep verification, cross-checking voter decisions against an independent algebraic implementation.

### Architectural & Simulation Boundaries
- **QEMU Emulation Platform**: QEMU operates via Dynamic Binary Translation (TCG). Instruction timing, cache hit/miss penalties, bus contention, and clock jitter are simulated and are not cycle-accurate. Performance Monitor Unit (PMU) readings reflect simulated instruction cycles.
- **Single-Die Silicon**: All four virtual cores reside in a single virtual machine instance. On physical silicon, a monolithic quad-core chip shares a single die, substrate, and power rail; physical damage to the die would affect all cores. Manned aerospace avionics mandate physical separation across independent microcontrollers or lockstep safety MCUs (such as Cortex-R5F / TMS570).
- **Watchdog Implementation**: Core 0's watchdog is a software spin-counter loop executed within the arbiter's thread. An unrecoverable hardware stall of the system bus would halt the counter. Flight systems require an external, physically independent windowed watchdog timer IC.
- **MMU Translation**: ARMv7-A Short-Descriptor Level-1 translation tables are generated in memory and verified in host tests; bare-metal QEMU execution relies on physical spatial memory partitioning and stack pointer isolation.
- **Certification Scope**: This project is an advanced software demonstrator. No airworthiness certification (FAA/EASA TSO or STC) has been applied for or granted.

---

## 🧪 Verification & Test Suite Summary

The verification pipeline comprises host unit tests, an automated fault-injection campaign, and bare-metal QEMU telemetry assertions:

| Test Harness | Scope & Modules Exercised | Exact Verified Result |
|---|---|---|
| `make test-host` | 11 native C unit test suites: Voter, Health, Fail-Safe, Stack Monitor, Supervision, POST, Safe Math, MMU, Lockstep, Mailbox, PMU, Diversity | **246,094 assertions executed, 0 failed** (includes 15,625-point voter stress grid) |
| `make test-fi` / `python tests/fi/run_fault_campaign.py` | Automated Fault Injection Campaign: Phase 1 (Native C) + Phase 2 (Bare-Metal QEMU) | **113 test vectors evaluated across 7 fault categories, 222 assertions verified, 0 failures; 11/11 QEMU assertions verified** |
| `python tools/check_traceability.py` | Safety Requirements Traceability Matrix Audit | **16/16 Safety Requirements (SR-001..SR-016) fully traced to hazards, code, and tests** |
| `python tests/check_uart_output.py` | QEMU Live Flight Telemetry Output Validator | **11/11 telemetry patterns verified** from UART output |
| `make all` | ARM GNU Toolchain bare-metal cross-compilation (`arm-none-eabi-gcc`) | Clean build, **0 warnings and 0 errors** (`-Wall -Wextra -Werror`) |

---

## 📁 Repository Structure

```text
.
├── src/
│   ├── startup.S               # Multicore assembly boot, 8-vector exception table, MPIDR, stacks
│   ├── main.c                  # Core 0 Arbiter, sensor distribution, test suite, flight loop
│   ├── config.h                # Safety parameters, tolerance thresholds, and operational bounds
│   ├── types.h                 # Fixed-width types, DMB/DSB/ISB/SEV/WFE architectural barriers
│   ├── memory_map.h            # Physical memory map (Zones 1-3, System/Text, UART, Holding Pen)
│   ├── safe_math.h             # Saturating 32-bit integer arithmetic library
│   ├── voter.h / voter.c       # 2oo3 Bounded Majority Voter with median consensus & rate limiter
│   ├── node_health.h / .c      # Leaky-bucket fault counters and permanent node latch-out
│   ├── failsafe.h / .c         # Fail-safe command state machine and reason code logging
│   ├── supervision.h / .c      # Frame deadline supervision, heartbeats, and CFI signatures
│   ├── post.h / .c             # Power-On Self-Test (CPU registers, March C- RAM, CRC32, voter)
│   ├── stack_monitor.h / .c    # Stack boundary canaries and peak watermarking diagnostics
│   ├── mmu.h / mmu.c           # ARMv7-A Short-Descriptor MMU tables & spatial partition setup
│   ├── lockstep.h / lockstep.c # Core 0 Dual-Rail Software Lockstep and voter self-monitoring
│   ├── mailbox.h / mailbox.c   # Double-buffered CRC32 inter-core mailbox protocol
│   ├── pmu.h / pmu.c           # Cortex-A15 Performance Monitor Unit (PMU) cycle profiler
│   ├── flight_control.h / .c   # Flight control laws (Primary & Diverse Q15) + SEU injector
│   └── uart.h / uart.c         # ARM PL011 UART console telemetry driver at 0x1C090000
├── docs/
│   ├── FAULT_MODEL.md          # Fault model, assumptions, and mitigation boundaries
│   ├── CODE_AUDIT.md           # Architectural audit of initial baseline implementation
│   ├── DISCREPANCIES.md        # Audit discrepancies and design reconciliations
│   ├── IPC_PROTOCOL.md         # Inter-Processor Communication protocol and barrier rules
│   ├── MEMORY_PROTECTION.md    # Spatial MMU translation tables, access permissions, and XN
│   ├── CODING_GUIDELINES.md    # Defensive C coding guidelines deviation catalog with rationale
│   ├── COVERAGE.md             # Structural verification and coverage analysis report
│   ├── FI_REPORT.md            # 113-vector fault injection campaign report
│   ├── PORTING_TO_SAFETY_MCU.md# Porting roadmap to lockstep silicon (Cortex-R5F, TMS570, AURIX)
│   └── safety/                 # Flight Safety Documentation Set
│       ├── SAFETY_PLAN.md      # Software Safety Plan (SSP)
│       ├── HAZARD_ANALYSIS.md  # System Hazard Analysis and Risk Assessment (HARA)
│       ├── SAFETY_REQUIREMENTS.md # Formal numbered requirements (SR-001..SR-016)
│       ├── FMEA.md             # Subsystem Failure Modes and Effects Analysis
│       ├── FTA.md              # Fault Tree Analysis with Top Event logic diagram
│       ├── COMMON_CAUSE_ANALYSIS.md # Common Cause Analysis (CCA) and independence defense
│       ├── ASSUMPTIONS_OF_USE.md # Assumptions of Use and operational envelope
│       ├── LIMITATIONS.md      # Technical limitations and simulation boundaries
│       └── TRACEABILITY.md     # Bi-directional traceability matrix (Hazards -> Reqs -> Tests)
├── tests/
│   ├── host/                   # 11 Native C host unit test harnesses (246,094 assertions)
│   ├── fi/                     # Automated fault-injection campaign harness and runner (113 vectors)
│   └── check_uart_output.py    # QEMU UART simulation output validator (11 assertions)
├── tools/
│   └── check_traceability.py   # Requirements traceability matrix validator (16/16 verified)
├── linker.ld                   # Linker script defining System partition, 32KB stacks, and Zone origins
├── Makefile / CMakeLists.txt   # Dual synchronized build systems
├── quickstart.bat / .sh        # Turnkey launchers for Windows and Linux/WSL
└── live_monitor.py             # Desktop Tkinter GUI telemetry monitor with animated voter plots
```

---

## 📜 License
Released under the [MIT License](LICENSE).
