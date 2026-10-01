#include "voter.h"

static inline int32_t safe_diff(int32_t a, int32_t b) {
    int64_t diff = (int64_t)a - (int64_t)b;
    if (diff < 0) diff = -diff;
    if (diff > 0x7FFFFFFF) return 0x7FFFFFFF;
    return (int32_t)diff;
}

voter_result_t vote_2oo3(int32_t y1, int32_t y2, int32_t y3) {
    voter_result_t res;
    res.val1 = y1;
    res.val2 = y2;
    res.val3 = y3;
    res.diff12 = safe_diff(y1, y2);
    res.diff23 = safe_diff(y2, y3);
    res.diff13 = safe_diff(y1, y3);
    res.timed_out_core_mask = 0;

    bool pair12 = (res.diff12 <= VOTER_TOLERANCE_BOUND);
    bool pair23 = (res.diff23 <= VOTER_TOLERANCE_BOUND);
    bool pair13 = (res.diff13 <= VOTER_TOLERANCE_BOUND);

    if (pair12 && pair23 && pair13) {
        /* All three cores agree within tolerance bound */
        res.status = VOTE_UNANIMOUS;
        res.final_pwm = (int32_t)(((int64_t)y1 + (int64_t)y2 + (int64_t)y3) / 3);
    } else if (pair12 && !pair23 && !pair13) {
        /* Nodes 1 and 2 agree; Node 3 is an outlier */
        res.status = VOTE_MAJORITY_NODE3_MASKED;
        res.final_pwm = (int32_t)(((int64_t)y1 + (int64_t)y2) / 2);
    } else if (pair13 && !pair12 && !pair23) {
        /* Nodes 1 and 3 agree; Node 2 is an outlier */
        res.status = VOTE_MAJORITY_NODE2_MASKED;
        res.final_pwm = (int32_t)(((int64_t)y1 + (int64_t)y3) / 2);
    } else if (pair23 && !pair12 && !pair13) {
        /* Nodes 2 and 3 agree; Node 1 is an outlier */
        res.status = VOTE_MAJORITY_NODE1_MASKED;
        res.final_pwm = (int32_t)(((int64_t)y2 + (int64_t)y3) / 2);
    } else if (pair12 || pair23 || pair13) {
        /*
         * Boundary condition: Two pairs agree (e.g. Node 2 bridges Nodes 1 and 3).
         * Select the agreeing pair with the lowest difference.
         */
        if (pair12 && (res.diff12 <= res.diff23 && res.diff12 <= res.diff13)) {
            res.status = VOTE_MAJORITY_NODE3_MASKED;
            res.final_pwm = (int32_t)(((int64_t)y1 + (int64_t)y2) / 2);
        } else if (pair13 && (res.diff13 <= res.diff12 && res.diff13 <= res.diff23)) {
            res.status = VOTE_MAJORITY_NODE2_MASKED;
            res.final_pwm = (int32_t)(((int64_t)y1 + (int64_t)y3) / 2);
        } else {
            res.status = VOTE_MAJORITY_NODE1_MASKED;
            res.final_pwm = (int32_t)(((int64_t)y2 + (int64_t)y3) / 2);
        }
    } else {
        /*
         * Total Disagreement: No two nodes agree within delta <= 5.
         * System commands predefined fail-safe state to prevent erratic actuation.
         */
        res.status = VOTE_TOTAL_DISAGREEMENT;
        res.final_pwm = FAIL_SAFE_VALUE;
    }

    return res;
}

const char* vote_status_to_string(vote_status_t status) {
    switch (status) {
        case VOTE_UNANIMOUS:
            return "UNANIMOUS (All Nodes Agree)";
        case VOTE_MAJORITY_NODE1_MASKED:
            return "MAJORITY 2oo3 (Node 1 Outlier Masked)";
        case VOTE_MAJORITY_NODE2_MASKED:
            return "MAJORITY 2oo3 (Node 2 Outlier Masked)";
        case VOTE_MAJORITY_NODE3_MASKED:
            return "MAJORITY 2oo3 (Node 3 Outlier Masked)";
        case VOTE_TOTAL_DISAGREEMENT:
            return "FAIL-SAFE ACTIVATED (Total Disagreement)";
        case VOTE_TIMEOUT_ERROR:
            return "FAIL-SAFE ACTIVATED (Core Watchdog Timeout)";
        default:
            return "UNKNOWN STATUS";
    }
}
