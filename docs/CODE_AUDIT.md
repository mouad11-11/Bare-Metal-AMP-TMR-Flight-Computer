# Code Audit Report: Bare-Metal AMP TMR Flight Computer

**Audit Date:** 2026-09-30  
**Target Architecture:** ARM Cortex-A15 MPCore (`vexpress-a15`)  
**Scope:** Complete baseline repository audit prior to hardening modifications (Step P1.0).

---

## 1. Memory Management & Cache Architecture
- **MMU Status (`SCTLR.M`)**: The Memory Management Unit is currently **disabled**. Neither `startup.S` nor any C source file enables `SCTLR.M` (bit 0 of CP15 c1). All memory addresses operate as physical addresses.
- **Cache Status (`SCTLR.C`, `SCTLR.I`)**: Data caches and Instruction caches are **disabled** by default at reset and never enabled.
- **Memory Attributes of Zones**: With the MMU disabled on ARMv7-A, all memory accesses behave with default architectural fallback attributes: Strongly-ordered / Device or Normal Non-cacheable depending on implementation. Thus, writes to Zone 1 (`0x81000000`), Zone 2 (`0x82000000`), and Zone 3 (`0x83000000`) are currently serialized directly to memory, but lack hardware page protection or access permission enforcement.

---

## 2. Multiprocessing Boot & Mailbox Synchronization
- **Secondary Core Wakeup (`src/amp.c:13-60`, `src/startup.S:41-68`)**:
  - Secondary cores (Cores 1, 2, 3) start in the QEMU Versatile Express holding pen (`0x1C010030` and `0x10000030`).
  - Core 0 writes `_start` into these holding pen registers (`src/amp.c:23-24`), enables the GIC Distributor (`0x2C001000`), enables SGIs 0-15, enables GIC CPU Interface 0 (`0x2C002000`), and transmits SGI 0 to Cores 1-3 via `GICD_SGIR` accompanied by `DSB` and `SEV` (`src/amp.c:51-59`).
  - Secondary cores jump to `_start`, evaluate MPIDR (`src/startup.S:41-54`), compute private stack pointers (`src/startup.S:60-63`), notify readiness via `secondary_core_boot_notify()` (`src/startup.S:91`), and enter `secondary_wait_loop` (`src/startup.S:98`).
- **Mailbox Operation & Memory Ordering (`src/amp.c:104-144`, `src/startup.S:98-118`)**:
  - Dispatch: Core 0 writes `secondary_spin_addr = secondary_core_entry` after clearing `core_done[1..3]` (`src/amp.c:107-115`), bracketed by `dmb()`, followed by `dsb()` and `sev()`.
  - Reception: Cores 1-3 wait in `wfe`, poll `secondary_spin_addr`, and jump to `blx r2` (`src/startup.S:98-105`).
  - Completion: Cores write output, issue `dmb()`, write `core_done[core_id] = 1`, issue `dmb()`, and call `sev()` (`src/amp.c:98-101`).
  - Handshake: Secondary cores spin in `wait_mailbox_clear` until Core 0 sets `secondary_spin_addr = NULL` (`src/startup.S:111-115`).
  - **Weakness Identified**: In `src/startup.S:111-115`, `wait_mailbox_clear` is an unbounded loop without timeout. If Core 0 stalls or hangs, secondary cores lock up in that loop indefinitely.

---

## 3. Watchdog Implementation
- **Implementation Mechanism (`src/amp.c:125-142`)**:
  - The watchdog is implemented strictly as a **software loop counter**:
    ```c
    uint32_t timeout = WATCHDOG_MAX_CYCLES; // 500000U
    while ((!core_done[1] || !core_done[2] || !core_done[3]) && --timeout > 0) {
        __asm__ volatile("nop");
    }
    ```
  - **Peripheral Status**: No real hardware watchdog peripheral (e.g. ARM SP805 at `0x1C0F0000` or SP804 timer at `0x1C110000`) is currently initialized or ticked.
  - If the timeout expires (`timeout == 0`), Core 0 builds `timed_out_mask` identifying missing nodes and returns `false`, causing `execute_flight_frame()` to trigger `FAIL_SAFE_VALUE`.

---

## 4. Exception Vectors & Handlers
- **Vector Table (`src/startup.S:12-20`)**:
  - `0x00` Reset: Branches to `_start`.
  - `0x04` Undefined Instruction: Branches to `_undef_handler`.
  - `0x08` Supervisor Call (SVC): Branches to `_svc_handler`.
  - `0x0C` Prefetch Abort: Branches to `_pabt_handler`.
  - `0x10` Data Abort: Branches to `_dabt_handler`.
  - `0x14` Reserved: `b .` (infinite loop).
  - `0x18` IRQ: Branches to `_irq_handler`.
  - `0x1C` FIQ: Branches to `_fiq_handler`.
