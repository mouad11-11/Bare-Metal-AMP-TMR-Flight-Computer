#include "flight_control.h"

volatile fault_injection_t g_fault_mode = FAULT_NONE;

int32_t flight_control_compute(uint32_t core_id, int32_t sensor_input) {
    /*
     * Deterministic Flight Control Algorithm:
     * Computes commanded actuator PWM from sensor attitude feedback.
     * Nominal sensor input is attitude rate in tenths of a degree per second.
     * Neutral pulse width is 1500 microseconds.
     */
    int32_t error = sensor_input; /* Desired attitude rate is 0 */
    
    /* Fixed-point proportional control: Kp = 0.3 */
    int32_t delta = (error * 3) / 10;
    int32_t pwm = PWM_NEUTRAL_MICROSECONDS + delta;
    
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
        if (core_id == 1) pwm += 2;
        if (core_id == 2) pwm -= 3;
        if (core_id == 3) pwm += 1;
    } else if (g_fault_mode == FAULT_TOTAL_DISAGREE) {
        /* Total Disagreement: multiple radiation strikes corrupting all channels */
        if (core_id == 1) pwm += 120;
        if (core_id == 2) pwm -= 80;
        if (core_id == 3) pwm += 260;
    } else if (g_fault_mode == FAULT_HANG_NODE2 && core_id == 2) {
        /* Simulate core hardware lockup / watchdog test */
        while (1) {
            __asm__ volatile("nop");
        }
    }
    
    /* Clamp actuator command within physical bounds (unless in fail-safe) */
    if (pwm < PWM_MIN_MICROSECONDS) pwm = PWM_MIN_MICROSECONDS;
    if (pwm > PWM_MAX_MICROSECONDS) pwm = PWM_MAX_MICROSECONDS;
    
    return pwm;
}
