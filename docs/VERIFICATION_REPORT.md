# Host Unit Test & Verification Report

## 1. Executive Summary

This report documents the verification results of the **Bare-Metal AMP TMR Flight Computer** test suite. The test framework exercises all flight-critical modules using GCC-instrumented native C test runners on the host environment:

- **11 Native C Test Suites**: Testing voter consensus, node health tracking, fail-safe logic, execution supervision, pre-flight diagnostics (POST), saturating math, spatial memory isolation, dual-rail lockstep, mailbox IPC, performance monitoring, and design diversity.
- **246,122 Total Assertions Executed**: **0 Failed** (100% Pass Rate).
- **Voter Stress Testing**: Includes an exhaustive 15,625-point parameter grid exploring all permutations in the operational boundary around neutral setpoint ($1500 \pm 12\,\mu\text{s}$).

---

## 2. Test Suite Breakdown

| Suite # | Test Binary | Target Modules Tested | Assertions | Result | Key Capabilities Verified |
|---|---|---|:---:|:---:|---|
| **01** | `test_voter` | `src/voter.c`, `src/node_health.c` | **236,819** | **PASS** | 2oo3 median consensus, permutation symmetry, 15,625-point grid, dual-threshold transient masking vs hard latching, degraded 2oo2 |
| **02** | `test_diversity` | `src/flight_control.c` | **9,025** | **PASS** | Primary vs Q15 diverse control laws across $[-1500, +1500]$ ddeg/s, triplicate sensor cross-check |
| **03** | `test_safe_math` | `src/safe_math.h` | **60** | **PASS** | Saturating 32-bit addition, subtraction, multiplication, division-by-zero, inverted bounds clamping |
| **04** | `test_mmu` | `src/mmu.c` | **39** | **PASS** | Level-1 Short-Descriptor section setup, 1MB boundaries, Core 0..3 spatial isolation permissions |
| **05** | `test_failsafe` | `src/failsafe.c` | **37** | **PASS** | State machine latching, reason codes, exception context frame logging, command status strings |
| **06** | `test_supervision`| `src/supervision.c` | **26** | **PASS** | Monotonic 5-state CFI checkpoint transitions, per-frame reset, sequence divergence detection, missed deadline limits |
| **07** | `test_lockstep` | `src/lockstep.c` | **27** | **PASS** | Independent dual-rail lockstep checking, algebraic median comparison, voter self-monitoring, corrupted status detection |
| **08** | `test_mailbox` | `src/mailbox.c` | **35** | **PASS** | 3-slot lock-free tri-buffering, writer-laps-reader overrun immunity, IEEE 802.3 CRC32 verification, frame sequence token check |
| **09** | `test_stack_monitor`| `src/stack_monitor.c` | **21** | **PASS** | 4-word `0xDEADBEEF` canary detection, `0xA5A5A5A5` watermarking, high-water mark computation |
| **10** | `test_post` | `src/post.c` | **18** | **PASS** | CPU register walking 1s/0s, 6-element March C- RAM buffer test, CRC32 code segment integrity |
| **11** | `test_pmu` | `src/pmu.c` | **15** | **PASS** | Cortex-A15 PMU cycle counter query, frame segment timing recording, WCET margin reporting |
| **Total** | | | **246,122** | **0 FAIL** | **All 11 test suites passing** |

---

## 3. Structural Coverage Metrics (GCC `gcov`)

Structural line and branch coverage was measured using GCC `gcov -b -c` on native test runs:

