#ifndef VOTER_H
#define VOTER_H

#include "types.h"

#define FAIL_SAFE_VALUE         (-9999)
#define VOTER_TOLERANCE_BOUND   5
#define WATCHDOG_MAX_CYCLES     500000U

typedef enum {
    VOTE_UNANIMOUS = 0,
    VOTE_MAJORITY_NODE1_MASKED,
    VOTE_MAJORITY_NODE2_MASKED,
    VOTE_MAJORITY_NODE3_MASKED,
    VOTE_TOTAL_DISAGREEMENT,
    VOTE_TIMEOUT_ERROR
} vote_status_t;

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

/* Execute 2-out-of-3 Bounded Majority Voting */
voter_result_t vote_2oo3(int32_t y1, int32_t y2, int32_t y3);

/* Convert status enum to human-readable string */
const char* vote_status_to_string(vote_status_t status);

#endif /* VOTER_H */
