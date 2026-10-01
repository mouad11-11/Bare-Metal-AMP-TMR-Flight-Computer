#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "flight_control.h"
#include "safe_math.h"
#include "config.h"

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_diversity_algorithm_equivalence(void) {
    printf("[RUN] T-DIV-001: Primary vs Diverse Algorithm Equivalence across [-1500, +1500]\n");
    g_fault_mode = FAULT_NONE;

    for (int32_t input = -1500; input <= 1500; input++) {
        int32_t primary = flight_control_compute(1, input);
        int32_t diverse = flight_control_compute_diverse(2, input);

        int32_t diff = safe_diff_i32(primary, diverse);
        /* Algorithms must agree within +/- 1 microsecond across entire flight regime */
        TEST_ASSERT(diff <= 1);
        TEST_ASSERT(primary >= PWM_MIN_MICROSECONDS && primary <= PWM_MAX_MICROSECONDS);
        TEST_ASSERT(diverse >= PWM_MIN_MICROSECONDS && diverse <= PWM_MAX_MICROSECONDS);
    }
}

static void test_diversity_fault_injection(void) {
    printf("[RUN] T-DIV-002: Diverse Algorithm Fault Injection Modes\n");
    
    /* SEU bit-flip on Node 2 */
    g_fault_mode = FAULT_SEU_NODE2;
    int32_t base = flight_control_compute(1, 0);
    int32_t seu_node2 = flight_control_compute_diverse(2, 0);
    TEST_ASSERT(base == 1500);
    TEST_ASSERT(seu_node2 != 1500);
    TEST_ASSERT(safe_diff_i32(base, seu_node2) >= 200);

    /* Bounded noise on Node 2 */
    g_fault_mode = FAULT_BOUNDED_NOISE;
    int32_t noise_node2 = flight_control_compute_diverse(2, 0);
    TEST_ASSERT(noise_node2 == 1497);

    /* Total disagreement on Node 2 */
    g_fault_mode = FAULT_TOTAL_DISAGREE;
    int32_t dis_node2 = flight_control_compute_diverse(2, 0);
    TEST_ASSERT(dis_node2 == 1420);

    g_fault_mode = FAULT_NONE;
}

static void test_triplicate_sensor_voting(void) {
    printf("[RUN] T-SENS-001: Triplicate Sensor Unanimous Agreement\n");
    sensor_vote_result_t res = sensor_validate_triplicate(100, 104, 102);
    TEST_ASSERT(res.status == SENSOR_OK);
    TEST_ASSERT(res.voted_value == 102);
    TEST_ASSERT(res.outlier_channel == 0);

    printf("[RUN] T-SENS-002: Triplicate Sensor Channel 1 Outlier Masking\n");
    res = sensor_validate_triplicate(500, 100, 104);
    TEST_ASSERT(res.status == SENSOR_DEGRADED_MASKED_1);
    TEST_ASSERT(res.voted_value == 102);
    TEST_ASSERT(res.outlier_channel == 1);

    printf("[RUN] T-SENS-003: Triplicate Sensor Channel 2 Outlier Masking\n");
    res = sensor_validate_triplicate(100, -300, 106);
    TEST_ASSERT(res.status == SENSOR_DEGRADED_MASKED_2);
    TEST_ASSERT(res.voted_value == 103);
    TEST_ASSERT(res.outlier_channel == 2);

    printf("[RUN] T-SENS-004: Triplicate Sensor Channel 3 Outlier Masking\n");
    res = sensor_validate_triplicate(100, 105, 999);
    TEST_ASSERT(res.status == SENSOR_DEGRADED_MASKED_3);
    TEST_ASSERT(res.voted_value == 102);
    TEST_ASSERT(res.outlier_channel == 3);

    printf("[RUN] T-SENS-005: Triplicate Sensor Chain Agreement\n");
    /* d12 = 8 <= 10, d23 = 8 <= 10, but d13 = 16 > 10 */
    res = sensor_validate_triplicate(100, 108, 116);
    TEST_ASSERT(res.status == SENSOR_OK);
    TEST_ASSERT(res.voted_value == 108);
    TEST_ASSERT(res.outlier_channel == 0);

    printf("[RUN] T-SENS-006: Triplicate Sensor Total Disagreement Failure\n");
    /* All pairwise deltas > 10 */
    res = sensor_validate_triplicate(100, 200, 300);
    TEST_ASSERT(res.status == SENSOR_DISAGREEMENT_FAIL);
    TEST_ASSERT(res.outlier_channel == 0);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Design Diversity & Triplicate Sensor Test Runner (P3.2 & P3.3)\n");
    printf("==============================================================================\n");

    test_diversity_algorithm_equivalence();
    test_diversity_fault_injection();
    test_triplicate_sensor_voting();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
