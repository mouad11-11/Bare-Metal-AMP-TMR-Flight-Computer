#include "lockstep.h"
#include "failsafe.h"
#include "safe_math.h"

static lockstep_diag_t s_diag = {
    .rail_a_pwm = PWM_NEUTRAL_US,
    .rail_b_pwm = PWM_NEUTRAL_US,
    .match = true,
    .verification_count = 0,
    .failure_count = 0
};

void lockstep_init(void) {
    s_diag.rail_a_pwm = PWM_NEUTRAL_US;
    s_diag.rail_b_pwm = PWM_NEUTRAL_US;
    s_diag.match = true;
    s_diag.verification_count = 0;
    s_diag.failure_count = 0;
}

static inline int32_t algebraic_median3(int32_t a, int32_t b, int32_t c) {
    int32_t min_ab = (a < b) ? a : b;
    int32_t max_ab = (a > b) ? a : b;
    int32_t min_max_c = (max_ab < c) ? max_ab : c;
    return (min_ab > min_max_c) ? min_ab : min_max_c;
}

bool lockstep_verify_voter(int32_t y1, int32_t y2, int32_t y3, voter_result_t *res) {
    if (!res) return false;

    /* Rail A value from primary voter pass */
    int32_t rail_a = res->final_pwm;
    int32_t rail_b = FAIL_SAFE_VALUE;

    /* Independent Rail B computation of pairwise deltas */
    int32_t d12 = safe_diff_i32(y1, y2);
    int32_t d23 = safe_diff_i32(y2, y3);
    int32_t d13 = safe_diff_i32(y1, y3);

    /* Verify arithmetic difference integrity against Rail A */
    if (res->diff12 != d12 || res->diff23 != d23 || res->diff13 != d13) {
        goto divergence_detected;
    }

    bool pair12 = (d12 <= VOTER_TOLERANCE_BOUND);
    bool pair23 = (d23 <= VOTER_TOLERANCE_BOUND);
    bool pair13 = (d13 <= VOTER_TOLERANCE_BOUND);

    vote_status_t independent_status = VOTE_TOTAL_DISAGREEMENT;
    int32_t independent_pwm = FAIL_SAFE_VALUE;

    if (pair12 && pair23 && pair13) {
        independent_status = VOTE_UNANIMOUS;
        independent_pwm = algebraic_median3(y1, y2, y3);
    } else if (pair12 && !pair23 && !pair13) {
        independent_status = VOTE_MAJORITY_NODE3_MASKED;
        independent_pwm = safe_div_i32(safe_add_i32(y1, y2), 2, 0);
    } else if (pair13 && !pair12 && !pair23) {
        independent_status = VOTE_MAJORITY_NODE2_MASKED;
        independent_pwm = safe_div_i32(safe_add_i32(y1, y3), 2, 0);
    } else if (pair23 && !pair12 && !pair13) {
        independent_status = VOTE_MAJORITY_NODE1_MASKED;
        independent_pwm = safe_div_i32(safe_add_i32(y2, y3), 2, 0);
    } else if (pair12 || pair23 || pair13) {
        /* Chain case: Two pairs agree (e.g. Node 2 bridges Nodes 1 and 3) */
        if (pair12 && (d12 <= d23 && d12 <= d13)) {
            independent_status = VOTE_MAJORITY_NODE3_MASKED;
            independent_pwm = algebraic_median3(y1, y2, y3);
        } else if (pair13 && (d13 <= d12 && d13 <= d23)) {
            independent_status = VOTE_MAJORITY_NODE2_MASKED;
            independent_pwm = algebraic_median3(y1, y2, y3);
        } else {
            independent_status = VOTE_MAJORITY_NODE1_MASKED;
            independent_pwm = algebraic_median3(y1, y2, y3);
        }
    } else {
        independent_status = VOTE_TOTAL_DISAGREEMENT;
        independent_pwm = FAIL_SAFE_VALUE;
    }

    rail_b = independent_pwm;

    /* Verify both status and PWM match independent Rail B derivation */
    if (res->status != independent_status || rail_a != independent_pwm) {
        goto divergence_detected;
    }

    /* Agreement verified */
    s_diag.rail_a_pwm = rail_a;
    s_diag.rail_b_pwm = rail_b;
    s_diag.match = true;
    s_diag.verification_count++;
    return true;

divergence_detected:
    s_diag.rail_a_pwm = rail_a;
    s_diag.rail_b_pwm = rail_b;
    s_diag.match = false;
    s_diag.failure_count++;
    failsafe_trigger(REASON_INTEGRITY_FAIL);
    res->final_pwm = FAIL_SAFE_VALUE;
    res->status = VOTE_TOTAL_DISAGREEMENT;
    return false;
}

void lockstep_get_diagnostics(lockstep_diag_t *diag_out) {
    if (diag_out) {
        *diag_out = s_diag;
    }
}
