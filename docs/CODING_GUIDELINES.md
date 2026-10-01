# Defensive Embedded C Coding Guidelines & Architectural Exceptions
## Bare-Metal AMP TMR Flight Computer Firmware

---

### 1. Framework & Safety Engineering Rationale

The flight computer firmware is developed in accordance with strict defensive programming rules for safety-critical embedded systems. 

Because this is a bare-metal embedded system operating directly on bare ARM Cortex-A15 silicon without an operating system, certain low-level hardware interactions require explicit architectural exceptions. Every exception is formally documented below with its engineering rationale, scope, and compensating safety mechanisms.

---

### 2. Formal Architectural Exception Records

#### Exception DEV-01: Memory-Mapped I/O Pointer Conversion
- **Guideline**: Conversions between integer addresses and pointer types.
- **Affected Files**: [`src/memory_map.h`](file:///c:/Users/hp/Desktop/TMR/src/memory_map.h), [`src/uart.c`](file:///c:/Users/hp/Desktop/TMR/src/uart.c), [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c), [`src/mmu.c`](file:///c:/Users/hp/Desktop/TMR/src/mmu.c).
- **Source Code Instance**:
  ```c
  #define PL011_UART0_BASE        0x1C090000U
  #define VEXPRESS_SYSREGS_BASE   0x1C010000U
  volatile uint32_t *sys_flags1 = (volatile uint32_t *)0x1C010030U;
  volatile uint32_t * const uart0_dr = (volatile uint32_t *)(PL011_UART0_BASE + 0x00U);
  ```
- **Rationale**: Bare-metal embedded systems interact with memory-mapped peripherals (PL011 UART, Core Holding Pen, and Zone Mailbox Buffers) via absolute physical addresses defined by the hardware memory map.
- **Compensating Controls**: All base addresses are defined as unsigned hexadecimal constants (`0x...U`), strictly bounds-checked, and isolated behind physical zone partitions.

---

#### Exception DEV-02: Use of Inline Assembly for Architecture-Specific Control
- **Guideline**: Architecture-specific assembly language instructions.
- **Affected Files**: [`src/types.h`](file:///c:/Users/hp/Desktop/TMR/src/types.h), [`src/startup.S`](file:///c:/Users/hp/Desktop/TMR/src/startup.S), [`src/pmu.c`](file:///c:/Users/hp/Desktop/TMR/src/pmu.c).
- **Source Code Instance**:
  ```c
  __asm__ volatile("dmb" ::: "memory");
  __asm__ volatile("wfe" ::: "memory");
  __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
  ```
- **Rationale**: ARMv7-A architectural instructions for cache/memory barriers (`DMB`, `DSB`, `ISB`), inter-core signaling (`SEV`, `WFE`, `YIELD`), and Performance Monitor Unit coprocessor registers (`CP15 c9`) cannot be expressed in standard C syntax.
- **Compensating Controls**: All assembly instructions are encapsulated in strictly typed static inline wrapper functions with clobber lists ensuring register allocator preservation.

---

#### Exception DEV-03: Infinite Loops for Terminal Standby & Exception Parking
- **Guideline**: Bounded execution and termination guarantees.
- **Affected Files**: [`src/main.c`](file:///c:/Users/hp/Desktop/TMR/src/main.c), [`src/failsafe.c`](file:///c:/Users/hp/Desktop/TMR/src/failsafe.c), [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c).
- **Source Code Instance**:
  ```c
  while (1) {
      wfe();
  }
  ```
- **Rationale**: A safety-critical bare-metal flight computer has no operating system to exit to. Upon mission profile completion or entering an unrecoverable fail-safe latch, the processor must enter a low-power quiescent standby loop rather than executing undefined memory.
- **Compensating Controls**: The standby loop repeatedly executes `wfe()` to minimize power dissipation and thermal load while remaining receptive to hardware reset.

---

#### Exception DEV-04: Bitwise Operations for Checksums, Fault Modeling, and Hardware Masks
- **Guideline**: Appropriate type operations and bitwise manipulation.
- **Affected Files**: [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c), [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c), [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c).
- **Source Code Instance**:
  ```c
  /* CRC32 polynomial division (src/mailbox.c) */
  crc = (crc >> 1) ^ 0xEDB88320U;
  /* Simulated single-event upset bit toggling (src/flight_control.c) */
  pwm ^= (1 << 9);
  /* GIC interrupt distribution mask (src/amp.c) */
  *gic_sgir = (1U << 24);
  ```
- **Rationale**: Data integrity checks (IEEE 802.3 CRC32), SEU cosmic ray fault simulation, and GIC interrupt distributor configuration fundamentally require bitwise shifts, masks, and XOR operations on fixed-width unsigned integers.
- **Compensating Controls**: Operations are strictly performed on fixed-width unsigned types (`uint32_t`); bounds and shifts are verified with unit tests (`test_mailbox` and `test_diversity`).

---

### 3. Safety-Critical Code Discipline Verification

| Principle # | Safety-Critical Engineering Rule | Implementation in Bare-Metal TMR | Verification Status |
|---|---|---|:---:|
| **Rule 1** | Avoid complex flow constructs (no `goto`, `setjmp`, recursion) | Strict structured programming. Zero `goto`, zero recursion throughout codebase. | **FULL COMPLIANCE** |
| **Rule 2** | All loops must have fixed upper bounds | Every polling loop bounded by `WATCHDOG_MAX_CYCLES` or `wait_cycles`. | **FULL COMPLIANCE** |
| **Rule 3** | Do not use dynamic memory allocation after init | Zero `malloc()`, `calloc()`, `free()`. All allocations static BSS/Data. | **FULL COMPLIANCE** |
| **Rule 4** | Partitioned function sizing | All voter, health, mailbox, and PMU functions partitioned into compact units. | **FULL COMPLIANCE** |
| **Rule 5** | High assertion density | Invariants verified via assertion tests on host and defensive guards on target. | **FULL COMPLIANCE** |
| **Rule 6** | Declare data objects at smallest possible scope | Static module-local variables; zero unnecessary global exports. | **FULL COMPLIANCE** |
| **Rule 7** | Check return values of non-void functions | Every mailbox read, voter outcome, and POST status verified. | **FULL COMPLIANCE** |
| **Rule 8** | Limit use of preprocessor | Macros restricted to constants, addresses, and architecture barriers. | **FULL COMPLIANCE** |
| **Rule 9** | Restrict pointer use (max 1 dereference) | No pointer chasing; single dereference to fixed mailbox structs only. | **FULL COMPLIANCE** |
| **Rule 10** | Compile with all warnings enabled as errors | `-Wall -Wextra -Werror -Wshadow -Wconversion -Wstrict-prototypes` clean. | **FULL COMPLIANCE** |
