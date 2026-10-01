# System Fault Model & Threat Specification

**Document Version:** 1.0  
**Project:** Bare-Metal AMP TMR Flight Computer  
**Target Platform:** Quad-Core ARM Cortex-A15 MPCore (`vexpress-a15`)  
**Safety Classification:** DO-178C Level A / ECSS Architecture Demonstrator  

---

## 1. Overview & Purpose
This document defines the formal fault model, fault containment boundaries, failure modes, mitigation strategies, and residual risks for the Bare-Metal Asymmetric Multiprocessing (AMP) Triple Modular Redundancy (TMR) Flight Computer.

---

## 2. Covered Fault Classes (Masked in Real Time)
The system is architected to detect, isolate, and mask the following single-point failure modes within a single flight control frame without vehicle trajectory disturbance:

1. **Single Transient Bit Flip (SEU) in Compute Node State**:
   - Ionizing radiation strikes altering register values, program counter (PC), ALU flags, or arithmetic intermediates on any single worker core (Core 1, Core 2, or Core 3).
   - *Mitigation*: The 2-out-of-3 bounded majority voter (|delta| <= 5 µs) isolates the divergent node and commands the mean/median of the two agreeing nodes.
2. **Single Bit Flip in Node Input or Output Memory Zones**:
   - Single Event Upset in dedicated physical RAM zones (`0x81000000`, `0x82000000`, or `0x83000000`).
   - *Mitigation*: Redundant data integrity checks (`~value` complement check or CRC) and out-of-bounds plausibility rejection.
3. **Single Node Execution Hang (Hardware Lockup / Infinite Loop)**:
   - Worker core trapped in an unresponsive state, unhandled exception, or infinite spin.
   - *Mitigation*: Arbiter frame deadline watchdog counter / timer. The timed-out core is isolated, flagged in the fault mask, and excluded from quorum.
4. **Single Node Late, Missing, or Stale Sequence Output**:
   - Node finishes computation after the frame deadline has expired or delivers stale data from frame `k - 1`.
   - *Mitigation*: Monotonically incrementing `frame_id` token echoed by each worker node. Mismatched sequence numbers are rejected prior to voting.
5. **Single Corrupted Mailbox Record**:
   - Bit-flip in dispatch pointer (`secondary_spin_addr`) or completion flag (`core_done[i]`).
   - *Mitigation*: Strict memory ordering barriers (`DMB`/`DSB`/`ISB`), double-buffered sequence verification, and bounded handshake timeouts.

---

## 3. Detected Failure Classes (Unmasked -> Safe Transition)
When fault multiplicity exceeds single-node containment limits or threatens arbiter integrity, the system transitions deterministically to the **SAFE** state (commanding neutral actuator positions, asserting `FAILSAFE` status, and logging reason codes):

1. **Total Disagreement (Quorum Loss)**:
   - Multiple simultaneous bit flips or sensor errors where no two nodes agree within the agreement threshold ($\Delta_{\text{thresh}} \le 5\,\mu\text{s}$).
   - *Action*: Voter invokes `SAFE(TOTAL_DISAGREEMENT)`.
2. **Multiple Node Failure (Insufficient Healthy Nodes)**:
   - Failure of 2 or more worker nodes in a single frame. When fewer than 2 valid nodes exist (or when degraded 2oo2 mode is disabled/disagrees).
   - *Action*: `SAFE(INSUFFICIENT_NODES)` or `SAFE(CANNOT_ARBITRATE)`.
3. **Core 0 (Master Arbiter) Exception or Hardware Abort**:
   - Data abort, prefetch abort, or undefined instruction occurring within Core 0 execution context.
   - *Action*: Exception trap drives `SAFE(EXCEPTION)` immediately, parks in fail-safe mode, and inhibits watchdog kicks.
4. **Core 0 Critical Variable / Memory Corruption**:
   - Bit flips in voter state, node health counters, or page translation tables.
   - *Action*: Duplicated storage / bitwise complement mismatch or periodic background memory scrubbing triggers `SAFE(INTEGRITY_FAIL)`.
5. **Frame Deadline Overrun**:
   - Execution time of the complete frame (dispatch, compute, vote, actuate) exceeds hard real-time cyclic budget.
   - *Action*: `SAFE(DEADLINE)`.
6. **Power-On Self-Test (POST) Failure**:
   - Register fault, RAM march test failure, ROM/code CRC checksum mismatch, or peripheral timeout during initial boot.
   - *Action*: `SAFE(POST_FAIL)` before flight outputs are ever enabled.

---

## 4. Uncovered Failure Classes & Residual Risks
The following hazards cannot be mitigated by software-implemented fault tolerance on a single silicon die and constitute residual risks:

1. **Core 0 as Single Point of Failure (SPOF)**:
   - While Core 0 integrity is monitored via internal checks and watchdog timeouts, a permanent physical silicon failure of CPU Core 0, its local interconnect, or its L1 interrupt controller prevents flight computer execution.
2. **Common-Mode Software Faults (CMF)**:
   - Systematic software bugs in the shared flight control algorithm, voter logic, or compiler toolchain will replicate identically across Cores 1, 2, and 3, evading majority voting.
3. **Common Sensor Ingestion Corruption**:
   - An analog sensor failure or ADC bus corruption feeding identical erroneous data to Core 0 will be replicated identically across all three zones. (Addressed in Phase 3 by multi-channel independent input ingestion).
4. **Die-Level Common Hardware Disturbances**:
   - Complete loss of primary supply voltage, clock oscillator jitter/drift, thermal shutdown, or Single Event Latchup (SEL) short-circuiting the SoC substrate.
5. **Simultaneous Correlated Multi-Bit Upsets (MBU)**:
   - Heavy radiation ion strike causing dense cluster ionization across multiple cores simultaneously before scrubbing intervals.
6. **Emulation Fidelity Limitations**:
   - QEMU TCG (`thread=multi` or `thread=single`) simulates architectural instruction execution, not physical silicon electrical properties, bus arbitration contention, cache line eviction timing, or thermal noise. Real-time WCET cannot be validated on QEMU.

---

## 5. System Safety Assumptions
1. **Fault Arrival Rate**: The mean time between single event upsets (MTBF) is significantly larger than the frame cycle period (10–20 ms) and node health decay window ($M = 100$ frames), ensuring faults do not accumulate faster than they are isolated or scrubbed.
2. **Initial Condition**: Hardware boots from a fault-free reset state and successfully passes all Power-On Self-Tests (POST) prior to actuator engagement.
3. **Actuator Fail-Safe Integrity**: Downstream actuator electronics and flight surfaces monitor the independent `status` field and implement independent hardware loss-of-signal auto-neutralization (`ACTUATOR_TIMEOUT_MS`).
4. **Memory Retention**: Linker-defined memory partitions remain distinct physical RAM banks on target silicon.
