# Assumptions of Use (AoU) & System Operating Context
## Bare-Metal AMP TMR Flight Computer System
### Reference: DO-178C Section 11.2 / ARP4754A / ISO 26262-10

---

### 1. Operational Envelope & Environmental Context

This software safety baseline is developed under the following operational assumptions:

1. **Actuator Interface Protocol**:
   - The downstream actuator subsystem (servo controller, electro-hydraulic valve, or ESC) must evaluate both the `status` flag (`OK`, `DEGRADED`, `FAILSAFE`) and the pulse width value.
   - The actuator controller must incorporate an independent hardware loss-of-signal timeout (`ACTUATOR_TIMEOUT_MS = 50 ms`). If no valid PWM pulse is received within 50 ms, the actuator must autonomously feather or center to aerodynamic neutral.

2. **Sensor Signal Quality**:
   - Primary sensor attitude pitch rates are delivered in tenths of a degree per second (`ddeg/s`).
   - Normal operating envelope: $[-1000, +1000]$ ddeg/s.
   - Extreme boundary envelope: $[-1500, +1500]$ ddeg/s. Values outside this range are safely saturated.

3. **Radiation Environment & Fault Arrival Rate**:
   - Single Event Upset (SEU) events are assumed to follow a Poisson arrival process with an arrival rate $\lambda \ll 10^{-2}$ per frame.
   - The fault arrival rate must be low enough to permit the leaky-bucket filter ($M=100$ healthy frames per fault decay) to recover between isolated transient bit flips.

4. **Power-On Reset Conditions**:
   - Power-on hardware reset brings Core 0 into privileged SVC mode with caches disabled and interrupts masked.
   - The hardware base (CPU registers, SRAM, internal buses) must be defect-free at cold boot, verified by passing 100% of pre-flight POST diagnostics.
