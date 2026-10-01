# Bare-Metal AMP TMR Flight Computer: Architecture & Execution Model

## 1. System Overview

This project implements a bare-metal **Software-Implemented Fault Tolerance (SIFT)** architecture utilizing **Triple Modular Redundancy (TMR)** on a quad-core **ARM Cortex-A15** processor (`vexpress-a15` target in QEMU).

The system executes in **Asymmetric Multiprocessing (AMP)** mode without an operating system or hypervisor. Each physical core is statically assigned a dedicated role:

```text
+-----------------------------------------------------------------------------------+
|                        Core 0: Master Arbiter (System RAM)                        |
|  • Bootstraps hardware, zeroes .bss, wakes secondary cores from holding pen       |
|  • Executes Power-On Self-Test (CPU registers, March C- RAM, CRC32, voter)        |
|  • Ingests raw sensor telemetry and distributes independent copies to zones       |
|  • Dispatches frames via SEV and synchronizes via volatile completion flags       |
|  • Enforces frame deadline supervision via a 500,000-cycle watchdog countdown     |
|  • Computes 2-out-of-3 (2oo3) bounded majority vote using median selection        |
|  • Performs dual-rail software lockstep cross-checking of all voter calculations  |
|  • Applies output rate limiting (|Δ| <= 200 us) and commands actuators / failsafe |
+-----------------------------------------------------------------------------------+
           │                                 │                                 │
           ▼                                 ▼                                 ▼
+-----------------------+         +-----------------------+         +-----------------------+
|   Core 1 (Node 1)     |         |   Core 2 (Node 2)     |         |   Core 3 (Node 3)     |
| • Spatial Zone 1      |         | • Spatial Zone 2      |         | • Spatial Zone 3      |
| • 8KB Private Stack   |         | • 8KB Private Stack   |         | • 8KB Private Stack   |
| • Ingests Zone 1/MBox |         | • Ingests Zone 2/MBox |         | • Ingests Zone 3/MBox |
| • Primary Flight Law  |         | • Diverse/Primary Law |         | • Primary Flight Law  |
| • Computes PWM Output |         | • Computes PWM Output |         | • Computes PWM Output |
| • Reports CFI Tokens  |         | • Reports CFI Tokens  |         | • Reports CFI Tokens  |
| • Waits in WFE Loop   |         | • Waits in WFE Loop   |         | • Waits in WFE Loop   |
+-----------------------+         +-----------------------+         +-----------------------+
```

---

## 2. Multicore Boot & Pre-Flight Initialization

### 2.1 Multiprocessor Affinity Routing (`src/startup.S`)
At system reset, all 4 cores enter `_start`. Each core reads its Multiprocessor Affinity Register (`MPIDR`, `CP15 c0, c0, 5`):
- Bits `[7:0]` provide the CPU ID (0 to 3).
- Bits `[15:8]` provide the Cluster ID.
- Core ID is computed: `core_id = (cluster << 2) | cpu_id`.

```text
[Core 0 (Master Arbiter)]               [Cores 1..3 (Secondary Compute Nodes)]
          │                                                │
          ├─ Set VBAR exception vector table               ├─ Set VBAR exception vector table
          ├─ Enable VFP/NEON coprocessors (CP10/CP11)      ├─ Enable VFP/NEON coprocessors
          ├─ Partition 8KB private stack (SVC + Banked)    ├─ Partition 8KB private stack
          ├─ Zero .bss section                             └─ Enter secondary_boot:
          ├─ Call main()                                        └─ Sleep in WFE holding loop
          │                                                          until woken by Core 0
```

### 2.2 Stack Partitioning
A contiguous 32KB stack region (`0x8000` bytes in `linker.ld`) is statically reserved between `_stack_bottom` and `_stack_top`. Each core calculates its stack pointer at boot:

$$\text{sp}_{\text{core}} = \text{\_stack\_top} - (\text{core\_id} \times 8192)$$

Within each 8KB partition:
- **6,144 bytes (6KB)**: Allocated for Supervisor (SVC) mode runtime stack.
- **2,048 bytes (2KB)**: Subdivided into 512-byte banked stacks for exception modes (Abort, Undefined, IRQ, FIQ).
- **Canary Boundaries**: 4-word `0xDEADBEEF` canaries are positioned at the bottom of each 8KB partition and at the base of the SVC stack.
- **Watermarking**: Unused stack memory is painted with `0xA5A5A5A5` at boot to measure peak consumption and headroom during mission diagnostics (`stack_monitor.c`).

### 2.3 Secondary Core Release (`src/amp.c`)
In QEMU `vexpress-a15`, secondary cores sleep in the board holding pen (`SYS_FLAGS` at `0x1C010030` and `0x10000030`). Core 0 wakes them by:
1. Writing the entry address (`_start`) into both `SYS_FLAGS` registers.
2. Enabling the ARM Generic Interrupt Controller (GIC) Distributor (`0x2C001000`) and CPU Interface (`0x2C002000`).
3. Broadcasting a Software Generated Interrupt (`SGI 0`) targeting CPUs 1, 2, and 3 via `GICD_SGIR`.
4. Issuing `dsb` (Data Synchronization Barrier) and `sev` (Send Event).
5. Polling `core_ready[1..3]` until all three redundant nodes acknowledge online readiness.

