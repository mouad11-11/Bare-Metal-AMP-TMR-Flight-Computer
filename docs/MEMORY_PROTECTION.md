# Spatial Memory Protection & MMU Partitioning Specification
## ARMv7-A Short-Descriptor Translation & Core Isolation Architecture

---

### 1. Spatial Partitioning Principles

Under DO-178C Level A and ECSS-E-ST-40C standards, software components with different criticality levels or independent redundant channels must be physically isolated to prevent fault propagation (spatial freedom from interference).

In the Bare-Metal AMP TMR Flight Computer, spatial memory isolation is achieved using ARMv7-A Virtual Memory System Architecture (VMSA) with Short-Descriptor 1 MB section translation tables. Each of the four Cortex-A15 processor cores operates with its own distinct translation table pointed to by `TTBR0` (Translation Table Base Register 0).

```
   Physical Memory (128 MB RAM: 0x80000000 - 0x88000000)
   +-------------------------------------------------------------+
   | System & Boot Partition (0x80000000 - 0x80FFFFFF)           |
   | Core 0: Read/Write/Execute                                  |
   | Cores 1-3: Read-Only Code (0x80000000-0x800FFFFF), Rest NO ACCESS
   +-------------------------------------------------------------+
   | Zone 1 Partition (0x81000000 - 0x81FFFFFF)                  |
   | Core 1: Read/Write (XN)                                     |
   | Core 0: Read-Only (XN)                                      |
   | Cores 2, 3: NO ACCESS (Fault on touch)                      |
   +-------------------------------------------------------------+
   | Zone 2 Partition (0x82000000 - 0x82FFFFFF)                  |
   | Core 2: Read/Write (XN)                                     |
   | Core 0: Read-Only (XN)                                      |
   | Cores 1, 3: NO ACCESS (Fault on touch)                      |
   +-------------------------------------------------------------+
   | Zone 3 Partition (0x83000000 - 0x83FFFFFF)                  |
   | Core 3: Read/Write (XN)                                     |
   | Core 0: Read-Only (XN)                                      |
   | Cores 1, 2: NO ACCESS (Fault on touch)                      |
   +-------------------------------------------------------------+
   | Peripheral Space (PL011 UART: 0x1C090000, Pen: 0x1C010030)  |
   | Core 0: Read/Write (Device, XN)                             |
   | Cores 1-3: NO ACCESS                                        |
   +-------------------------------------------------------------+
```

---

### 2. Comprehensive Memory Region Access Control Matrix

| Region Base | End Address | Size | Memory Type | XN | Core 0 Perm | Core 1 Perm | Core 2 Perm | Core 3 Perm | Description / Contents |
|---|---|---|---|:---:|:---:|:---:|:---:|:---:|---|
| `0x1C010000` | `0x1C01FFFF` | 64 KB | Device | 1 | **RW** | **NO ACCESS** | **NO ACCESS** | **NO ACCESS** | System Holding Pen (`0x1C010030`) |
| `0x1C090000` | `0x1C09FFFF` | 64 KB | Device | 1 | **RW** | **NO ACCESS** | **NO ACCESS** | **NO ACCESS** | PrimeCell PL011 UART Console |
| `0x80000000` | `0x800FFFFF` | 1 MB | Normal (WT) | 0 | **RW / Exec** | **RO / Exec** | **RO / Exec** | **RO / Exec** | Vector table, `.text`, `.rodata` |
| `0x80100000` | `0x80FFFFFF` | 15 MB | Normal (WB) | 1 | **RW / XN** | **NO ACCESS** | **NO ACCESS** | **NO ACCESS** | Core 0 private BSS, stacks, state |
| `0x81000000` | `0x81FFFFFF` | 16 MB | Strongly-Ord | 1 | **RO / XN** | **RW / XN** | **NO ACCESS** | **NO ACCESS** | Zone 1 Input/Output Mailbox RAM |
| `0x82000000` | `0x82FFFFFF` | 16 MB | Strongly-Ord | 1 | **RO / XN** | **NO ACCESS** | **RW / XN** | **NO ACCESS** | Zone 2 Input/Output Mailbox RAM |
| `0x83000000` | `0x83FFFFFF` | 16 MB | Strongly-Ord | 1 | **RO / XN** | **NO ACCESS** | **NO ACCESS** | **RW / XN** | Zone 3 Input/Output Mailbox RAM |
| `0x84000000` | `0x87FFFFFF` | 64 MB | Unmapped | 1 | **FAULT** | **FAULT** | **FAULT** | **FAULT** | Unallocated RAM (Guard band) |

---

### 3. Translation Table Structure & Hardware Bitfields

ARMv7-A Short-Descriptor format uses 4096 entries of 4 bytes each (16 KB per table aligned to 16 KB boundary):

```
 31                           20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+-------------------------------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|      Section Base Address     |NS| 0|nG| S|AP2|  TEX  |  AP[1:0] | 1|  Domain |XN| C| B| 1| 0|
+-------------------------------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

#### Key Bitfield Settings:
- **Bits [1:0] = 0b10**: 1 MB Section Descriptor.
- **Bit [4] = XN (Execute-Never)**: Set to `1` on all data, heap, stack, mailbox, and peripheral sections to prevent code injection attacks.
- **Bits [8:5] = Domain 0**: Client mode controlled by `DACR` (`0x55555555`).
- **Bits [15, 11:10] = AP[2:0] (Access Permissions)**:
  - `0b000` / `0b000`: No Access (Generates Domain/Permission Fault).
  - `0b010`: Full Read/Write Access in Privileged Mode.
  - `0b110`: Read-Only Access in Privileged Mode.
- **Bits [14:12, 3:2] = TEX[2:0], C, B (Memory Type & Cacheability)**:
  - `TEX=0b000, C=0, B=0`: Strongly-Ordered / Device memory (used for mailboxes and PL011 UART).
  - `TEX=0b001, C=1, B=1`: Normal Memory, Outer and Inner Write-Back, Write-Allocate.

---

### 4. Stack Boundary Canaries & Guard Page Defense

To prevent stack-smashing and stack-overflow cross-contamination between cores:
1. **Canary Pattern**: Four 32-bit words (`0xDEADBEEF`) placed at the base of each core's dedicated 8 KB stack region.
2. **Watermarking**: Entire stack space pre-filled at boot with `0xA5A5A5A5` to track high-water-mark peak consumption.
3. **Guard Boundary**: The 4 KB immediately preceding each stack allocation is unmapped in the page table, raising a hardware Data Abort on stack exhaustion.

---

### 5. Violation Handling & Fault Isolation

If any compute node attempts an unauthorized memory transaction:
1. The ARM Cortex-A15 Memory Management Unit asserts a **Data Abort** (`0x00000010`) or **Prefetch Abort** (`0x0000000C`).
2. Execution vectors to `vector_data_abort` in [`src/startup.S`](file:///c:/Users/hp/Desktop/TMR/src/startup.S).
3. The hardware registers `DFSR` (Data Fault Status Register), `DFAR` (Data Fault Address Register), `SPSR`, and `LR` are captured into a non-volatile per-core fault record.
4. **Node Core Action**: Immediately transitions to fail-silent mode (disables interrupts, stops publishing heartbeats, parks in low-power `WFE`).
5. **Core 0 Action**: Sees node timeout in next supervision cycle, trips Tree 2 health counter, and latches the node offline without system interruption.
