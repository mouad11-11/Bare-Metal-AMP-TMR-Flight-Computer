# Inter-Processor Communication (IPC) Protocol Specification
## Bare-Metal Asymmetric Multiprocessing (AMP) Triple Modular Redundancy (TMR)

---

### 1. Architectural Overview & Design Philosophy

The Bare-Metal AMP TMR Flight Computer operates as a 4-core Asymmetric Multiprocessing (AMP) real-time system on ARM Cortex-A15 MPCore (`vexpress-a15`). 
Because the system runs bare-metal without an underlying Real-Time Operating System (RTOS) or hardware cache coherency interconnect (CCI-400 snooping is not relied upon in bare-metal boot state), cross-core communication must be provably deterministic, race-free, and resilient against memory ordering hazards, pipeline speculation, and core lockups.

```
+-----------------------------------------------------------------------------+
|                           Core 0 (Master Arbiter)                           |
+-----------------------------------------------------------------------------+
       |                                |                               |
 [Mailbox 1 In]                   [Mailbox 2 In]                  [Mailbox 3 In]
       |                                |                               |
       v                                v                               v
+---------------+                +---------------+               +---------------+
| Core 1 Node 1 |                | Core 2 Node 2 |               | Core 3 Node 3 |
+---------------+                +---------------+               +---------------+
       |                                |                               |
 [Mailbox 1 Out]                  [Mailbox 2 Out]                 [Mailbox 3 Out]
       |                                |                               |
       +--------------------------------+-------------------------------+
                                        |
                             +--------------------+
                             |  2oo3 Voter (C0)   |
                             +--------------------+
```

---

### 2. Single-Writer IPC Discipline

To guarantee that write-after-write (WAW) hazards, atomic torn writes, and cache thrashing cannot occur, every byte in the shared memory space obeys strict single-writer ownership:

| Memory Region | Address Range | Sole Writer | Readers | Data Stored |
|---|---|---|---|---|
| **System Partition** | `0x80000000 - 0x80FFFFFF` | Core 0 | Core 0 | Arbiter code, vector table, BSS, stack 0 |
| **Zone 1 Input Mailbox** | `0x81000000 - 0x81000003` | Core 0 | Core 1 | Ingested sensor attitude rate, sequence token |
| **Zone 1 Output Mailbox** | `0x81000004 - 0x8100001F` | Core 1 | Core 0 | Actuator command, complement, CRC32, seq |
| **Zone 2 Input Mailbox** | `0x82000000 - 0x82000003` | Core 0 | Core 2 | Ingested sensor attitude rate, sequence token |
| **Zone 2 Output Mailbox** | `0x82000004 - 0x8200001F` | Core 2 | Core 0 | Actuator command, complement, CRC32, seq |
| **Zone 3 Input Mailbox** | `0x83000000 - 0x83000003` | Core 0 | Core 3 | Ingested sensor attitude rate, sequence token |
| **Zone 3 Output Mailbox** | `0x83000004 - 0x8300001F` | Core 3 | Core 0 | Actuator command, complement, CRC32, seq |
| **Synchronization Flags** | Global BSS | Respective Core | All Cores | `core_ready[i]`, `core_done[i]` |

No secondary core may write into another secondary core's partition or into the Core 0 system partition. This rule is statically enforced by code review and hardware-enforced via the MMU Short-Descriptor translation tables (see [`docs/MEMORY_PROTECTION.md`](file:///c:/Users/hp/Desktop/TMR/docs/MEMORY_PROTECTION.md)).

---

### 3. Double-Buffered CRC32 Mailbox Structure

Communication across cores utilizes a double-buffered layout (`mailbox_buffer_t`) to eliminate reader-writer contention:

```c
typedef struct {
    volatile int32_t  payload;        /* Actuator PWM or Sensor Input */
    volatile int32_t  inverted_val;   /* Bitwise complement (~payload) */
    volatile uint32_t sequence_id;    /* Monotonically increasing frame counter */
    volatile uint32_t crc32;          /* IEEE 802.3 CRC32 over payload & seq */
    volatile uint32_t status_flags;   /* Node health status token */
} mailbox_slot_t;

typedef struct {
    mailbox_slot_t slots[2];          /* Ping-pong buffers: Slot 0 & Slot 1 */
    volatile uint32_t active_index;   /* Currently published buffer index (0 or 1) */
} mailbox_channel_t;
```

