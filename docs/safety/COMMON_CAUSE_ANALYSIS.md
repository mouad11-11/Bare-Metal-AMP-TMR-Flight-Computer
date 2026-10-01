# Common Cause Analysis (CCA) & Independence Assessment
## Common Mode & Particular Risks in Multicore Redundant Architectures
### Reference: ARP4761 / DO-297 / CAST-32A / DO-178C

---

### 1. Purpose & Regulatory Context

In redundant fault-tolerant systems (TMR), the assumption of independence between redundant channels can be invalidated by **Common Cause Failures (CCF)**, where a single environmental, design, or hardware trigger causes simultaneous failure of multiple channels.
This document analyzes all five common-mode risk domains and defines architectural mitigations.

---

### 2. Common Cause Failure Domains & Mitigation Matrix

| Common Cause Domain | Specific Failure Threat | Impact on TMR Architecture | Architectural Mitigation & Defense |
|---|---|---|---|
| **1. Shared Sensor Input** | Transient noise, ADC drift, or sensor stuck-at value ingested by Core 0 and broadcast to all three nodes. | All three nodes compute identical incorrect PWM commands; voter cannot detect fault. | **Triplicate Sensor Validation (P3.3)**: Independent sensor channels voted prior to computation. Rate-of-change limiter ($\le 200$ µs/frame) and saturating bounds prevent wild jumps. |
| **2. Shared Software Code** | Systematic software bug in control algorithm or integer overflow logic present on all cores. | Identical software defect causes all three cores to fail simultaneously in an identical manner. | **Design Diversity (P3.2)**: Node 2 runs an independently formulated Q15 fixed-point algorithm (`flight_control_compute_diverse`). NASA Power of Ten rules eliminate unbounded constructs. |
| **3. Shared Toolchain & Compiler** | GCC code generation bug or aggressive `-O2` optimization defect corrupting control logic. | Bug manifests across all nodes compiled with the same optimization flags. | Formal host verification (`tests/host/`), diverse math formulations, and CBMC/formal property verification. |
| **4. Shared Clock & Power Silicon** | Supply voltage brownout, ground bounce, or oscillator PLL lock slip affecting multicore die. | All four cores experience clock glitch or brownout simultaneously. | Target safety MCU porting plan ([`docs/PORTING_TO_SAFETY_MCU.md`](file:///c:/Users/hp/Desktop/TMR/docs/PORTING_TO_SAFETY_MCU.md)) specifies dual independent oscillators and hardware brownout monitors. |
| **5. Shared Memory & Bus Contention** | Runaway pointer or DMA write from one core clobbering another core's stack or mailbox. | Single rogue core corrupts arbitration logic or peer node outputs. | **Spatial MMU Partitioning (P2.1)**: Short-descriptor page tables enforce hardware read-only and no-access rules. Stack canaries detect boundary breaches. |

---

### 3. Independence Verification Summary

Through the combination of **Spatial Memory Isolation (MMU)**, **Algorithmic Diversity (Q15 formulation on Node 2)**, **Triplicate Sensor Cross-Checking**, and **Core 0 Dual-Rail Software Lockstep**, the architecture establishes verifiable independence between compute channels in accordance with CAST-32A multicore certification objectives.
