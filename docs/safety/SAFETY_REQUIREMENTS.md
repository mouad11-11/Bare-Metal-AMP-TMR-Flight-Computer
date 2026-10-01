# Software Safety Requirements Specification (SRS)
## Bare-Metal AMP TMR Flight Computer System
### Traceable to System Hazard Analysis (HAZARD_ANALYSIS.md)

---

### 1. Requirements Allocation & Format

Each safety requirement adheres to the formal EARS (Easy Approach to Requirements Syntax) structure and carries an explicit allocation to software architecture components and verification test vectors.

---

### 2. High-Level Safety Requirements Catalog

#### SR-001: 2-out-of-3 Bounded Consensus Voting
- **Parent Hazard**: `HZ-01` | **Criticality**: Critical
- **Statement**: When all three compute nodes provide valid output commands within the agreement threshold ($|y_a - y_b| \le 5$ µs), the voter shall output the mathematical median of the three values with status `VOTE_UNANIMOUS`.
- **Allocated Module**: [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c) (`vote_2oo3`)
- **Verification Method**: Host Unit Test (`tests/host/test_voter.c`), Test ID: `T-VOTE-001`.

---

#### SR-002: Single Compute Node Outlier Masking
- **Parent Hazard**: `HZ-01` | **Criticality**: Critical
- **Statement**: When exactly two compute nodes agree within the tolerance bound ($|y_a - y_b| \le 5$ µs) and the third node deviates by $> 5$ µs, the voter shall compute the commanded PWM as the arithmetic mean of the two agreeing nodes and flag the outlier node with status `VOTE_MAJORITY_2OO3`.
- **Allocated Module**: [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c) (`vote_2oo3`)
- **Verification Method**: Host Unit Test (`tests/host/test_voter.c`), Test ID: `T-VOTE-002`, `T-VOTE-003`, `T-VOTE-004`.

---