| Source Module | Target File | Lines Executed | Line Coverage | Branches Executed | Branch Coverage | Taken At Least Once |
|---|---|:---:|:---:|:---:|:---:|:---:|
| **Voter & Consensus** | `src/voter.c` | 144 / 167 | 86.23% | 105 / 105 | 100.00% | 90.48% |
| **Node Health Latch** | `src/node_health.c` | 37 / 40 | 92.50% | 32 / 32 | 100.00% | 81.25% |
| **Fail-Safe Subsystem**| `src/failsafe.c` | 72 / 97 | 74.23% | 27 / 38 | 71.05% | 63.16% |
| **Frame Supervision** | `src/supervision.c` | 60 / 60 | 100.00% | 50 / 50 | 100.00% | 86.00% |
| **Power-On Self-Test** | `src/post.c` | 89 / 109 | 81.65% | 87 / 87 | 100.00% | 71.26% |
| **Safe Math Library** | `src/safe_math.h` | 48 / 48 | 100.00% | 30 / 30 | 100.00% | 96.67% |
| **MMU & Isolation** | `src/mmu.c` | 47 / 52 | 90.38% | 32 / 32 | 100.00% | 75.00% |
| **Dual-Rail Lockstep**| `src/lockstep.c` | 69 / 70 | 98.57% | 38 / 38 | 100.00% | 81.58% |
| **CRC32 Mailbox** | `src/mailbox.c` | 87 / 89 | 97.75% | 44 / 44 | 100.00% | 90.91% |
| **PMU & Timing** | `src/pmu.c` | 40 / 40 | 100.00% | 16 / 16 | 100.00% | 100.00% |
| **Stack Monitor** | `src/stack_monitor.c` | 60 / 61 | 98.36% | 36 / 36 | 100.00% | 97.22% |
| **Flight Control & Div**| `src/flight_control.c` | 66 / 81 | 81.48% | 84 / 118 | 71.19% | 47.46% |

> **Coverage Note**: Unexecuted lines in `post.c`, `mmu.c`, and `failsafe.c` correspond strictly to inline ARM assembly instructions (`mcr`, `mrc`, `cps`, `isb`, `dsb`) wrapped in `#if defined(__arm__)`, which cannot execute natively on x86_64 host processors and are verified during bare-metal QEMU execution.

---

## 4. Modified Condition / Decision Coverage (MC/DC) for 2oo3 Voter

The voter decision tree in `src/voter.c` evaluates three pairwise differences against tolerance bound $T = 5\ \mu\text{s}$:
- Condition $A$: $(d_{12} \le 5)$
- Condition $B$: $(d_{23} \le 5)$
- Condition $C$: $(d_{13} \le 5)$

```text
Decision Formulae:
  D_unanimous = A and B and C
  D_chain     = (A and B and not C) or (A and not B and C) or (not A and B and C)
  D_outlier   = (A and not B and not C) or (not A and B and not C) or (not A and not B and C)
  D_failsafe  = not A and not B and not C
```

| Vector ID | Condition $A$ ($d_{12} \le 5$) | Condition $B$ ($d_{23} \le 5$) | Condition $C$ ($d_{13} \le 5$) | Decision Outcome | Independent Factor Proved |
|---|:---:|:---:|:---:|:---:|---|
| **VEC-01** (Nominal) | **TRUE** | **TRUE** | **TRUE** | **TRUE** (Unanimous) | Baseline True Vector |
| **VEC-02** (Outlier 3) | **TRUE** | **FALSE** | **FALSE** | **FALSE** | Shows $B$ and $C$ affect outcome |
| **VEC-03** (Chain 1-2-3)| **TRUE** | **TRUE** | **FALSE** | **FALSE** | **Proves Condition $C$ independently** (VEC-01 vs VEC-03) |
| **VEC-04** (Chain 2-1-3)| **TRUE** | **FALSE** | **TRUE** | **FALSE** | **Proves Condition $B$ independently** (VEC-01 vs VEC-04) |
| **VEC-05** (Chain 1-3-2)| **FALSE** | **TRUE** | **TRUE** | **FALSE** | **Proves Condition $A$ independently** (VEC-01 vs VEC-05) |

---

## 5. Automated Verification Commands

To reproduce all test results:

```bash
# Execute all 11 host unit test suites with coverage
make test-host

# Execute automated 113-vector fault-injection campaign
python tests/fi/run_fault_campaign.py

# Verify safety requirements traceability matrix
python tools/check_traceability.py

# Verify bare-metal QEMU flight telemetry assertions
python tests/check_uart_output.py
```
