# Bi-Directional Safety Traceability Matrix
## DO-178C DAL A / ARP4754A Complete Life-Cycle Traceability

---

### 1. Traceability Flow Architecture

```
[HAZARDS: HZ-01..07] <---> [REQUIREMENTS: SR-001..016] <---> [DESIGN & CODE] <---> [TEST SUITES: T-xxx, FI-xxx]
```

---

### 2. Comprehensive Traceability Matrix

| Hazard ID | Safety Requirement | Design Element | Source File & Function | Verification Test ID | Test Category |
|---|---|---|---|---|---|
| **HZ-01** | `SR-001` | 2oo3 Median Voter | [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c): `vote_2oo3` | `T-VOTE-001` | Host Unit Test |
| **HZ-01** | `SR-002` | Single Outlier Masking | [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c): `vote_2oo3` | `T-VOTE-002` | Host Unit Test |
| **HZ-01** | `SR-003` | Plausibility & Clamping | [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c): `flight_control_compute` | `T-MATH-006` | Host Unit Test |
| **HZ-02** | `SR-004` | Frame Deadline Supervision | [`src/supervision.c`](file:///c:/Users/hp/Desktop/TMR/src/supervision.c): `supervision_evaluate_nodes` | `T-SUP-001` | Host Unit Test |
| **HZ-02** | `SR-005` | Fail-Safe Latching | [`src/failsafe.c`](file:///c:/Users/hp/Desktop/TMR/src/failsafe.c): `failsafe_trigger` | `T-SAFE-001` | Host Unit Test |
| **HZ-03** | `SR-006` | CRC32 Mailbox Validation | [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c): `mailbox_read_output` | `T-MBOX-001` | Host Unit Test |
| **HZ-03** | `SR-007` | Sequence Counter Check | [`src/mailbox.c`](file:///c:/Users/hp/Desktop/TMR/src/mailbox.c): `mailbox_read_output` | `T-MBOX-003` | Host Unit Test |
| **HZ-04** | `SR-008` | Spatial MMU Translation | [`src/mmu.c`](file:///c:/Users/hp/Desktop/TMR/src/mmu.c): `mmu_init_tables` | `T-MMU-001` | Host Unit Test |
| **HZ-04** | `SR-009` | Stack Boundary Canaries | [`src/stack_monitor.c`](file:///c:/Users/hp/Desktop/TMR/src/stack_monitor.c): `stack_canary_check_core` | `T-STK-001` | Host Unit Test |
| **HZ-05** | `SR-010` | Dual-Rail Arbiter Lockstep | [`src/lockstep.c`](file:///c:/Users/hp/Desktop/TMR/src/lockstep.c): `lockstep_verify_voter` | `T-LOCK-001` | Host Unit Test |
| **HZ-05** | `SR-011` | Voter Self-Monitoring | [`src/lockstep.c`](file:///c:/Users/hp/Desktop/TMR/src/lockstep.c): `lockstep_verify_voter` | `T-LOCK-003` | Host Unit Test |
| **HZ-06** | `SR-012` | PWM Rate-of-Change Limiter | [`src/voter.c`](file:///c:/Users/hp/Desktop/TMR/src/voter.c): `voter_apply_rate_limit` | `T-VOTE-009` | Host Unit Test |
| **HZ-06** | `SR-013` | Triplicate Sensor Cross-Check | [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c): `sensor_validate_triplicate` | `T-SENS-001` | Host Unit Test |
| **HZ-07** | `SR-014` | Power-On Self-Test (POST) | [`src/post.c`](file:///c:/Users/hp/Desktop/TMR/src/post.c): `post_run_all` | `T-POST-005` | Host Unit Test |
| **HZ-01** | `SR-015` | Leaky-Bucket Health Latch | [`src/node_health.c`](file:///c:/Users/hp/Desktop/TMR/src/node_health.c): `node_health_record_fault` | `T-HLTH-001` | Host Unit Test |
| **HZ-01** | `SR-016` | Algorithmic Design Diversity | [`src/flight_control.c`](file:///c:/Users/hp/Desktop/TMR/src/flight_control.c): `flight_control_compute_diverse` | `T-DIV-001` | Host Unit Test |