- **Handler Actions**:
  - `_undef_handler` (`src/startup.S:120-138`): Writes ASCII string `[UNDEF]\n` to PL011 UART (`0x1C090000`) directly by hardcoded MMIO stores, then spins in `b .`.
  - `_pabt_handler` (`src/startup.S:143-159`): Writes `[PABT]\n` directly to UART and spins in `b .`.
  - `_dabt_handler` (`src/startup.S:161-177`): Writes `[DABT]\n` directly to UART and spins in `b .`.
  - `_svc_handler` (`src/startup.S:140-141`): Spins in `b .`.
  - `_irq_handler` / `_fiq_handler` (`src/startup.S:179-183`): Executes `subs pc, lr, #4` to return immediately without clearing pending interrupt lines in the GIC.
- **Weaknesses Identified**:
  - Handlers do not save or log context (`DFSR`, `IFSR`, `DFAR`, `IFAR`, `SPSR`, `LR`, Core ID).
  - Handlers do not differentiate between Core 0 and node cores.
  - Handlers do not transition the system to `FAILSAFE` state (Tree 6) or fail-silent (Tree 4).
  - No per-mode exception stacks are configured; handlers execute on whatever stack is active or in uninitialized banked SP registers.

---

## 5. Fail-Safe Output & Propagation Path
- **Definition (`src/voter.h:6`)**:
  - Defined as `#define FAIL_SAFE_VALUE (-9999)`.
- **Propagation Path (`src/main.c:48, 79-82`, `src/voter.c:61`)**:
  - In `src/voter.c:61`, if no two cores agree within tolerance, `res.final_pwm = FAIL_SAFE_VALUE`.
  - In `src/main.c:48`, if `amp_dispatch_and_wait()` reports a timeout, `res.final_pwm = FAIL_SAFE_VALUE`.
  - Actuator path: `print_flight_frame_result()` checks `res.final_pwm == FAIL_SAFE_VALUE` and logs fail-safe activation.
  - **Weakness Identified**: Fail-safe is signaled solely as an in-band magic number (`-9999`) in the `final_pwm` output integer. There is no independent status field (`command_t`), no reason code enum, and no latched fail-safe state machine.

---

## 6. Fault Injection Framework
- **Implementation (`src/flight_control.c:3, 18-43`, `src/flight_control.h:6-18`)**:
  - Global variable `volatile fault_injection_t g_fault_mode` controls the injected scenario.
  - In `flight_control_compute()`, an `if/else` chain checks `g_fault_mode` and injects bit flips (`pwm ^= (1 << 9)`), noise, disagreement, or an infinite loop (`while(1) nop`).
- **Production Path Risk**:
  - `g_fault_mode` and the fault injection branching code are currently compiled directly into all builds. There is no separation between `TEST_BUILD` and `FLIGHT_BUILD`.

---

## 7. Arithmetic, Types, and Undefined Behavior Review
- **Floating-Point**:
  - VFP/NEON is enabled in `startup.S:31-39` to permit GCC vector optimizations.
  - However, no floating-point arithmetic (`float`/`double`) is used in the control or voter paths. All control logic uses 32-bit fixed-point (`(error * 3) / 10`).
- **Division**:
  - Division is used in `flight_control.c:15` (`/ 10`), `voter.c:27` (`/ 3`), `voter.c:31, 35, 39, 47, 50, 53` (`/ 2`), and `uart.c:57, 125, 147` (`/ 10`).
  - Divisors are all hardcoded positive non-zero constants; division-by-zero is physically impossible in these locations.
- **Dynamic Memory**:
  - Zero dynamic memory allocation (`malloc`, `free`, `sbrk`). All variables and buffers are statically allocated in `.data`, `.bss`, or on the stack.
- **Recursion**:
  - None. Call trees are strictly acyclic and shallow (depth <= 4).
- **Unbounded Loops**:
  - `startup.S:111-115`: `wait_mailbox_clear` spins until `secondary_spin_addr == 0` without timeout.
  - `flight_control.c:40-42`: `while(1) nop;` for `FAULT_HANG_NODE2`.
  - `uart.c:34-36`: `while (UARTFR & UARTFR_TXFF)` spins waiting for UART transmit FIFO without timeout.
  - `main.c:141-143, 173-175`: `while (1) { wfe(); }` standby loops (intended).
- **Signed Overflow & Boundary Behavior**:
  - `safe_diff()` in `src/voter.c:3-8` correctly uses `int64_t` difference casting and clamps to `0x7FFFFFFF`, mitigating 32-bit subtraction overflow.
  - `uart.c:66, 119` correctly negates negative values using `0U - (uint32_t)val` to prevent undefined behavior on `INT32_MIN`.
  - Plausibility clamping in `flight_control.c:46-47` clamps `pwm` to `[1000, 2000]`, but when `g_fault_mode` injects bit flips or offsets, clamping occurs *after* fault injection, which in some tests clamps values back into range.
