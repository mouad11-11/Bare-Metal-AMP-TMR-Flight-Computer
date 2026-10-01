# Discrepancies Between Prompt Description and Baseline Codebase

This document records the exact discrepancies identified between the agent prompt specifications and the actual baseline codebase (`mouad11-11/Bare-Metal-AMP-TMR-Flight-Computer`) during the initial audit phase (Step P1.0).

---

## 1. Voter Algorithm (Prompt §1.4 vs Code)
- **Prompt Description**: States that unanimous consensus selects the average `(y1+y2+y3)/3` in legacy mode, transitioning to median selection in Step P1.6.
- **Actual Code (`src/voter.c:27`)**: Currently implements `(y1 + y2 + y3) / 3`.
- **Finding**: Aligns with legacy description. Step P1.6 will transition this to median selection.

## 2. Watchdog Timeout Cycles (Prompt §1.2 vs Code)
- **Prompt Description**: States watchdog counter polls with a watchdog counter.
- **Actual Code (`src/voter.h:8`, `src/amp.c:125`)**: Defines `#define WATCHDOG_MAX_CYCLES 500000U` (500,000 cycles).
- **Documentation Discrepancy (`DOCUMENTATION.md:291`)**: `DOCUMENTATION.md` previously cited `AMP_TIMEOUT_CYCLES 5000000` (5,000,000 cycles). The code currently executes with 500,000 cycles.

## 3. Spatial Memory Partition Sizes (`linker.ld` vs Documentation)
- **Prompt Description / Documentation**: Mentions 1MB zones (e.g. `ZONE1_RAM : ORIGIN = 0x81000000, LENGTH = 1M`).
- **Actual Code (`linker.ld:9-11`)**:
  ```ld
  ZONE1 (rw)  : ORIGIN = 0x81000000, LENGTH = 4K
  ZONE2 (rw)  : ORIGIN = 0x82000000, LENGTH = 4K
  ZONE3 (rw)  : ORIGIN = 0x83000000, LENGTH = 4K
  ```
- **Finding**: In the linker script, each zone is allocated 4KB (`0x1000` bytes). Only 8 bytes per zone are currently utilized (offset `+0x0` for input, `+0x4` for output).

## 4. Fault Injector Compilation Path (`src/flight_control.c`)
- **Prompt Description**: Requires that fault injector hooks exist only in `TEST_BUILD` and are eliminated from `FLIGHT_BUILD`.
- **Actual Code (`src/flight_control.c:3, 19-43`)**: `g_fault_mode` and the fault injection branching logic are currently compiled unconditionally in all builds.

## 5. Fail-Safe Representation (`src/voter.h`, `src/main.c`)
- **Prompt Description**: Requires a `command_t` struct `{ int32_t pwm_us; uint32_t status; uint32_t frame_id; uint32_t crc; }` with separate status and reason codes.
- **Actual Code**: Only returns an in-band integer `res.final_pwm = FAIL_SAFE_VALUE (-9999)` with enum `vote_status_t`. The actuator dispatch simply takes `final_pwm`.

## 6. Concurrent UART Access During Boot (`src/amp.c:66`)
- **Prompt Description**: States Core 0 owns UART (`0x1C090000`).
- **Actual Code**: Cores 1, 2, and 3 invoke `secondary_core_boot_notify()` which directly calls `uart_printf("[SYNC] Secondary Core Woke Up: ...")`. Because this occurs concurrently without locking, boot UART characters interleave in multi-threaded execution (observed in `docs/baseline_output.txt`).

## 7. Exception Vector Completeness (`src/startup.S:10-20, 120-184`)
- **Prompt Description**: Handlers must follow Tree 4 (capture context into per-core fault record, fail-silent or SAFE, dedicated stacks).
- **Actual Code**: Handlers write ASCII strings directly to UART (`[UNDEF]`, `[PABT]`, `[DABT]`) and spin infinitely (`b .`). `_svc_handler` spins without output. `_irq_handler` and `_fiq_handler` return blindly (`subs pc, lr, #4`).
