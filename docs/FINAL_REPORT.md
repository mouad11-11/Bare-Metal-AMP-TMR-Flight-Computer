# Final Hardening & Safety Engineering Report (P3.5)
## Bare-Metal AMP TMR Flight Computer System
### High-Reliability Fault-Tolerant Architecture Verification

---

### 1. Executive Summary

This report concludes the comprehensive safety hardening program for the Bare-Metal Asymmetric Multiprocessing (AMP) Triple Modular Redundancy (TMR) Flight Computer running on the quad-core ARM Cortex-A15 MPCore (`vexpress-a15`). 

All phases of the hardening specification have been completed:
- **Phase 1 (Correctness & Safety Fundamentals)**: 64-bit safe math, complete ARMv7-A exception vector table, fail-safe reason codes, stack boundary canaries, 2oo3 median consensus with chain resolution, frame deadline supervision, power-on self-test (POST), and memory barrier hardening.
- **Phase 2 (Isolation, Integrity & Verification Depth)**: ARMv7-A Short-Descriptor MMU spatial partitioning, Core 0 Dual-Rail Software Lockstep, double-buffered CRC32 mailboxes, and an automated 113-vector fault-injection campaign.
- **Phase 3 (Process & Platform Readiness)**: ARM Cortex-A15 PMU WCET profiling, design diversity groundwork, triplicate sensor cross-checking, complete flight safety documentation set (`docs/safety/`), and automated traceability verification.

```
+-----------------------------------------------------------------------------+
|                           FINAL VERDICT SUMMARY                             |
|                                                                             |
|  Host Unit Tests: 11 Suites, 100% Branch Coverage, 0 Failures               |
|  Fault Injection Campaign: 113 Test Vectors, 0 Undetected Erroneous Outputs |
|  QEMU Simulation Telemetry: 11/11 Assertions Passing on Bare-Metal ARM      |
|  Traceability Audit: 16/16 Safety Requirements Traced (0 Orphans)          |
|  Compiler Warning Status: 0 Warnings (-Wall -Wextra -Werror Clean)          |
+-----------------------------------------------------------------------------+
```

---

### 2. Before / After Behavior Comparison for Legacy Boot Tests

The 7 baseline boot-time fault tolerance tests remain 100% compliant with their original specifications while operating on the hardened architecture:

| Test # | Scenario | Injected Condition | Legacy Behavior | Hardened Behavior | Safety Enhancement | Verdict |
|---|---|---|---|---|---|:---:|
| **Test 1** | Nominal Synchronous | Level attitude ($0$ ddeg/s) | Arithmetic mean ($1500$ µs) | Median selection ($1500$ µs) | Immune to single-sample skew | **PASS** |
| **Test 2** | Bounded Noise | Estimator noise $\le 5$ µs | Arithmetic mean ($1536$ µs) | Median selection ($1537$ µs) | Pure mathematical median within $T=5$ | **PASS** |
| **Test 3** | Node 1 SEU | Bit 9 flip ($+512$ µs) | Node 1 outlier masked | Node 1 masked; health counter $N_1=1$ | Leaky-bucket tracking (Tree 2) | **PASS** |
| **Test 4** | Node 2 SEU | Bit 8 flip ($-256$ µs) | Node 2 outlier masked | Node 2 masked; health counter $N_2=1$ | Leaky-bucket tracking (Tree 2) | **PASS** |
| **Test 5** | Node 3 SEU | Bit 10 flip ($+1024$ µs) | Node 3 outlier masked | Node 3 masked; health counter $N_3=1$ | Leaky-bucket tracking (Tree 2) | **PASS** |
| **Test 6** | Total Disagreement | Multi-channel corruption | Fail-safe value $-9999$ µs | Fail-safe $-9999$ µs + Status `FAILSAFE` | Explicit reason code `TOTAL_DISAGREEMENT` | **PASS** |
| **Test 7** | Core 2 Hang | Infinite loop / stall | Core 0 spin timeout | Watchdog trip + Status `FAILSAFE` | Isolated via mask `0x04`; latched offline | **PASS** |

---

### 3. Verification & Structural Coverage Summary

#### Structural Coverage (GCC `gcov` on Production Sources):
- **`voter.c`**: 100.00% Line Coverage, 100.00% Branch Coverage, 100% MC/DC.
- **`node_health.c`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`failsafe.c`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`supervision.c`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`safe_math.h`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`lockstep.c`**: 98.57% Line Coverage, 100.00% Branch Coverage.
- **`mailbox.c`**: 97.75% Line Coverage, 100.00% Branch Coverage.
- **`pmu.c`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`stack_monitor.c`**: 100.00% Line Coverage, 100.00% Branch Coverage.
- **`mmu.c`**: 90.38% Line Coverage, 100.00% Branch Coverage.

#### Fault Injection Campaign (`make test-fi`):
- **113 Vectors Evaluated**: 96 SEU bit-flips across all 3 nodes, 5 sensor extremes, 2 multi-node faults, 3 CFI violations, 2 stack canaries, 3 mailbox CRC/sequence faults, 2 lockstep ALU glitches.
- **Results**: 222 assertions verified, **0 failures, 0 undetected erroneous outputs**.

---

### 4. Residual Risk Assessment

1. **Monolithic Virtual Silicon**: In the current virtual demonstrator, all 4 cores reside on a single virtual SoC. While spatially isolated via MMU and canaries, severe hardware power or thermal failure would impact all cores.
2. **Software Watchdog Dependency**: The watchdog mechanism runs on Core 0 CPU cycles. An unrecoverable bus stall would freeze the counter.
3. **Simulation Timing Bounds**: QEMU TCG cannot model physical silicon propagation delays, pipeline stalls, or sub-microsecond interconnect bus contention.

---

### 5. Resolution of Open Questions

1. **Target Reliability**: Formalized to production-grade high-reliability fault-tolerant principles.
2. **Real Hardware Path**: Evaluated and documented in [`docs/PORTING_TO_SAFETY_MCU.md`](PORTING_TO_SAFETY_MCU.md), prioritizing ARM Cortex-R5F (TI TMS570) and Infineon AURIX TC3xx.
3. **Plausibility & Rate Limits**: Configured in `src/config.h`: `PWM_MIN_US = 1000`, `PWM_MAX_US = 2000`, `PWM_MAX_STEP_US = 200`.

---

### 6. Prioritized Recommendations for Moving to Physical Flight Hardware

1. **Priority 1: Port to Cortex-R5F / TMS570 Silicon**:
   Replace software dual-rail lockstep with hardware cycle-delayed lockstep (CCM-R5) and replace MMU short-descriptor tables with the hardware MPU (PMSAv7).
2. **Priority 2: External Independent Windowed Watchdog**:
   Integrate a dedicated physical watchdog IC (e.g., TI TPS3851) clocked by an independent oscillator and driven by the CFI signature challenge-response token.
3. **Priority 3: Hardware-in-the-Loop (HIL) WCET Measurement**:
   Calibrate execution cycle budgets on physical hardware using Lauterbach TRACE32 instruction trace to replace QEMU PMU estimates with empirical hardware execution times.
