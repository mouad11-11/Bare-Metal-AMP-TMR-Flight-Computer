#ifndef FLIGHT_CONTROL_H
#define FLIGHT_CONTROL_H

#include "types.h"

/* Fault Injection Scenarios */
typedef enum {
    FAULT_NONE = 0,
    FAULT_SEU_NODE1,        /* Bit-flip / major deviation on Node 1 */
    FAULT_SEU_NODE2,        /* Bit-flip / major deviation on Node 2 */
    FAULT_SEU_NODE3,        /* Bit-flip / major deviation on Node 3 */
    FAULT_BOUNDED_NOISE,    /* Noise within tolerance bound (|delta| <= 5) */
    FAULT_TOTAL_DISAGREE,   /* All nodes disagree by > 5 */
    FAULT_HANG_NODE2        /* Node 2 hangs (tests watchdog timeout) */
} fault_injection_t;

/* Global fault injection configuration (set by Core 0 for testing) */
extern volatile fault_injection_t g_fault_mode;

/* Actuator PWM Limits */
#define PWM_MIN_MICROSECONDS    1000
#define PWM_NEUTRAL_MICROSECONDS 1500
#define PWM_MAX_MICROSECONDS    2000

/* Deterministic Flight Control Algorithm */
int32_t flight_control_compute(uint32_t core_id, int32_t sensor_input);

#endif /* FLIGHT_CONTROL_H */