#### SR-003: Actuator Command Plausibility & Bound Clamping
- **Parent Hazard**: `HZ-01`, `HZ-06` | **Criticality**: Critical
- **Statement**: The flight control and voting pipeline shall clamp all commanded actuator pulse widths to the legal physical envelope of $1000$ µs to $2000$ µs using safe saturating arithmetic.
- **Allocated Module**: [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c), [`src/safe_math.h`](file:///c:/Users/hp/Desktop/TMR/src/safe_math.h)
- **Verification Method**: Host Unit Test (`tests/host/test_safe_math.c`), Test ID: `T-MATH-006`.

---

#### SR-004: Core Execution Deadline & Watchdog Supervision
- **Parent Hazard**: `HZ-02` | **Criticality**: Critical
- **Statement**: If any compute node fails to publish its frame completion token within `WATCHDOG_MAX_CYCLES` (500,000 cycles), Core 0 shall terminate the wait, isolate the unresponding core, and assert the fail-safe command.
- **Allocated Module**: [`src/amp.c`](file:///c:/Users/hp/Desktop/TMR/src/amp.c), [`src/supervision.c`](file:///c:/Users/hp/Desktop/TMR/src/supervision.c)
- **Verification Method**: QEMU Live Flight Simulation, Test ID: `T-QEMU-009`.

---

#### SR-005: Deterministic Fail-Safe Actuator Latching
- **Parent Hazard**: `HZ-02` | **Criticality**: Critical
- **Statement**: Upon total disagreement (no two nodes agreeing within 5 µs), watchdog timeout, or unrecoverable exception, the flight computer shall command the neutral fail-safe pulse width of $-9999$ µs and permanently latch the fail-safe state until external ground intervention.
- **Allocated Module**: [`src/failsafe.c`](file:///c:/Users/hp/Desktop/TMR/src/failsafe.c)
- **Verification Method**: Host Unit Test (`tests/host/test_failsafe.c`), Test ID: `T-SAFE-001`, `T-SAFE-002`.

---

#### SR-006: Inter-Core CRC32 Mailbox Integrity Verification
- **Parent Hazard**: `HZ-03` | **Criticality**: High
- **Statement**: All inter-core communication across shared RAM partitions shall be encapsulated in double-buffered slots validated by an IEEE 802.3 32-bit Cyclic Redundancy Check (CRC32); any slot exhibiting CRC mismatch shall be rejected as invalid.
- **Allocated Module**: [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c)
- **Verification Method**: Host Unit Test (`tests/host/test_mailbox.c`), Test ID: `T-MBOX-001`, `T-MBOX-003`.

---

#### SR-007: Stale Frame Replay Rejection
- **Parent Hazard**: `HZ-03` | **Criticality**: High
- **Statement**: Each mailbox slot shall contain a monotonically incrementing frame sequence counter; the receiver shall reject any message whose sequence ID does not match the active frame index.
- **Allocated Module**: [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c)
- **Verification Method**: Host Unit Test (`tests/host/test_mailbox.c`), Test ID: `T-MBOX-003`.

---

#### SR-008: Spatial Memory Translation & Core Isolation
- **Parent Hazard**: `HZ-04` | **Criticality**: Critical
- **Statement**: The system shall configure ARMv7-A Short-Descriptor MMU tables such that secondary compute nodes are restricted to read-only access to code, read-write access to their own zone, and zero access to other zones or Core 0 private system memory.
- **Allocated Module**: [`src/mmu.c`](file:///c:/Users/hp/Desktop/TMR/src/mmu.c)
- **Verification Method**: Host Unit Test (`tests/host/test_mmu.c`), Test ID: `T-MMU-001..004`.

---

#### SR-009: Stack Canary and High-Water Mark Defense
- **Parent Hazard**: `HZ-04` | **Criticality**: Critical
- **Statement**: Each core's dedicated stack shall be bounded by four 32-bit canary words (`0xDEADBEEF`); any corruption detected during frame supervision shall immediately trigger fail-safe transition.
- **Allocated Module**: [`src/stack_monitor.c`](file:///c:/Users/hp/Desktop/TMR/src/stack_monitor.c)
- **Verification Method**: Host Unit Test (`tests/host/test_stack_monitor.c`), Test ID: `T-STK-001`.

---

#### SR-010: Master Arbiter Dual-Rail Software Lockstep
- **Parent Hazard**: `HZ-05` | **Criticality**: Critical
- **Statement**: Core 0 shall execute redundant voter consensus pipelines using inverted operand encoding and cross-compare both results before issuing actuator commands to detect internal CPU ALU transient errors.
- **Allocated Module**: [`src/lockstep.c`](file:///c:/Users/hp/Desktop/TMR/src/lockstep.c)
- **Verification Method**: Host Unit Test (`tests/host/test_lockstep.c`), Test ID: `T-LOCK-001`, `T-LOCK-002`.

---

#### SR-011: Voter Self-Monitoring & State Scrubbing
- **Parent Hazard**: `HZ-05` | **Criticality**: Critical
- **Statement**: Core 0 shall evaluate golden built-in test vectors through its voter logic periodically and assert immediate fail-safe if the self-monitoring step detects arithmetic divergence.
- **Allocated Module**: [`src/lockstep.c`](file:///c:/Users/hp/Desktop/TMR/src/lockstep.c), [`src/post.c`](file:///c:/Users/hp/Desktop/TMR/src/post.c)
- **Verification Method**: Host Unit Test (`tests/host/test_lockstep.c`), Test ID: `T-LOCK-003`.

---

#### SR-012: Command Rate-of-Change Limiting
- **Parent Hazard**: `HZ-06` | **Criticality**: High
- **Statement**: The voter subsystem shall enforce a maximum pulse-width rate-of-change limit of $\le 200$ µs between consecutive execution frames.
- **Allocated Module**: [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c) (`voter_enforce_rate_limit`)
- **Verification Method**: Host Unit Test (`tests/host/test_voter.c`), Test ID: `T-VOTE-009`.

---

#### SR-013: Triplicate Sensor Channel Cross-Checking & Voting
- **Parent Hazard**: `HZ-06` | **Criticality**: High
- **Statement**: The sensor input pipeline shall support triplicated sensor channels, voting on inputs prior to control computation and rejecting single-channel outliers exceeding `SENSOR_VOTE_TOLERANCE` (10 ddeg/s).
- **Allocated Module**: [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c) (`sensor_validate_triplicate`)
- **Verification Method**: Host Unit Test (`tests/host/test_diversity.c`), Test ID: `T-SENS-001..006`.

---

#### SR-014: Power-On Self-Test (POST) Hardware Validation
- **Parent Hazard**: `HZ-07` | **Criticality**: Critical
- **Statement**: Prior to releasing secondary compute cores or enabling actuator commands, the system shall complete POST diagnostics including CPU registers, RAM March C- scan, and code section CRC32.
- **Allocated Module**: [`src/post.c`](file:///c:/Users/hp/Desktop/TMR/src/post.c)
- **Verification Method**: Host Unit Test (`tests/host/test_post.c`), Test ID: `T-POST-001..005`.

---

#### SR-015: Node Fault Latching & Leaky-Bucket Decay
- **Parent Hazard**: `HZ-01`, `HZ-02` | **Criticality**: Critical
- **Statement**: Any compute node that accumulates $\ge 3$ consecutive equivalent faults shall be latched out permanently from the voting pool for the remainder of the flight profile.
- **Allocated Module**: [`src/node_health.c`](file:///c:/Users/hp/Desktop/TMR/src/node_health.c)
- **Verification Method**: Host Unit Test (`tests/host/test_voter.c`), Test ID: `T-HLTH-001..003`.

---

#### SR-016: Algorithmic Diversity between Redundant Channels
- **Parent Hazard**: `HZ-01`, `HZ-05` | **Criticality**: Critical
- **Statement**: Node 2 shall support execution of an independently formulated control algorithm (Q15 fixed-point arithmetic) providing algorithmic and compiler optimization diversity against common-mode ALU defects.
- **Allocated Module**: [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c) (`flight_control_compute_diverse`)
- **Verification Method**: Host Unit Test (`tests/host/test_diversity.c`), Test ID: `T-DIV-001`, `T-DIV-002`.
