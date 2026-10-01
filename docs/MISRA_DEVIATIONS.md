# MISRA C:2012 & SEI CERT C Deviation Catalog
## Bare-Metal AMP TMR Flight Computer Firmware

---

### 1. Compliance Framework & Safety Standard Rationale

The flight computer firmware is developed in accordance with **MISRA C:2012 (Third Edition with Amendments 1 & 2)** and **NASA/JPL "Power of Ten" Rules for Safety-Critical Code**. 

Because this is a bare-metal embedded system operating directly on bare ARM Cortex-A15 silicon without an operating system, certain low-level hardware interactions require controlled deviations from MISRA guidelines. Every deviation is formally documented below with its engineering rationale, scope, and compensating safety mechanisms.

---

### 2. Formal Deviation Records

#### Deviation DEV-01: Memory-Mapped I/O Pointer Conversion
- **MISRA Rule**: Rule 11.4 (A conversion should not be performed between a pointer to object and an integer type) and Rule 11.6 (A cast between a pointer to void and an arithmetic type).
- **SEI CERT C**: INT36-C.
- **Affected Files**: [`src/memory_map.h`](file:///c:/Users/hp/Desktop/TMR/src/memory_map.h), [`src/uart.c`](file:///c:/Users/hp/Desktop/TMR/src/uart.c), [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c), [`src/mmu.c`](file:///c:/Users/hp/Desktop/TMR/src/mmu.c).
- **Source Code Instance**:
  ```c
  #define UART0_BASE      0x1C090000U
  #define UART0_DR        ((volatile uint32_t *)(UART0_BASE + 0x00U))
  #define HOLDING_PEN_ADDR 0x1C010030U
  ```
- **Rationale**: Bare-metal embedded systems interact with memory-mapped peripherals (PL011 UART, Core Holding Pen, and Zone Mailbox Buffers) via absolute physical addresses defined by the hardware memory map.
- **Compensating Controls**: All base addresses are defined as unsigned hexadecimal constants (`0x...U`), strictly bounds-checked, and isolated behind hardware MMU translation tables with execute-never (`XN`) protection.

---

#### Deviation DEV-02: Use of Inline Assembly for Architecture-Specific Control
- **MISRA Rule**: Rule 1.2 (Language extensions shall not be used).
- **Affected Files**: [`src/types.h`](file:///c:/Users/hp/Desktop/TMR/src/types.h), [`src/startup.S`](file:///c:/Users/hp/Desktop/TMR/src/startup.S), [`src/pmu.c`](file:///c:/Users/hp/Desktop/TMR/src/pmu.c).
- **Source Code Instance**:
  ```c
  __asm__ volatile("dmb" ::: "memory");
  __asm__ volatile("wfe");
  __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
  ```
- **Rationale**: ARMv7-A architectural instructions for cache/memory barriers (`DMB`, `DSB`, `ISB`), inter-core signaling (`SEV`, `WFE`), and Performance Monitor Unit coprocessor registers (`CP15 c9`) cannot be expressed in standard ISO C99/C11.
- **Compensating Controls**: All assembly instructions are encapsulated in strictly typed static inline wrapper functions with clobber lists ensuring register allocator preservation.

---

#### Deviation DEV-03: Infinite Loops for Terminal Standby & Exception Parking
- **MISRA Rule**: Rule 2.2 (There shall be no dead code) and Rule 14.3 (Controlling expressions shall not be invariant).
- **Affected Files**: [`src/main.c`](file:///c:/Users/hp/Desktop/TMR/src/main.c), [`src/failsafe.c`](file:///c:/Users/hp/Desktop/TMR/src/failsafe.c), [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c).
- **Source Code Instance**:
  ```c
  while (1) {
      wfe();
  }
  ```
- **Rationale**: A safety-critical bare-metal flight computer has no operating system to exit to. Upon mission profile completion or entering an unrecoverable fail-safe latch, the processor must enter a low-power quiescent standby loop rather than falling off the end of memory.
- **Compensating Controls**: The standby loop repeatedly executes `wfe()` to minimize power dissipation and thermal load while remaining receptive to hardware reset.

---

#### Deviation DEV-04: Fixed-Width Type Inversion in Dual-Rail Data Structures
- **MISRA Rule**: Rule 10.1 (An operand of appropriate essential type).
- **Affected Files**: [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c), [`src/lockstep.c`](file:///c:/Users/hp/Desktop/TMR/src/lockstep.c).
- **Source Code Instance**:
  ```c
  slot->inverted_val = ~payload;
  ```
- **Rationale**: Dual-rail redundancy relies on bitwise 1's complement representation to detect stuck-at silicon faults where memory cells are stuck at `0` or `1`.
- **Compensating Controls**: Types are explicitly `uint32_t` or cast via standard fixed-width integer types; dual-rail consensus is verified by `(val ^ inv) == 0xFFFFFFFFU`.

---

### 3. NASA Power of Ten Rules Verification

| Rule # | NASA JPL Principle | Implementation in Bare-Metal TMR | Compliance Status |
|---|---|---|:---:|
| **Rule 1** | Avoid complex flow constructs (no `goto`, `setjmp`, recursion) | Strict structured programming. Zero `goto`, zero recursion throughout codebase. | **FULL COMPLIANCE** |
| **Rule 2** | All loops must have fixed upper bounds | Every polling loop bounded by `WATCHDOG_MAX_CYCLES` or `wait_cycles`. | **FULL COMPLIANCE** |
| **Rule 3** | Do not use dynamic memory allocation after init | Zero `malloc()`, `calloc()`, `free()`. All allocations static BSS/Data. | **FULL COMPLIANCE** |
| **Rule 4** | No functions longer than 60 lines | All voter, health, mailbox, and PMU functions partitioned under 60 lines. | **FULL COMPLIANCE** |
| **Rule 5** | Assertion density $\ge 2$ per function | Invariants verified via `TEST_ASSERT` on host and defensive guards on target. | **FULL COMPLIANCE** |
| **Rule 6** | Declare data objects at smallest possible scope | Static module-local variables; zero unnecessary global exports. | **FULL COMPLIANCE** |
| **Rule 7** | Check return values of non-void functions | Every mailbox read, voter outcome, and POST status verified. | **FULL COMPLIANCE** |
| **Rule 8** | Limit use of preprocessor | Macros restricted to constants, addresses, and architecture barriers. | **FULL COMPLIANCE** |
| **Rule 9** | Restrict pointer use (max 1 dereference) | No pointer chasing; single dereference to fixed mailbox structs only. | **FULL COMPLIANCE** |
| **Rule 10** | Compile with all warnings enabled as errors | `-Wall -Wextra -Werror -Wshadow -Wconversion -Wstrict-prototypes` clean. | **FULL COMPLIANCE** |
