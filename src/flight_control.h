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

/* Deterministic Flight Control Algorithm (Primary Implementation) */
int32_t flight_control_compute(uint32_t core_id, int32_t sensor_input);

/* Diverse Flight Control Implementation (Q15 Fixed-Point Arithmetic - P3.2) */
int32_t flight_control_compute_diverse(uint32_t core_id, int32_t sensor_input);

/* Sensor Input Cross-Checking & Triplicate Voting (P3.3) */
typedef enum {
    SENSOR_OK = 0,
    SENSOR_DEGRADED_MASKED_1,
    SENSOR_DEGRADED_MASKED_2,
    SENSOR_DEGRADED_MASKED_3,
    SENSOR_DISAGREEMENT_FAIL
} sensor_vote_status_t;

typedef struct {
    int32_t voted_value;
    sensor_vote_status_t status;
    uint32_t outlier_channel;
} sensor_vote_result_t;

/**
 * @brief Cross-check and vote on triplicate sensor channels prior to control computation.
 */
sensor_vote_result_t sensor_validate_triplicate(int32_t ch1, int32_t ch2, int32_t ch3);

#endif /* FLIGHT_CONTROL_H */
