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
    int32_t rail_b = rail_a; /* Presumed agreement unless independently computed otherwise */

    /* Independent Rail B computation of pairwise deltas */
    int32_t d12 = safe_diff_i32(y1, y2);
    int32_t d23 = safe_diff_i32(y2, y3);
    int32_t d13 = safe_diff_i32(y1, y3);

    /* Verify arithmetic difference integrity */
    if (res->diff12 != d12 || res->diff23 != d23 || res->diff13 != d13) {
        goto divergence_detected;
    }

    /* Verify consensus decision integrity against independent algebraic logic */
    switch (res->status) {
        case VOTE_UNANIMOUS: {
            int32_t expected_median = algebraic_median3(y1, y2, y3);
            rail_b = expected_median;
            if (rail_a != expected_median) {
                goto divergence_detected;
            }
            if (d12 > VOTER_TOLERANCE_BOUND || d23 > VOTER_TOLERANCE_BOUND || d13 > VOTER_TOLERANCE_BOUND) {
                goto divergence_detected;
            }
            break;
        }

        case VOTE_MAJORITY_NODE1_MASKED: {
            rail_b = safe_div_i32(safe_add_i32(y2, y3), 2, 0);
            if (rail_a != rail_b || d23 > VOTER_TOLERANCE_BOUND) {
                goto divergence_detected;
            }
            break;
        }

        case VOTE_MAJORITY_NODE2_MASKED: {
            rail_b = safe_div_i32(safe_add_i32(y1, y3), 2, 0);
            if (rail_a != rail_b || d13 > VOTER_TOLERANCE_BOUND) {
                goto divergence_detected;
            }
            break;
        }

        case VOTE_MAJORITY_NODE3_MASKED: {
            rail_b = safe_div_i32(safe_add_i32(y1, y2), 2, 0);
            if (rail_a != rail_b || d12 > VOTER_TOLERANCE_BOUND) {
                goto divergence_detected;
            }
            break;
        }

        case VOTE_TOTAL_DISAGREEMENT: {
            rail_b = FAIL_SAFE_VALUE;
            if (rail_a != FAIL_SAFE_VALUE) {
                goto divergence_detected;
            }
            break;
        }

        default:
            /* Other statuses (e.g. VOTE_TIMEOUT_ERROR, DEGRADED, CHAIN_CASE if handled) */
            break;
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
