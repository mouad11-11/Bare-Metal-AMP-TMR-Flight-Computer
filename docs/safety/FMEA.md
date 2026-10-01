# Failure Modes, Effects, and Criticality Analysis (FMEA / FMECA)
## Bare-Metal AMP TMR Flight Computer System
### Reference: ARP4761 / MIL-STD-1629A / DO-178C DAL A

---

### 1. Analysis Scope & Methodology

This FMECA evaluates every hardware-software subsystem in the Bare-Metal AMP TMR Flight Computer. Each subsystem is systematically examined for potential failure modes, failure causes, local and system effects, detection mechanisms, and mitigation safeguards.

---

### 2. Comprehensive Subsystem FMECA Table

| Subsystem | Component / Unit | Failure Mode | Failure Cause | Local Effect | System Effect | Detection Mechanism | Mitigation Safeguard | Severity |
|---|---|---|---|---|---|---|---|:---:|
| **1. Sensor Pipeline** | IMU / Gyro Ingest | Sensor stuck-at maximum ($+1500$) | Transducer failure or ADC rail short | Huge raw rate reading | Excessive pitch command demand | Saturated clamping in `safe_math.h` | Output clamped to $2000$ µs; rate limiter prevents instantaneous jerk. | **Major** |
| | Sensor Distribution | Torn sensor write to Node Mailbox | Interrupted memory bus cycle | Corrupted input word to one node | One node computes aberrant output | IEEE 802.3 CRC32 in mailbox | Node rejects corrupted mailbox slot; voter masks outlier node. | **Minor** |
| **2. Compute Nodes** | Cortex-A15 Cores 1..3 | Single Event Upset (SEU) in ALU register | Atmospheric cosmic ray neutron strike | Commanded PWM jumps by $+512$ µs | Aberrant command from 1 of 3 nodes | 2oo3 Voter delta threshold ($>5$ µs) | Outlier node masked; command driven by mean of 2 agreeing nodes. | **Minor (Masked)** |
| | Cortex-A15 Cores 1..3 | Infinite Loop / Core Lockup | Hardware pipeline freeze / memory stall | Core ceases responding | Node fails to publish completion token | Core 0 hardware watchdog spin-counter | Core 0 detects timeout, asserts mask `0x04`, routes to fail-safe. | **Critical** |
| | Cortex-A15 Cores 1..3 | Stack Overflow into neighboring zone | Unbounded recursion or local buffer overrun | Stack canary corruption or memory fault | Potential corruption of system partition | Stack canaries (`0xDEADBEEF`) & MMU guard page | Data Abort raised; core quarantined to fail-silent mode. | **Critical** |
| **3. Voter Subsystem** | 2oo3 Consensus Engine | Total Disagreement ($\Delta > 5$ µs all pairs) | Multi-channel ionizing radiation storm | No two channels agree | Cannot determine safe consensus output | Pairwise delta checks $d_{12}, d_{23}, d_{13} > 5$ | Instant fail-safe activation; command driven to safe state $-9999$ µs. | **Critical** |
| | Median Selection | Arithmetic Overflow during Delta Diff | Edge condition with extreme values | Incorrect signed delta calculation | False agreement or incorrect masking | 64-bit safe difference (`safe_diff_i32`) | Saturated arithmetic prevents 32-bit signed overflow. | **Minor** |
| **4. Health Subsystem** | Node Health Monitor | Intermittent Chatter / Flapping Fault | Degraded silicon or marginal voltage rail | Node oscillates between good and bad | Exhausts voter margins periodically | Leaky-bucket fault filter ($N=3, M=100$) | Node permanently latched offline upon 3rd fault; no in-flight re-admission. | **Major** |
| **5. Supervision** | Frame Watchdog & CFI | Control-Flow Integrity (CFI) Violation | Program counter glitch / jump over checks | Frame phases executed out of order | Watchdog kicked without valid vote | Monotonic signature accumulator | Signature mismatch prevents watchdog kick; triggers forced reset. | **Critical** |
| **6. Inter-Core IPC** | Shared RAM Mailbox | Stale Frame Replay Attack / Hazard | Core 0 delayed; node reads previous frame | Node computes on old sensor data | Latency penalty / phase lag | Frame sequence counter verification | Sequence ID mismatch flags packet as invalid; node enters wait loop. | **Minor** |
| **7. Exceptions** | ARMv7-A Vector Table | Unhandled Data / Prefetch Abort | Illegal memory address dereference | Processor halts or vectors to undefined | Uncontrolled system crash | Populated VBAR exception vector stubs | Fault context recorded in ring buffer; node parks in `WFE`; Core 0 to SAFE. | **Critical** |
