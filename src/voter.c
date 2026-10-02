#include "voter.h"
#include "node_health.h"
#include "failsafe.h"
#include "safe_math.h"

static int32_t s_last_commanded_pwm = PWM_NEUTRAL_US;

static inline int32_t safe_diff(int32_t a, int32_t b) {
    return safe_diff_i32(a, b);
}

int32_t median3(int32_t a, int32_t b, int32_t c) {
    if ((a >= b && a <= c) || (a <= b && a >= c)) {
        return a;
    }
    if ((b >= a && b <= c) || (b <= a && b >= c)) {
        return b;
    }
    return c;
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
        /* All three cores agree within tolerance bound: Unanimous consensus via median */
        res.status = VOTE_UNANIMOUS;
        res.final_pwm = median3(y1, y2, y3);
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
         * Chain case: Two pairs agree (e.g. Node 2 bridges Nodes 1 and 3).
         * Select the bridging node via median3 and attribute mask to the closest pair.
         */
        if (pair12 && (res.diff12 <= res.diff23 && res.diff12 <= res.diff13)) {
            res.status = VOTE_MAJORITY_NODE3_MASKED;
            res.final_pwm = median3(y1, y2, y3);
        } else if (pair13 && (res.diff13 <= res.diff12 && res.diff13 <= res.diff23)) {
            res.status = VOTE_MAJORITY_NODE2_MASKED;
            res.final_pwm = median3(y1, y2, y3);
        } else {
            res.status = VOTE_MAJORITY_NODE1_MASKED;
            res.final_pwm = median3(y1, y2, y3);
        }
    } else {
        /*
         * Total Disagreement: No two nodes agree within tolerance bound.
         * Drives predefined fail-safe command.
         */
        res.status = VOTE_TOTAL_DISAGREEMENT;
        res.final_pwm = FAIL_SAFE_VALUE;
    }

    return res;
}

int32_t voter_apply_rate_limit(int32_t candidate_pwm) {
    if (candidate_pwm == FAIL_SAFE_VALUE) {
        return FAIL_SAFE_VALUE;
    }

    int32_t clamped = candidate_pwm;
    int32_t step = candidate_pwm - s_last_commanded_pwm;

    if (step > PWM_MAX_STEP_US) {
        clamped = s_last_commanded_pwm + PWM_MAX_STEP_US;
    } else if (step < -PWM_MAX_STEP_US) {
        clamped = s_last_commanded_pwm - PWM_MAX_STEP_US;
    }

    s_last_commanded_pwm = clamped;
    return clamped;
}

void voter_reset_rate_limit(int32_t initial_pwm) {
    s_last_commanded_pwm = initial_pwm;
}

voter_result_t vote_frame_inputs(const node_sample_t samples[3], uint32_t expected_frame_id) {
    voter_result_t res;
    res.val1 = samples[0].pwm_us;
    res.val2 = samples[1].pwm_us;
    res.val3 = samples[2].pwm_us;
    res.diff12 = safe_diff(samples[0].pwm_us, samples[1].pwm_us);
    res.diff23 = safe_diff(samples[1].pwm_us, samples[2].pwm_us);
    res.diff13 = safe_diff(samples[0].pwm_us, samples[2].pwm_us);
    res.timed_out_core_mask = 0;
    res.status = VOTE_INSUFFICIENT_NODES;
    res.final_pwm = FAIL_SAFE_VALUE;

    bool valid_node[3] = { false, false, false };
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < 3; i++) {
        uint32_t node_id = i + 1;
        if (node_health_is_latched(node_id)) {
            continue;
        }
        if (!samples[i].valid) {
            node_health_record_fault(node_id);
            continue;
        }
        if (samples[i].frame_id != expected_frame_id) {
            node_health_record_fault(node_id);
            continue;
        }
        if (samples[i].pwm_us < PWM_MIN_US || samples[i].pwm_us > PWM_MAX_US) {
            node_health_record_fault(node_id);
            continue;
        }
        valid_node[i] = true;
        valid_count++;
    }

    if (valid_count == 3) {
        /* Nominal 3-node voting */
        res = vote_2oo3(samples[0].pwm_us, samples[1].pwm_us, samples[2].pwm_us);

        if (res.status == VOTE_TOTAL_DISAGREEMENT) {
            node_health_record_fault(1);
            node_health_record_fault(2);
            node_health_record_fault(3);
            failsafe_trigger(REASON_TOTAL_DISAGREEMENT);
        } else {
            int32_t med = median3(
                samples[0].pwm_us,
                samples[1].pwm_us,
                samples[2].pwm_us
            );

            for (uint32_t i = 0; i < 3; i++) {
                int32_t deviation = safe_diff(samples[i].pwm_us, med);

                if (deviation > HARD_FAULT_THRESHOLD) {
                    node_health_record_fault(i + 1);
                } else if (deviation > VOTER_TOLERANCE_BOUND) {
                    node_health_record_transient_mask(i + 1);
                } else {
                    node_health_record_success(i + 1);
                }
            }
        }

        if (res.final_pwm != FAIL_SAFE_VALUE) {
            res.final_pwm = voter_apply_rate_limit(res.final_pwm);
        }
        return res;
    }

#if ALLOW_DEGRADED_2OO2
    if (valid_count == 2) {
        /* Degraded 2-out-of-2 operational mode */
        uint32_t a = 0, b = 1;
        if (!valid_node[0]) {
            a = 1; b = 2;
        } else if (!valid_node[1]) {
            a = 0; b = 2;
        }

        int32_t ya = samples[a].pwm_us;
        int32_t yb = samples[b].pwm_us;
        int32_t dab = safe_diff(ya, yb);

        if (dab <= VOTER_TOLERANCE_BOUND) {
            res.status = VOTE_DEGRADED_2OO2;
            res.final_pwm = (int32_t)(((int64_t)ya + yb) / 2);
            node_health_record_success(a + 1);
            node_health_record_success(b + 1);
            res.final_pwm = voter_apply_rate_limit(res.final_pwm);
        } else {
            res.status = VOTE_CANNOT_ARBITRATE;
            res.final_pwm = FAIL_SAFE_VALUE;
            node_health_record_fault(a + 1);
            node_health_record_fault(b + 1);
            failsafe_trigger(REASON_CANNOT_ARBITRATE);
        }
        return res;
    }
#endif

    /* Fewer than 2 healthy nodes: Quorum loss */
    res.status = VOTE_INSUFFICIENT_NODES;
    res.final_pwm = FAIL_SAFE_VALUE;
    failsafe_trigger(REASON_INSUFFICIENT_NODES);
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
        case VOTE_DEGRADED_2OO2:
            return "DEGRADED 2oo2 (Consensus Reached)";
        case VOTE_CANNOT_ARBITRATE:
            return "FAIL-SAFE ACTIVATED (Cannot Arbitrate)";
        case VOTE_INSUFFICIENT_NODES:
            return "FAIL-SAFE ACTIVATED (Insufficient Nodes)";
        case VOTE_PLAUSIBILITY_FAULT:
            return "FAIL-SAFE ACTIVATED (Actuator Out-of-Bounds)";
        case VOTE_SEQUENCE_FAULT:
            return "FAIL-SAFE ACTIVATED (Stale Sequence Token)";
        default:
            return "UNKNOWN STATUS";
    }
}
