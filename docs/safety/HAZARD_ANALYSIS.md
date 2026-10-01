# Hazard Analysis and Risk Assessment (HARA)
## Bare-Metal AMP TMR Flight Computer System

---

### 1. Hazard Severity Classification Framework

Severity Classifications:
- **Catastrophic**: Results in loss of flight control, structural airframe failure, or loss of vehicle.
- **Hazardous / Severe-Major**: Large reduction in safety margins, physical distress, or higher crew workload.
- **Major**: Significant reduction in safety margins or operational capabilities.
- **Minor**: Slight reduction in safety margins.
- **No Safety Effect**: Inconvenience or no operational impact.

---

### 2. System Hazard Log

| Hazard ID | Hazard Description | Severity | Target SIL/DAL | Root Cause & Failure Mechanism | Operational Effect | Mitigation Architecture | Safety Req |
|---|---|:---:|:---:|---|---|---|:---:|
| **HZ-01** | Erroneous Actuator Output Command | **Catastrophic** | Critical | Cosmic ray SEU bit-flip in compute node ALU/register resulting in wild PWM pulse width. | Uncommanded control surface deflection; loss of aircraft attitude control. | Triple Modular Redundancy (TMR) with 2oo3 median voter masking single-node outliers. Output plausibility clamping ($1000 \dots 2000$ µs). | `SR-001`<br>`SR-002`<br>`SR-003` |
| **HZ-02** | Complete Loss of Actuator Command | **Catastrophic** | Critical | Compute core infinite spin-lock, deadlock, or unhandled CPU hardware exception. | Actuator floating or freezing in position; catastrophic control loss. | Core 0 hardware watchdog spin-counter, ARMv7-A exception vector table, and fail-safe latching to neutral $-9999$ µs. | `SR-004`<br>`SR-005` |
| **HZ-03** | Inter-Core Communication Corruption | **Hazardous** | High | Torn reads/writes, out-of-order memory pipeline hazards, or stale frame injection. | Arbiter acts on old or invalid node output data. | Double-buffered mailbox architecture, sequence counter tracking, and IEEE 802.3 CRC32 verification with `DMB` barriers. | `SR-006`<br>`SR-007` |
| **HZ-04** | Cross-Core Spatial Memory Contamination | **Catastrophic** | Critical | Stack overflow or runaway pointer on secondary core overwriting Arbiter system data or code. | Silent corruption of voting logic or system state; loss of redundancy. | ARMv7-A Short-Descriptor MMU tables enforcing Execute-Never (`XN`), isolated zone permissions, and stack canaries. | `SR-008`<br>`SR-009` |
| **HZ-05** | Master Arbiter Single-Point ALU Failure | **Catastrophic** | Critical | Transient bit flip or silicon defect inside Core 0 ALU during voter execution. | Corrupted command dispatched to actuator despite healthy redundant nodes. | Core 0 Dual-Rail Software Lockstep running parallel redundant voting pipelines and cross-comparing before actuation. | `SR-010`<br>`SR-011` |
| **HZ-06** | Sensor Input Anomaly or Out-of-Bounds | **Hazardous** | High | Gyro/IMU sensor failure, stuck-at fault, electrical noise, or transient telemetry glitch. | Massive control demand causing structural flutter or aerodynamic stall. | Rate-of-change limiter ($\le 200$ µs/frame), saturating arithmetic, and triplicate sensor cross-checking validation. | `SR-012`<br>`SR-013` |
| **HZ-07** | Latent Silicon / Memory Boot Defects | **Catastrophic** | Critical | Hardware memory cell stuck-at 0/1, corrupted flash text segment, or defective CPU register. | Flight execution commences with compromised hardware base. | Pre-flight Power-On Self-Test (POST) executing CPU register check, March C- RAM scan, code CRC32, and known voter vectors. | `SR-014`<br>`SR-015` |