#### Transmission Flow (Sender Side):
1. **Compute Candidate Slot**: `write_idx = 1 - channel->active_index`.
2. **Populate Payload**: Write `payload`, `inverted_val = ~payload`, and `sequence_id = current_frame`.
3. **Compute CRC32**: Compute 32-bit CRC across payload and sequence words using `crc32_calculate()`.
4. **Data Memory Barrier (`DMB`)**: Enforce completion of all payload and CRC writes to L1/memory before publishing.
5. **Publish Pointer**: Update `channel->active_index = write_idx`.
6. **Data Synchronization Barrier (`DSB`)**: Drain CPU store buffers.
7. **Signal Event (`SEV`)**: Send cross-core event to wake any sleeping cores waiting in low-power `WFE`.

#### Reception Flow (Receiver Side):
1. **Read Active Index**: `idx = channel->active_index`.
2. **Data Memory Barrier (`DMB`)**: Ensure index read completes before reading slot fields.
3. **Sample Slot**: Copy `payload`, `inverted_val`, `sequence_id`, `crc32`.
4. **Data Memory Barrier (`DMB`)**: Ensure data fetch completes.
5. **Multi-Rail Validation**:
   - Verify `payload ^ inverted_val == 0xFFFFFFFFU` (Dual-rail bitwise consistency).
   - Verify `sequence_id == expected_frame_id` (Stale / replay rejection).
   - Verify `crc32 == crc32_calculate(&slot)` (Transmission channel integrity).
6. If any check fails, mark channel as corrupt and increment node fault counter.

---

### 4. Memory Barriers and Ordering Semantics

On the out-of-order, superscalar ARM Cortex-A15 core, memory accesses can be reordered by the pipeline. The following memory barriers are strictly placed:

1. **`dmb()` (`mcr p15, 0, r0, c7, c10, 5` or `__asm__ volatile("dmb" ::: "memory")`)**:
   - Ensures memory accesses before the barrier are observed by other observers before memory accesses after the barrier.
   - Placed after writing mailbox data and before updating `core_done[core_id]`.
   - Placed by receiver after observing `core_done[core_id]` before reading mailbox content.

2. **`dsb()` (`mcr p15, 0, r0, c7, c10, 4` or `__asm__ volatile("dsb" ::: "memory")`)**:
   - Halts instruction execution until all pending explicit memory transactions have completed across the bus.
   - Executed immediately before issuing `sev()`.

3. **`isb()` (`mcr p15, 0, r0, c7, c5, 4` or `__asm__ volatile("isb" ::: "memory")`)**:
   - Flushes pipeline fetch stage and re-fetches instructions.
   - Executed after modifying system control registers (`SCTLR`, `DACR`, `TTBR0`, `VBAR`).

---

### 5. Deadlock Prevention and Bounded Wait Loops

No synchronization loop in the flight software is unbounded:
- **Secondary Nodes (Cores 1, 2, 3)**:
  Wait in a low-power `wfe()` loop polling `mailbox_has_new_input()`. Each node maintains an internal watchdog timeout counter (`5,000,000` cycles). If Core 0 stalls, secondary nodes transition to fail-safe quiescence.
- **Master Arbiter (Core 0)**:
  Waits for all three nodes to signal completion (`core_done[1..3] == frame_id`). The polling loop is guarded by `WATCHDOG_MAX_CYCLES` (`500,000` iterations). If any core fails to respond within the deadline budget, Core 0 trips the watchdog mask and immediately commands the fail-safe actuator state (`-9999` µs).

---

### 6. Cache and Translation Table Attributes

To guarantee deterministic, zero-latency shared memory operations without requiring complex software cache line flush/invalidate routines:
- All shared mailbox regions (`0x81000000 - 0x83FFFFFF`) are configured in the ARMv7-A Short-Descriptor page tables as **Device / Strongly-Ordered memory** (`Section descriptor AP=0b011, TEX=0b000, C=0, B=0, XN=1`).
- All reads and writes hit the interconnect directly without cache line allocation, guaranteeing immediate coherency across all cores.
