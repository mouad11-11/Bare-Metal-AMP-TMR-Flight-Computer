#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "voter.h"
#include "node_health.h"
#include "failsafe.h"

/* Host test stubs for embedded symbols */
volatile uint32_t core_ready[4] = {0, 0, 0, 0};
volatile uint32_t core_done[4] = {0, 0, 0, 0};
volatile uint32_t g_cycle_counter = 0;
void uart_printf(const char *fmt, ...) { (void)fmt; }
void uart_puts(const char *str) { (void)str; }

static uint32_t g_assertions = 0;

#define TEST_ASSERT(cond, msg, ...) do { \
    g_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(1); \
    } \
} while (0)

static int64_t ref_diff(int32_t a, int32_t b) {
    int64_t d = (int64_t)a - (int64_t)b;
    return (d < 0) ? -d : d;
}

static int32_t ref_median3(int32_t a, int32_t b, int32_t c) {
    if ((a >= b && a <= c) || (a <= b && a >= c)) return a;
    if ((b >= a && b <= c) || (b <= a && b >= c)) return b;
    return c;
}

typedef struct {
    int32_t pwm;
    vote_status_t status;
} ref_result_t;

static ref_result_t ref_vote_2oo3(int32_t y1, int32_t y2, int32_t y3) {
    ref_result_t r;
    int64_t d12 = ref_diff(y1, y2);
    int64_t d23 = ref_diff(y2, y3);
    int64_t d13 = ref_diff(y1, y3);

    bool p12 = (d12 <= VOTER_TOLERANCE_BOUND);
    bool p23 = (d23 <= VOTER_TOLERANCE_BOUND);
    bool p13 = (d13 <= VOTER_TOLERANCE_BOUND);

    if (p12 && p23 && p13) {
        r.status = VOTE_UNANIMOUS;
        r.pwm = ref_median3(y1, y2, y3);
    } else if (p12 && !p23 && !p13) {
        r.status = VOTE_MAJORITY_NODE3_MASKED;
        r.pwm = (int32_t)(((int64_t)y1 + y2) / 2);
    } else if (p13 && !p12 && !p23) {
        r.status = VOTE_MAJORITY_NODE2_MASKED;
        r.pwm = (int32_t)(((int64_t)y1 + y3) / 2);
    } else if (p23 && !p12 && !p13) {
        r.status = VOTE_MAJORITY_NODE1_MASKED;
        r.pwm = (int32_t)(((int64_t)y2 + y3) / 2);
    } else if (p12 || p23 || p13) {
        if (p12 && (d12 <= d23 && d12 <= d13)) {
            r.status = VOTE_MAJORITY_NODE3_MASKED;
            r.pwm = ref_median3(y1, y2, y3);
        } else if (p13 && (d13 <= d12 && d13 <= d23)) {
            r.status = VOTE_MAJORITY_NODE2_MASKED;
            r.pwm = ref_median3(y1, y2, y3);
        } else {
            r.status = VOTE_MAJORITY_NODE1_MASKED;
            r.pwm = ref_median3(y1, y2, y3);
        }
    } else {
        r.status = VOTE_TOTAL_DISAGREEMENT;
        r.pwm = FAIL_SAFE_VALUE;
    }
    return r;
}