---

## 3. Pre-Flight Diagnostics (Power-On Self-Test)

Before releasing secondary cores or entering the flight loop, Core 0 executes a comprehensive Power-On Self-Test suite (`src/post.c`):

1. **CPU Register Test**: Verifies register arithmetic with alternating bit patterns (`0xAAAAAAAA`, `0x55555555`, `0x00000000`, `0xFFFFFFFF`) and walking 1s and walking 0s across all 32 bit positions.
2. **RAM March C- Test**: Executes the 6-element March C- memory test on a 512-word static memory buffer:
   - $\Uparrow (w0)$ → $\Uparrow (r0, w1)$ → $\Uparrow (r1, w0)$ → $\Downarrow (r0, w1)$ → $\Downarrow (r1, w0)$ → $\Uparrow (r0)$
   - Detects address decoding faults, stuck-at bits, transition faults, and coupling faults.
3. **CRC32 Code Integrity**: Computes an IEEE 802.3 CRC32 checksum over the entire `.text` code section (`_text_start` to `_text_end`).
4. **Voter Golden Vector Test**: Exercises unanimous agreement, individual outlier masking, and total disagreement vectors against reference outputs.

If any POST check fails, the flight computer enters an infinite `wfe` halt and does not launch.

---

## 4. Memory Layout & Spatial Partitioning

Physical memory is statically mapped in `linker.ld` to ensure strict spatial separation:

| Region | Address Range | Size | Access Rules | Description |
|---|---|---|---|---|
| **System / Text** | `0x80000000 - 0x80FFFFFF` | 16 MB | Core 0 RWX | Code, vectors, constants, `.bss`, Arbiter |
| **Zone 1 Partition**| `0x81000000 - 0x81000FFF` | 4 KB | Core 0 RW, Core 1 RW | Dedicated data buffers for Node 1 |
| **Zone 2 Partition**| `0x82000000 - 0x82000FFF` | 4 KB | Core 0 RW, Core 2 RW | Dedicated data buffers for Node 2 |
| **Zone 3 Partition**| `0x83000000 - 0x83000FFF` | 4 KB | Core 0 RW, Core 3 RW | Dedicated data buffers for Node 3 |
| **Stack Partition** | `0x80010000 - 0x80017FFF` | 32 KB | Core-private offsets | 8KB statically partitioned per core |

### ARMv7-A Short-Descriptor MMU Translation Tables (`src/mmu.c`)
The system constructs 4 translation tables (16KB each, aligned to 16KB boundaries):
- **Section Size**: 1MB per first-level descriptor.
- **Attributes**:
  - `MMU_ATTR_DEVICE` (`0x00000C02`): Non-cacheable, shared device memory for UART (`0x1C090000`), Sysregs, and GIC (`0x2C000000`).
  - `MMU_ATTR_NORMAL_RWX` (`0x00000C0E`): Normal cacheable memory for the system text and execution regions.
  - `MMU_ATTR_NORMAL_RW` (`0x00000C1E`): Execute-Never (`XN = 1`) data partitions.
  - `MMU_DESC_FAULT` (`0x00000000`): Unmapped entries that generate a translation fault upon access.

In the bare-metal demonstrator, spatial isolation is enforced via physical address mapping and software permission checks (`mmu_check_permission()`), while hardware MMU translation activation (`mmu_enable_core()`) is validated in unit test harnesses.

---

## 5. Inter-Core Communication & Synchronization

Inter-processor communication uses a double-buffered shared memory mailbox protocol guarded by ARMv7-A architectural barriers (`src/mailbox.c`, `src/amp.c`):

```text
[Core 0 (Arbiter)]                                   [Cores 1..3 (Compute Nodes)]
        │                                                         │
        ├─ 1. Write sensor input to Zone & Mailbox                │
        ├─ 2. dmb (Data Memory Barrier)                           │
        ├─ 3. Set secondary_spin_addr = secondary_core_entry      │
        ├─ 4. Increment g_cycle_counter                           │
        ├─ 5. dsb (Data Synchronization Barrier)                  │
        ├─ 6. sev (Send Event - wakes secondary cores)            │
        │                                                         │
        │                                           Woken from WFE:
        │                                                         ├─ 1. Ingest input (Mailbox/Zone)
        │                                                         ├─ 2. Compute flight control law
        │                                                         ├─ 3. Write output to Zone & Mailbox
        │                                                         ├─ 4. Report CFI checkpoints
        │                                                         ├─ 5. Verify stack canary
        │                                                         ├─ 6. Set core_done[core_id] = 1
        │                                                         ├─ 7. dmb; dsb; sev
        │                                                         └─ Return to WFE holding loop
        │
  Poll core_done[1..3] with software watchdog timeout (2,000,000 cycles)
        │
  Validate completion flags & CFI tokens
        │
  Execute 2oo3 Voter (or Degraded 2oo2 if one core timed out/latched)
```

