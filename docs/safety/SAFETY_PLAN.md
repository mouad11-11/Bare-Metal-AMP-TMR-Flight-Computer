# Software Safety Plan (SSP)
## Bare-Metal AMP TMR Flight Computer System
### Applicable Standards: DO-178C (DAL A) / ECSS-E-ST-40C / ISO 26262 (ASIL D Reference)

---

### 1. Document Control & Scope

This Software Safety Plan (SSP) establishes the life-cycle processes, design principles, verification activities, and assurance methods applied to the Bare-Metal Asymmetric Multiprocessing (AMP) Triple Modular Redundancy (TMR) Flight Computer.
The target system is an attitude control demonstrator responsible for ingesting pitch rates, calculating pitch actuator pulse-width commands (PWM), performing majority voting, and commanding actuators while mitigating Single Event Upsets (SEUs), hardware lockups, and spatial memory corruption.

---

### 2. Safety Lifecycle & Assurance Level

In accordance with DO-178C Table A-1, the software is developed under **Design Assurance Level (DAL) A (Catastrophic)**, where erroneous or lost actuator commands could lead to catastrophic loss of vehicle control.

```
+-----------------------------------------------------------------------------+
|                          DO-178C DAL A LIFECYCLE                            |
|                                                                             |
|  [System Hazard Analysis]                                                   |
|          |                                                                  |
|          v                                                                  |
|  [Safety Requirements: SR-001..SR-016]                                      |
|          |                                                                  |
|          v                                                                  |
|  [Architectural Design: AMP TMR, Spatial MMU, Dual-Rail Lockstep]           |
|          |                                                                  |
|          v                                                                  |
|  [Low-Level Source Implementation: C99, MISRA C:2012, NASA Power of Ten]    |
|          |                                                                  |
|          v                                                                  |
|  [Verification & Validation: 100% Branch/MCDC Coverage, Fault Injection]    |
+-----------------------------------------------------------------------------+
```

---

### 3. Safety Activities & Work Breakdown

1. **Hazard Identification & Analysis**:
   Perform System Hazard Analysis (HARA) and Fault Tree Analysis (FTA) to identify all catastrophic and hazardous conditions.
2. **Safety Requirements Allocation**:
   Derive formal, numbered, testable safety requirements (`SR-001` through `SR-016`) allocated to software components.
3. **Failure Modes, Effects, and Criticality Analysis (FMECA)**:
   Conduct detailed failure mode and effects analysis across all 7 functional subsystems.
4. **Common Cause Analysis (CCA)**:
   Evaluate common-mode failure risks (common power, shared clock, compiler optimization, identical algorithmic structures) and define independence mitigations.
5. **Architectural Hardening**:
   Implement spatial memory partitioning (MMU), double-buffered CRC32 mailboxes, dual-rail lockstep self-monitoring, and 2oo3 median voting.
6. **Multi-Tier Verification**:
   - Host native unit tests with 100% statement and branch coverage, plus MC/DC truth table proofs.
   - Target bare-metal QEMU multi-core simulation validating all flight telemetry strings.
   - Automated Fault Injection Campaign (113 vectors) demonstrating zero undetected erroneous outputs.
7. **Traceability Auditing**:
   Maintain bi-directional traceability linking Hazards -> Requirements -> Source Functions -> Verification Test IDs.

---

### 4. Software Safety Organization & Roles

- **Safety Manager**: Authorizes Safety Plan, approves hazard analyses, and verifies safety case integrity.
- **Lead Embedded Architect**: Enforces MISRA C:2012, NASA Power of Ten guidelines, and memory map isolation.
- **Verification Lead**: Develops host test harnesses, maintains coverage instrumentation, and executes fault injection campaigns.