/* T-VOTE-001: Named Vectors from Legacy Boot Test Suite */
static void test_named_vectors(void) {
    printf("[RUN] T-VOTE-001: Named test suite vectors\n");
    
    /* Frame 1: Nominal */
    voter_result_t r1 = vote_2oo3(1500, 1500, 1500);
    TEST_ASSERT(r1.status == VOTE_UNANIMOUS, "F1 expected unanimous");
    TEST_ASSERT(r1.final_pwm == 1500, "F1 expected 1500, got %d", r1.final_pwm);

    /* Frame 2: Bounded noise (Unanimous consensus via median) */
    voter_result_t r2 = vote_2oo3(1538, 1533, 1537);
    TEST_ASSERT(r2.status == VOTE_UNANIMOUS, "F2 expected unanimous");
    TEST_ASSERT(r2.final_pwm == 1537, "F2 expected 1537, got %d", r2.final_pwm);

    /* Frame 3: Node 1 SEU */
    voter_result_t r3 = vote_2oo3(2000, 1515, 1515);
    TEST_ASSERT(r3.status == VOTE_MAJORITY_NODE1_MASKED, "F3 expected Node 1 masked");
    TEST_ASSERT(r3.final_pwm == 1515, "F3 expected 1515, got %d", r3.final_pwm);

    /* Frame 4: Node 2 SEU */
    voter_result_t r4 = vote_2oo3(1476, 1220, 1476);
    TEST_ASSERT(r4.status == VOTE_MAJORITY_NODE2_MASKED, "F4 expected Node 2 masked");
    TEST_ASSERT(r4.final_pwm == 1476, "F4 expected 1476, got %d", r4.final_pwm);

    /* Frame 5: Node 3 SEU */
    voter_result_t r5 = vote_2oo3(1545, 1545, 1000);
    TEST_ASSERT(r5.status == VOTE_MAJORITY_NODE3_MASKED, "F5 expected Node 3 masked");
    TEST_ASSERT(r5.final_pwm == 1545, "F5 expected 1545, got %d", r5.final_pwm);

    /* Frame 6: Total disagreement */
    voter_result_t r6 = vote_2oo3(1629, 1429, 1769);
    TEST_ASSERT(r6.status == VOTE_TOTAL_DISAGREEMENT, "F6 expected total disagreement");
    TEST_ASSERT(r6.final_pwm == FAIL_SAFE_VALUE, "F6 expected failsafe");
}

/* T-VOTE-002: Chain Case Tests */
static void test_chain_case(void) {
    printf("[RUN] T-VOTE-002: Chain case test\n");
    
    /* Case A: p12 (d12=3) and p23 (d23=4), p12 is closer -> Node 3 masked */
    voter_result_t r1 = vote_2oo3(1500, 1503, 1507);
    TEST_ASSERT(r1.diff12 == 3, "d12 expected 3");
    TEST_ASSERT(r1.diff23 == 4, "d23 expected 4");
    TEST_ASSERT(r1.diff13 == 7, "d13 expected 7");
    TEST_ASSERT(r1.status == VOTE_MAJORITY_NODE3_MASKED, "expected Node 3 masked");
    TEST_ASSERT(r1.final_pwm == 1503, "expected median 1503, got %d", r1.final_pwm);

    /* Case B: p13 (d13=3) and p23 (d23=4), p13 is closer -> Node 2 masked */
    voter_result_t r2 = vote_2oo3(1500, 1507, 1503);
    TEST_ASSERT(r2.diff13 == 3, "d13 expected 3");
    TEST_ASSERT(r2.diff23 == 4, "d23 expected 4");
    TEST_ASSERT(r2.diff12 == 7, "d12 expected 7");
    TEST_ASSERT(r2.status == VOTE_MAJORITY_NODE2_MASKED, "expected Node 2 masked");
    TEST_ASSERT(r2.final_pwm == 1503, "expected median 1503, got %d", r2.final_pwm);

    /* Case C: p23 (d23=3) and p13 (d13=4), p23 is closer -> Node 1 masked */
    voter_result_t r3 = vote_2oo3(1507, 1500, 1503);
    TEST_ASSERT(r3.diff23 == 3, "d23 expected 3");
    TEST_ASSERT(r3.diff13 == 4, "d13 expected 4");
    TEST_ASSERT(r3.status == VOTE_MAJORITY_NODE1_MASKED, "expected Node 1 masked");
    TEST_ASSERT(r3.final_pwm == 1503, "expected median 1503, got %d", r3.final_pwm);
}