### Double-Buffered Mailbox Structure
Each core pair communicates through a double-buffered mailbox struct:
- **Ping-Pong Buffering**: Alternates between buffer index 0 and 1 based on the frame sequence number, preventing write-after-read race conditions.
- **CRC32 Protection**: Every transaction includes an IEEE 802.3 CRC32 checksum computed over the payload. Corrupted frames are rejected (`MAILBOX_ERR_CRC`).
- **Sequence Validation**: Incoming tokens must match the expected cycle counter. Stale frames are rejected (`MAILBOX_ERR_SEQUENCE`).

---

## 6. Fault-Tolerant Voting Engine (`src/voter.c`)

The voter implements a bounded 2-out-of-3 (2oo3) majority gate utilizing median selection:

### 6.1 Median Consensus (`median3`)
Given three compute node outputs $y_1, y_2, y_3$:

$$\text{median3}(a, b, c) = \begin{cases} a & \text{if } (b \le a \le c) \lor (c \le a \le b) \\ b & \text{if } (a \le b \le c) \lor (c \le b \le a) \\ c & \text{otherwise} \end{cases}$$

Pairwise differences are evaluated against the tolerance bound ($\Delta \le 5\ \mu\text{s}$):
- $|y_1 - y_2| \le 5$, $|y_2 - y_3| \le 5$, $|y_1 - y_3| \le 5$: **Unanimous Consensus** (`VOTE_UNANIMOUS`). Output is $\text{median3}(y_1, y_2, y_3)$.
- If only one pair agrees (e.g., Nodes 1 and 2 agree, but Node 3 deviates by $> 5\ \mu\text{s}$): **Outlier Masking** (`VOTE_MAJORITY_NODE3_MASKED`). Output is $(y_1 + y_2) / 2$.
- **Chain Ambiguity Resolution**: If two overlapping pairs agree (e.g., Nodes 1 & 2 agree and Nodes 2 & 3 agree, but Nodes 1 & 3 disagree): the voter selects $\text{median3}(y_1, y_2, y_3)$ and attributes the fault to the node furthest from the closest pair.
- **Degraded 2oo2 Mode**: If one node times out or is permanently latched out, the system arbitrates between the two surviving nodes if $|y_a - y_b| \le 5\ \mu\text{s}$, sustaining flight control without tripping fail-safe.
- **Total Disagreement**: If no two nodes agree, the voter commands `FAIL_SAFE_VALUE` (`-9999 µs`) and triggers safe state.

### 6.2 Actuator Slew Rate Limiter
To prevent abrupt mechanical stress from sudden setpoint changes:

$$|\text{PWM}_k - \text{PWM}_{k-1}| \le 200\ \mu\text{s}$$

Candidate commands exceeding this bound are clamped to $\text{PWM}_{k-1} \pm 200\ \mu\text{s}$.

### 6.3 Dual-Rail Software Lockstep (`src/lockstep.c`)
To protect against transient ALU bit-flips on Core 0 during voter execution:
- Rail A evaluates the primary voter algorithm.
- Rail B independently re-calculates pairwise differences and executes an algebraic median formula (`algebraic_median3`).
- If Rail A and Rail B disagree on output value or fault classification, Core 0 immediately trips `failsafe_trigger(REASON_LOCKSTEP_FAIL)`.

---

## 7. Execution Supervision & Health Tracking

### 7.1 Control-Flow Integrity (CFI) Checkpoints (`src/supervision.c`)
Secondary cores report their execution progress through 5 monotonic token states:

$$\text{INIT} \longrightarrow \text{READ\_INPUT} \longrightarrow \text{COMPUTE} \longrightarrow \text{WRITE\_OUTPUT} \longrightarrow \text{CANARY\_CHECK} \longrightarrow \text{COMPLETE}$$

If a core skips a checkpoint, executes out of order, or halts mid-frame, its signature diverges to `0xFFFFFFFF` and it is flagged as faulted.

### 7.2 Leaky-Bucket Health Accumulator (`src/node_health.c`)
Each node maintains a persistent health tracking record:
- **Fault Accumulator**: Incremented whenever a node outputs an outlier, times out, or fails CFI.
- **Permanent Latch-Out**: If a node accumulates $N = 3$ consecutive faults, it is permanently latched offline (`is_latched = true`) and excluded from subsequent voting rounds.
- **Leaky Recovery**: A healthy node must complete $M = 100$ consecutive healthy frames to decay its fault counter by 1.

### 7.3 Software Watchdog Countdown
Core 0's dispatch loop spin-waits on `core_done[1..3]` with a bounded counter (`WATCHDOG_MAX_CYCLES = 2000000`). If a secondary core hangs (e.g., infinite loop), the countdown expires, the faulted core's bitmask is recorded (`timed_out_mask`), its output is marked invalid, and the voter transitions to degraded 2oo2 quorum if the surviving nodes agree. If multiple cores fail or surviving nodes disagree, the system safely activates fail-safe command.
