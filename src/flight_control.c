#include "flight_control.h"
#include "safe_math.h"
#include "config.h"

volatile fault_injection_t g_fault_mode = FAULT_NONE;

int32_t flight_control_compute_diverse(uint32_t core_id, int32_t sensor_input) {
    /*
     * Diverse Flight Control Formulation (Q15 Fixed-Point Arithmetic - P3.2):
     * Implements mathematically equivalent proportional gain Kp = 0.3
     * using fractional binary scaling:
     * Kp_q15 = round(0.3 * 32768) = 9830.
     * Provides algorithmic and compiler optimization diversity against common-mode ALU bugs.
     */
    int32_t error = sensor_input;
    int32_t prod = safe_mul_i32(error, 9830);
    int32_t rounded_prod = (prod >= 0) ? safe_add_i32(prod, 16384) : safe_sub_i32(prod, 16384);
    int32_t delta = rounded_prod / 32768;
    int32_t pwm = safe_add_i32(PWM_NEUTRAL_MICROSECONDS, delta);

    /* Apply Fault Injection consistent with Node 2 */
    if (g_fault_mode == FAULT_SEU_NODE2 && core_id == 2) {
        pwm ^= (1 << 8);
    } else if (g_fault_mode == FAULT_BOUNDED_NOISE) {
        if (core_id == 2) pwm = safe_sub_i32(pwm, 3);
    } else if (g_fault_mode == FAULT_TOTAL_DISAGREE) {
        if (core_id == 2) pwm = safe_sub_i32(pwm, 80);
    } else if (g_fault_mode == FAULT_HANG_NODE2 && core_id == 2) {
        while (1) {
            __asm__ volatile("nop");
        }
    }

    return safe_clamp_i32(pwm, PWM_MIN_MICROSECONDS, PWM_MAX_MICROSECONDS);
}

int32_t flight_control_compute(uint32_t core_id, int32_t sensor_input) {
#if DIVERSITY_ENABLED
    if (core_id == 2) {
        return flight_control_compute_diverse(core_id, sensor_input);
    }
#endif

    /*
     * Deterministic Flight Control Algorithm (Primary Implementation):
     * Computes commanded actuator PWM from sensor attitude feedback.
     * Nominal sensor input is attitude rate in tenths of a degree per second.
     * Neutral pulse width is 1500 microseconds.
     */
    int32_t error = sensor_input; /* Desired attitude rate is 0 */
    
    /* Fixed-point proportional control: Kp = 0.3 with saturating arithmetic */
    int32_t delta = safe_div_i32(safe_mul_i32(error, 3), 10, 0);
    int32_t pwm = safe_add_i32(PWM_NEUTRAL_MICROSECONDS, delta);
    
    /* Apply Fault Injection based on simulated Single Event Upsets (SEUs) */
    if (g_fault_mode == FAULT_SEU_NODE1 && core_id == 1) {
        /* Simulate cosmic ray SEU bit-flip in Node 1 register */
        pwm ^= (1 << 9); /* Flip bit 9 (+/- 512 us deviation) */
    } else if (g_fault_mode == FAULT_SEU_NODE2 && core_id == 2) {
        /* Simulate SEU bit-flip in Node 2 arithmetic logic */
        pwm ^= (1 << 8); /* Flip bit 8 (+/- 256 us deviation) */
    } else if (g_fault_mode == FAULT_SEU_NODE3 && core_id == 3) {
        /* Simulate SEU bit-flip in Node 3 state output */
        pwm ^= (1 << 10); /* Flip bit 10 (+/- 1024 us deviation) */
    } else if (g_fault_mode == FAULT_BOUNDED_NOISE) {
        /* Bounded state estimator / sensor noise: all within delta <= 5 */
        if (core_id == 1) pwm = safe_add_i32(pwm, 2);
        if (core_id == 2) pwm = safe_sub_i32(pwm, 3);
        if (core_id == 3) pwm = safe_add_i32(pwm, 1);
    } else if (g_fault_mode == FAULT_TOTAL_DISAGREE) {
        /* Total Disagreement: multiple radiation strikes corrupting all channels */
        if (core_id == 1) pwm = safe_add_i32(pwm, 120);
        if (core_id == 2) pwm = safe_sub_i32(pwm, 80);
        if (core_id == 3) pwm = safe_add_i32(pwm, 260);
    } else if (g_fault_mode == FAULT_HANG_NODE2 && core_id == 2) {
        /* Simulate core hardware lockup / watchdog test */
        while (1) {
            __asm__ volatile("nop");
        }
    }
    
    /* Clamp actuator command within physical bounds */
    pwm = safe_clamp_i32(pwm, PWM_MIN_MICROSECONDS, PWM_MAX_MICROSECONDS);
    
    return pwm;
}

sensor_vote_result_t sensor_validate_triplicate(int32_t ch1, int32_t ch2, int32_t ch3) {
    sensor_vote_result_t result;
    result.voted_value = 0;
    result.status = SENSOR_DISAGREEMENT_FAIL;
    result.outlier_channel = 0;

    int32_t d12 = safe_diff_i32(ch1, ch2);
    int32_t d23 = safe_diff_i32(ch2, ch3);
    int32_t d13 = safe_diff_i32(ch1, ch3);

    bool b12 = (d12 <= SENSOR_VOTE_TOLERANCE);
    bool b23 = (d23 <= SENSOR_VOTE_TOLERANCE);
    bool b13 = (d13 <= SENSOR_VOTE_TOLERANCE);

    if (b12 && b23 && b13) {
        /* Unanimous: return median of three channels */
        int32_t med;
        if ((ch1 >= ch2 && ch1 <= ch3) || (ch1 <= ch2 && ch1 >= ch3)) {
            med = ch1;
        } else if ((ch2 >= ch1 && ch2 <= ch3) || (ch2 <= ch1 && ch2 >= ch3)) {
            med = ch2;
        } else {
            med = ch3;
        }
        result.voted_value = med;
        result.status = SENSOR_OK;
        result.outlier_channel = 0;
    } else if (b23 && !b12 && !b13) {
        /* Channel 1 is outlier */
        result.voted_value = safe_div_i32(safe_add_i32(ch2, ch3), 2, 0);
        result.status = SENSOR_DEGRADED_MASKED_1;
        result.outlier_channel = 1;
    } else if (b13 && !b12 && !b23) {
        /* Channel 2 is outlier */
        result.voted_value = safe_div_i32(safe_add_i32(ch1, ch3), 2, 0);
        result.status = SENSOR_DEGRADED_MASKED_2;
        result.outlier_channel = 2;
    } else if (b12 && !b23 && !b13) {
        /* Channel 3 is outlier */
        result.voted_value = safe_div_i32(safe_add_i32(ch1, ch2), 2, 0);
        result.status = SENSOR_DEGRADED_MASKED_3;
        result.outlier_channel = 3;
    } else if ((b12 && b23) || (b12 && b13) || (b23 && b13)) {
        /* Chain agreement case: take median of three */
        int32_t med;
        if ((ch1 >= ch2 && ch1 <= ch3) || (ch1 <= ch2 && ch1 >= ch3)) {
            med = ch1;
        } else if ((ch2 >= ch1 && ch2 <= ch3) || (ch2 <= ch1 && ch2 >= ch3)) {
            med = ch2;
        } else {
            med = ch3;
        }
        result.voted_value = med;
        result.status = SENSOR_OK;
        result.outlier_channel = 0;
    } else {
        /* Total sensor disagreement */
        result.voted_value = 0;
        result.status = SENSOR_DISAGREEMENT_FAIL;
        result.outlier_channel = 0;
    }

    return result;
}
