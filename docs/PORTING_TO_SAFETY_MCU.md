# Safety Microcontroller Porting Roadmap (P3.4)
## Migrating Bare-Metal AMP TMR Architecture to Flight-Grade Silicon (Cortex-R5F, TMS570, AURIX, S32K)

---

### 1. Executive Summary & Purpose

The Bare-Metal Asymmetric Multiprocessing (AMP) Triple Modular Redundancy (TMR) demonstrator is currently implemented on an ARM Cortex-A15 MPCore (`vexpress-a15`) simulated in QEMU. While Cortex-A series processors excel in high-throughput computation, safety-critical aerospace and automotive systems (High-Reliability Flight Systems) typically target dedicated safety microcontrollers such as:
- **Texas Instruments Hercules TMS570** (Dual Cortex-R4F/R5F hardware lockstep)
- **Infineon AURIX TC3xx / TC4xx** (TriCore 6-core multi-lockstep)
- **NXP S32K3 / S32Z / S32E** (Dual Cortex-M7 / Cortex-R52)
- **Renesas RH850/U2A** (Lockstep 32-bit automotive MCU)

This document provides a concrete architectural roadmap for porting the software modules, replacing software-emulated defenses with silicon-native hardware safety mechanisms.

---

### 2. Hardware Architecture Comparison

| Architectural Feature | Current Architecture (QEMU Cortex-A15) | Target Safety Silicon (e.g., TI TMS570 / Infineon AURIX) |
|---|---|---|
| **CPU Lockstep** | Software Dual-Rail Lockstep (`src/lockstep.c`) | Hardware 2-cycle delayed lockstep comparison logic (CCM-R4/R5) |
| **Memory Protection** | MMU Short-Descriptor Tables (`src/mmu.c`) | MPU (PMSAv7 / PMSAv8) with 16/32 hardware region registers |
| **SRAM / Flash ECC** | Software March C- POST test (`src/post.c`) | Hardware Single-Error-Correction Double-Error-Detection (SEC-DED) |
| **Watchdog Peripheral** | Software loop counter / SP804 timer (`src/supervision.c`) | Independent Windowed Hardware Watchdog with challenge-response token |
| **Clock Supervision** | Shared system clock in simulator | Dual independent oscillators with Clock Monitor Unit (CMU) & PLL slip detect |
| **Power Domain** | Single virtual power rail | Dual independent voltage rails with Brown-Out Detectors (BOD) |
| **Inter-Core IPC** | Shared RAM mailboxes + `SEV`/`WFE` (`src/amp.c`) | Hardware Inter-Core Mailbox unit (IPC) with hardware semaphores & IRQ |

---

### 3. Firmware Module Portability Matrix

The codebase was deliberately designed with strict separation between architecture-independent safety algorithms and hardware-dependent device drivers:

| Source Module | Portability Status | Required Migration Actions |
|---|:---:|---|
| **`voter.c` / `voter.h`** | **100% PORTABLE (Unchanged)** | Drop-in replacement. Pure freestanding C99, integer arithmetic, zero platform dependencies. |
| **`node_health.c` / `node_health.h`** | **100% PORTABLE (Unchanged)** | Drop-in replacement. Leaky-bucket fault counters and latching logic are platform-agnostic. |
| **`safe_math.h`** | **100% PORTABLE (Unchanged)** | Drop-in replacement. Saturating 32-bit arithmetic works identically on all 32-bit RISC targets. |
| **`failsafe.c` / `failsafe.h`** | **100% PORTABLE (Unchanged)** | Drop-in replacement. Reason codes, latching, and command formatting require no modifications. |
| **`supervision.c` / `supervision.h`** | **90% PORTABLE (Minor Adapt)** | Retain CFI signatures and deadline state; map watchdog tick source to hardware timer (e.g., RTI). |
| **`pmu.c` / `pmu.h`** | **85% PORTABLE (Minor Adapt)** | Cortex-R5F uses identical CP15 cycle counter registers (`c9, c12, c13`). On AURIX/RH850, map to STM/CCNT. |
| **`post.c` / `post.h`** | **75% PORTABLE (Minor Adapt)** | Adapt CPU register and voter tests. Replace March C- with hardware ECC Built-In Self-Test (BIST). |
| **`mailbox.c` / `mailbox.h`** | **70% PORTABLE (Minor Adapt)** | Retain double-buffered CRC32 protocol; bind memory pointers to dedicated inter-core SRAM partition. |
| **`mmu.c` / `mmu.h`** | **REWRITE AS `mpu.c`** | Replace 16 KB page tables with 16 MPU hardware regions (Region Base, Size, Sub-region disable, Access control). |
| **`startup.S`** | **REWRITE FOR TARGET** | Replace Cortex-A15 holding pen boot with target reset vector, VFP/FPU init, and hardware lockstep check. |
| **`amp.c`** | **REWRITE FOR TARGET** | Replace memory-mapped holding pen release with target core start registers or hardware reset release. |

---

### 4. Step-by-Step Porting Guide to Cortex-R5F (e.g., TMS570LC4357)

#### Step 1: Memory Protection Unit (MPU) Initialization (`mpu.c`)
Replace translation table section generation with CP15 MPU region programming:
```c
/* Example Cortex-R5F MPU Region Programming */
void mpu_setup_region(uint32_t region_num, uint32_t base_addr, uint32_t size_encoded, uint32_t access_ctrl) {
    __asm__ volatile("mcr p15, 0, %0, c6, c2, 0" : : "r"(region_num));
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 0" : : "r"(base_addr));
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 4" : : "r"(access_ctrl));
    __asm__ volatile("mcr p15, 0, %0, c6, c1, 2" : : "r"(size_encoded | 1U)); /* Enable */
}
```

#### Step 2: Hardware Lockstep Configuration
On Cortex-R5F, Core 0 and Core 1 can be configured in hardware lockstep:
- Enable Core Comparison Module (CCM-R4/R5).
- If redundant execution diverges by even 1 clock cycle, CCM asserts an NMI/ESM error without consuming software CPU cycles.
- Software dual-rail lockstep in `src/lockstep.c` remains as an orthogonal defense against internal voter arithmetic errors.

#### Step 3: Hardware Windowed Watchdog Integration
Replace the software watchdog counter in `src/supervision.c` with the Digital Windowed Watchdog (DWD):
- Program closed window (e.g., first 50% of frame duration) and open window (final 50%).
- Kick watchdog via pseudorandom challenge-response token generated by `supervision_get_cfi_signature()`.
- Premature kicks (CFI skip) or overdue kicks (deadline miss) trigger immediate hardware reset.