/* T-VOTE-003: Permutation Symmetry */
static void test_permutation_symmetry(void) {
    printf("[RUN] T-VOTE-003: Permutation symmetry\n");
    int32_t triples[][3] = {
        {1500, 1502, 1504},
        {1500, 1500, 1800},
        {1200, 1500, 1500},
        {1500, 1100, 1500},
        {1000, 1500, 2000},
    };

    for (size_t i = 0; i < sizeof(triples) / sizeof(triples[0]); i++) {
        int32_t a = triples[i][0];
        int32_t b = triples[i][1];
        int32_t c = triples[i][2];

        voter_result_t r_abc = vote_2oo3(a, b, c);
        voter_result_t r_acb = vote_2oo3(a, c, b);
        voter_result_t r_bac = vote_2oo3(b, a, c);
        voter_result_t r_bca = vote_2oo3(b, c, a);
        voter_result_t r_cab = vote_2oo3(c, a, b);
        voter_result_t r_cba = vote_2oo3(c, b, a);

        TEST_ASSERT(r_abc.final_pwm == r_acb.final_pwm &&
                    r_abc.final_pwm == r_bac.final_pwm &&
                    r_abc.final_pwm == r_bca.final_pwm &&
                    r_abc.final_pwm == r_cab.final_pwm &&
                    r_abc.final_pwm == r_cba.final_pwm,
                    "Permutation symmetry failed for triple {%d, %d, %d}", a, b, c);
    }
}

/* T-VOTE-004: Exhaustive Bounded Grid */
static void test_exhaustive_grid(void) {
    printf("[RUN] T-VOTE-004: Exhaustive bounded grid (1500 +/- 12)\n");
    int count = 0;
    for (int32_t y1 = 1488; y1 <= 1512; y1++) {
        for (int32_t y2 = 1488; y2 <= 1512; y2++) {
            for (int32_t y3 = 1488; y3 <= 1512; y3++) {
                voter_result_t act = vote_2oo3(y1, y2, y3);
                ref_result_t ref = ref_vote_2oo3(y1, y2, y3);

                TEST_ASSERT(act.status == ref.status,
                    "Grid mismatch at {%d, %d, %d}: status %d vs ref %d",
                    y1, y2, y3, act.status, ref.status);
                TEST_ASSERT(act.final_pwm == ref.pwm,
                    "Grid mismatch at {%d, %d, %d}: pwm %d vs ref %d",
                    y1, y2, y3, act.final_pwm, ref.pwm);
                count++;
            }
        }
    }
    printf("       Verified %d grid triples against reference model\n", count);
}

/* T-VOTE-005: Boundary & Randomized Stress */
static void test_boundary_and_stress(void) {
    printf("[RUN] T-VOTE-005: Boundary and randomized stress testing\n");

    int32_t boundaries[] = {
        INT32_MIN, INT32_MIN + 1, -1000000, -1, 0, 1,
        1000, 1495, 1500, 1505, 2000, 1000000,
        INT32_MAX - 1, INT32_MAX
    };
    size_t nb = sizeof(boundaries) / sizeof(boundaries[0]);

    for (size_t i = 0; i < nb; i++) {
        for (size_t j = 0; j < nb; j++) {
            for (size_t k = 0; k < nb; k++) {
                int32_t y1 = boundaries[i];
                int32_t y2 = boundaries[j];
                int32_t y3 = boundaries[k];
                voter_result_t act = vote_2oo3(y1, y2, y3);
                ref_result_t ref = ref_vote_2oo3(y1, y2, y3);
                TEST_ASSERT(act.status == ref.status, "Boundary status mismatch");
                TEST_ASSERT(act.final_pwm == ref.pwm, "Boundary pwm mismatch");
            }
        }
    }

    uint32_t seed = 0x51F7C001U;
    for (int t = 0; t < 100000; t++) {
        seed = seed * 1664525U + 1013904223U;
        int32_t y1 = (int32_t)seed;
        seed = seed * 1664525U + 1013904223U;
        int32_t y2 = (int32_t)seed;
        seed = seed * 1664525U + 1013904223U;
        int32_t y3 = (int32_t)seed;

        voter_result_t act = vote_2oo3(y1, y2, y3);
        ref_result_t ref = ref_vote_2oo3(y1, y2, y3);
        TEST_ASSERT(act.status == ref.status, "PRNG stress status mismatch");
        TEST_ASSERT(act.final_pwm == ref.pwm, "PRNG stress pwm mismatch");
    }
}

