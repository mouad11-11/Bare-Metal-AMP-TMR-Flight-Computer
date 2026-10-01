# System Limitations & Safety Boundary Constraints
## Bare-Metal AMP TMR Flight Computer System
### Safety Boundary Constraints & Technical Scope

---

### 1. Honest Avionics Systems Engineering Disclosure

In strict accordance with professional engineering integrity standards, this document establishes the explicit technical boundaries, unmodeled physical effects, and residual constraints of the current Bare-Metal AMP TMR implementation.

---

### 2. Architectural Limitations

#### 1. Simulation Platform Limitations (QEMU vs Real Hardware)
- **Non-Real-Time Execution**: QEMU uses Dynamic Binary Translation (TCG). Instruction timing, cache hit/miss penalties, bus arbitration latency, and pipeline stall penalties are not cycle-accurate.
- **Timing Benchmarks**: While the Performance Monitor Unit (PMU) driver measures simulated cycle counts, true Worst-Case Execution Time (WCET) must be re-measured on physical silicon using a calibrated logic analyzer or hardware trace probe (e.g., Lauterbach TRACE32).
- **Physical Radiation**: QEMU models software bit-flips via arithmetic XOR operations. It cannot simulate physical single-event latchup (SEL), gate rupture (SEGR), total ionizing dose (TID), or clock tree jitter.

#### 2. Watchdog Independence
- In the current virtual demonstrator, the watchdog spin-counter executes within Core 0's instruction stream.
- An unrecoverable hardware freeze of the Cortex-A15 system bus would stall both the computation and the software counter.
- **Production Requirement**: A flight deployment requires an external, physically independent windowed watchdog IC (e.g., TI TPS3851 or dedicated RTI on TMS570) clocked by a separate quartz oscillator.

#### 3. Single-Die Silicon Sharing
- All four Cortex-A15 cores reside on a single monolithic silicon die.
- Although isolated through spatial MMU page tables, catastrophic physical damage to the die (power rail short-circuit, severe thermal runaway) would compromise all four cores simultaneously.
- For manned aerospace flight control (Safety-Critical Flight Control), physical redundancy across 3 physically distinct microcontrollers or dissimilar boards is mandated.

#### 4. Compliance and Certification Status
- This project implements the architectural principles, defensive programming rules, and verification artifacts (100% MC/DC, fault injection) required for high-reliability flight software.
- **Certification Statement**: This project serves as an **advanced architectural demonstrator**. No airworthiness certification (FAA/EASA TSO or STC) has been applied for or granted.
