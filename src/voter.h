#ifndef VOTER_H
#define VOTER_H

#include "types.h"
#include "config.h"

#define FAIL_SAFE_VALUE         FAILSAFE_PWM_US
#define VOTER_TOLERANCE_BOUND   VOTE_AGREE_THRESHOLD_US
#define HARD_FAULT_THRESHOLD    HARD_FAULT_THRESHOLD_US

/*
 * ==============================================================================
 * Bounded Majority Voter (Decision Tree 1)
 *
 * Implements 2-out-of-3 voting with median consensus, chain case resolution,
 * degraded 2-out-of-2 arbitration, plausibility bounds, and rate limiting.
 * ==============================================================================
 */

typedef enum {
    VOTE_UNANIMOUS = 0,
    VOTE_MAJORITY_NODE1_MASKED,
    VOTE_MAJORITY_NODE2_MASKED,
    VOTE_MAJORITY_NODE3_MASKED,
    VOTE_TOTAL_DISAGREEMENT,
    VOTE_TIMEOUT_ERROR,
    VOTE_DEGRADED_2OO2,
    VOTE_CANNOT_ARBITRATE,
    VOTE_INSUFFICIENT_NODES,
    VOTE_PLAUSIBILITY_FAULT,
    VOTE_SEQUENCE_FAULT
} vote_status_t;

typedef struct {
    int32_t pwm_us;
    uint32_t frame_id;
    bool valid;
} node_sample_t;

typedef struct {
    int32_t final_pwm;
    vote_status_t status;
    int32_t val1;
    int32_t val2;
    int32_t val3;
    int32_t diff12;
    int32_t diff23;
    int32_t diff13;
    uint32_t timed_out_core_mask;
} voter_result_t;

/* Compute median of 3 signed 32-bit integers */
int32_t median3(int32_t a, int32_t b, int32_t c);

/* Pure functional 2-out-of-3 majority gate with median selection */
voter_result_t vote_2oo3(int32_t y1, int32_t y2, int32_t y3);

/* Full Tree 1 voting engine integrating health tracking, plausibility, and 2oo2 fallback */
voter_result_t vote_frame_inputs(const node_sample_t samples[3], uint32_t expected_frame_id);

/* Apply actuator command rate limiter (PWM_MAX_STEP_US) */
int32_t voter_apply_rate_limit(int32_t candidate_pwm);

/* Reset rate limiter state */
void voter_reset_rate_limit(int32_t initial_pwm);

/* Convert status enum to human-readable telemetry string */
const char* vote_status_to_string(vote_status_t status);

#endif /* VOTER_H */