/* T-VOTE-006: Decision Tree 1 Plausibility, Sequence, and Health Integration */
static void test_tree1_and_tree2_integration(void) {
    printf("[RUN] T-VOTE-006: Tree 1 & Tree 2 full voting engine integration\n");
    node_health_init();
    voter_reset_rate_limit(1500);

    /* 1. Nominal 3-node frame */
    node_sample_t s1[3] = {
        { .pwm_us = 1520, .frame_id = 1, .valid = true },
        { .pwm_us = 1522, .frame_id = 1, .valid = true },
        { .pwm_us = 1521, .frame_id = 1, .valid = true }
    };
    voter_result_t r1 = vote_frame_inputs(s1, 1);
    TEST_ASSERT(r1.status == VOTE_UNANIMOUS, "Expected unanimous");
    TEST_ASSERT(r1.final_pwm == 1521, "Expected median 1521");

    /* 2. Plausibility violation on Node 2 (> 2000 us) */
    node_sample_t s2[3] = {
        { .pwm_us = 1530, .frame_id = 2, .valid = true },
        { .pwm_us = 2500, .frame_id = 2, .valid = true }, /* Out of bounds */
        { .pwm_us = 1532, .frame_id = 2, .valid = true }
    };
    voter_result_t r2 = vote_frame_inputs(s2, 2);
    TEST_ASSERT(r2.status == VOTE_DEGRADED_2OO2, "Expected degraded 2oo2");
    TEST_ASSERT(r2.final_pwm == 1531, "Expected (1530+1532)/2 = 1531");

    /* 3. Sequence token mismatch on Node 3 */
    node_sample_t s3[3] = {
        { .pwm_us = 1540, .frame_id = 3, .valid = true },
        { .pwm_us = 1542, .frame_id = 3, .valid = true },
        { .pwm_us = 1541, .frame_id = 1, .valid = true } /* Stale frame_id */
    };
    voter_result_t r3 = vote_frame_inputs(s3, 3);
    TEST_ASSERT(r3.status == VOTE_DEGRADED_2OO2, "Expected degraded 2oo2");
    TEST_ASSERT(r3.final_pwm == 1541, "Expected 1541");

    /* 4. Degraded 2oo2 Disagreement -> Cannot Arbitrate */
    node_sample_t s4[3] = {
        { .pwm_us = 1500, .frame_id = 4, .valid = true },
        { .pwm_us = 1600, .frame_id = 4, .valid = true }, /* delta = 100 > 5 */
        { .pwm_us = 1500, .frame_id = 0, .valid = false } /* Invalid */
    };
    voter_result_t r4 = vote_frame_inputs(s4, 4);
    TEST_ASSERT(r4.status == VOTE_CANNOT_ARBITRATE, "Expected cannot arbitrate");
    TEST_ASSERT(r4.final_pwm == FAIL_SAFE_VALUE, "Expected failsafe");

    /* 5. Insufficient nodes (2 invalid) */
    node_sample_t s5[3] = {
        { .pwm_us = 1500, .frame_id = 5, .valid = true },
        { .pwm_us = 1500, .frame_id = 0, .valid = false },
        { .pwm_us = 1500, .frame_id = 0, .valid = false }
    };
    voter_result_t r5 = vote_frame_inputs(s5, 5);
    TEST_ASSERT(r5.status == VOTE_INSUFFICIENT_NODES, "Expected insufficient nodes");
    TEST_ASSERT(r5.final_pwm == FAIL_SAFE_VALUE, "Expected failsafe");
}

/* T-VOTE-007: Decision Tree 2 Leaky-Bucket Health Accounting & Latching */
static void test_tree2_health_latching(void) {
    printf("[RUN] T-VOTE-007: Tree 2 leaky-bucket health and latching logic\n");
    node_health_init();

    /* Node 1: Accumulate 3 faults -> latched */
    TEST_ASSERT(!node_health_is_latched(1), "Node 1 should start healthy");
    node_health_record_fault(1);
    TEST_ASSERT(!node_health_is_latched(1), "Node 1 should not latch at fault 1");
    node_health_record_fault(1);
    TEST_ASSERT(!node_health_is_latched(1), "Node 1 should not latch at fault 2");
    node_health_record_fault(1);
    TEST_ASSERT(node_health_is_latched(1), "Node 1 should be latched at fault 3");

    /* Successes after latching do not unlatch */
    node_health_record_success(1);
    TEST_ASSERT(node_health_is_latched(1), "Node 1 should remain latched");

    /* Node 2: 2 faults, then 100 successes -> fault counter decays */
    node_health_record_fault(2);
    node_health_record_fault(2);
    node_health_t h2 = node_health_get(2);
    TEST_ASSERT(h2.fault_count == 2, "Fault count should be 2");

    for (int i = 0; i < 99; i++) {
        node_health_record_success(2);
    }
    h2 = node_health_get(2);
    TEST_ASSERT(h2.fault_count == 2, "Fault count should still be 2 at streak 99");

    node_health_record_success(2); /* 100th success */
    h2 = node_health_get(2);
    TEST_ASSERT(h2.fault_count == 1, "Fault count should decay to 1 at streak 100");
    TEST_ASSERT(h2.good_streak == 0, "Streak should reset to 0");

    /* Node health reset */
    node_health_reset(1);
    TEST_ASSERT(!node_health_is_latched(1), "Node 1 should be unlatched after reset");

    /* Out of bounds node id checks */
    TEST_ASSERT(node_health_is_latched(0), "Node 0 is latched/invalid");
    TEST_ASSERT(node_health_is_latched(4), "Node 4 is latched/invalid");
    node_health_record_fault(0);
    node_health_record_fault(99);
    node_health_record_success(0);
    node_health_reset(99);
}

/* T-VOTE-008: Actuator Rate Limiter */
static void test_rate_limiter(void) {
    printf("[RUN] T-VOTE-008: Actuator rate limiter (PWM_MAX_STEP_US = 200)\n");
    voter_reset_rate_limit(1500);

    /* Step +50 us: Allowed */
    int32_t out1 = voter_apply_rate_limit(1550);
    TEST_ASSERT(out1 == 1550, "Expected 1550, got %d", out1);

    /* Step +300 us: Clamped to 1550 + 200 = 1750 */
    int32_t out2 = voter_apply_rate_limit(1850);
    TEST_ASSERT(out2 == 1750, "Expected 1750, got %d", out2);

    /* Step -500 us: Clamped to 1750 - 200 = 1550 */
    int32_t out3 = voter_apply_rate_limit(1250);
    TEST_ASSERT(out3 == 1550, "Expected 1550, got %d", out3);

    /* Failsafe bypasses rate limiter */
    int32_t out_fs = voter_apply_rate_limit(FAIL_SAFE_VALUE);
    TEST_ASSERT(out_fs == FAIL_SAFE_VALUE, "Expected failsafe bypass");
}

/* T-VOTE-009: String Conversions */
static void test_status_strings(void) {
    printf("[RUN] T-VOTE-009: Telemetry status string coverage\n");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_UNANIMOUS), "UNANIMOUS") != NULL, "Unanimous string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_MAJORITY_NODE1_MASKED), "Node 1 Outlier") != NULL, "Node 1 string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_MAJORITY_NODE2_MASKED), "Node 2 Outlier") != NULL, "Node 2 string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_MAJORITY_NODE3_MASKED), "Node 3 Outlier") != NULL, "Node 3 string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_TOTAL_DISAGREEMENT), "Total Disagreement") != NULL, "Total disagreement string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_TIMEOUT_ERROR), "Core Watchdog Timeout") != NULL, "Timeout string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_DEGRADED_2OO2), "DEGRADED 2oo2") != NULL, "2oo2 string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_CANNOT_ARBITRATE), "Cannot Arbitrate") != NULL, "Cannot arbitrate string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_INSUFFICIENT_NODES), "Insufficient Nodes") != NULL, "Insufficient nodes string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_PLAUSIBILITY_FAULT), "Out-of-Bounds") != NULL, "Plausibility string");
    TEST_ASSERT(strstr(vote_status_to_string(VOTE_SEQUENCE_FAULT), "Sequence Token") != NULL, "Sequence string");
    TEST_ASSERT(strstr(vote_status_to_string((vote_status_t)99), "UNKNOWN") != NULL, "Unknown string");
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Voter Test Runner (Native C Unit & Coverage Harness)\n");
    printf("==============================================================================\n");

    test_named_vectors();
    test_chain_case();
    test_permutation_symmetry();
    test_exhaustive_grid();
    test_boundary_and_stress();
    test_tree1_and_tree2_integration();
    test_tree2_health_latching();
    test_rate_limiter();
    test_status_strings();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", g_assertions);
    printf("==============================================================================\n");

    return 0;
}
