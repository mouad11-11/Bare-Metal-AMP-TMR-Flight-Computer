#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "voter.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg, ...) do { \
    g_tests_run++; \
    if (!(cond)) { \
        g_tests_failed++; \
        fprintf(stderr, "[FAIL] %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
} while(0)

/* Independent reference model for voter */
static int64_t ref_diff(int32_t a, int32_t b) {
    int64_t d = (int64_t)a - (int64_t)b;
    return (d < 0) ? -d : d;
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
        r.pwm = (int32_t)(((int64_t)y1 + y2 + y3) / 3);
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
            r.pwm = (int32_t)(((int64_t)y1 + y2) / 2);
        } else if (p13 && (d13 <= d12 && d13 <= d23)) {
            r.status = VOTE_MAJORITY_NODE2_MASKED;
            r.pwm = (int32_t)(((int64_t)y1 + y3) / 2);
        } else {
            r.status = VOTE_MAJORITY_NODE1_MASKED;
            r.pwm = (int32_t)(((int64_t)y2 + y3) / 2);
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

    /* Frame 2: Bounded noise */
    voter_result_t r2 = vote_2oo3(1538, 1533, 1537);
    TEST_ASSERT(r2.status == VOTE_UNANIMOUS, "F2 expected unanimous");
    TEST_ASSERT(r2.final_pwm == 1536, "F2 expected 1536, got %d", r2.final_pwm);

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
    TEST_ASSERT(r6.final_pwm == FAIL_SAFE_VALUE, "F6 expected fail safe, got %d", r6.final_pwm);

    /* Strings conversion check */
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_UNANIMOUS), "UNANIMOUS (All Nodes Agree)") == 0, "status string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_MAJORITY_NODE1_MASKED), "MAJORITY 2oo3 (Node 1 Outlier Masked)") == 0, "node 1 string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_MAJORITY_NODE2_MASKED), "MAJORITY 2oo3 (Node 2 Outlier Masked)") == 0, "node 2 string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_MAJORITY_NODE3_MASKED), "MAJORITY 2oo3 (Node 3 Outlier Masked)") == 0, "node 3 string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_TOTAL_DISAGREEMENT), "FAIL-SAFE ACTIVATED (Total Disagreement)") == 0, "total disagree string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string(VOTE_TIMEOUT_ERROR), "FAIL-SAFE ACTIVATED (Core Watchdog Timeout)") == 0, "timeout string mismatch");
    TEST_ASSERT(strcmp(vote_status_to_string((vote_status_t)999), "UNKNOWN STATUS") == 0, "unknown string mismatch");
}

/* T-VOTE-002: Chain Case (d12 <= 5, d23 <= 5, d13 > 5) */
static void test_chain_case(void) {
    printf("[RUN] T-VOTE-002: Chain case test\n");
    /* y1 = 1500, y2 = 1505, y3 = 1510 -> d12=5, d23=5, d13=10 */
    voter_result_t r = vote_2oo3(1500, 1505, 1510);
    /* In current voter.c, d12 <= d13, so pair 12 is chosen */
    TEST_ASSERT(r.status == VOTE_MAJORITY_NODE3_MASKED, "Chain case node 3 masked expected");
    TEST_ASSERT(r.final_pwm == 1502, "Chain case expected 1502, got %d", r.final_pwm);
}

/* T-VOTE-003: Permutation Symmetry */
static void test_permutation_symmetry(void) {
    printf("[RUN] T-VOTE-003: Permutation symmetry\n");
    int32_t triplets[][3] = {
        {1500, 1500, 1500},
        {1500, 1504, 1502},
        {1000, 1500, 1502}, /* outlier */
        {1500, 1000, 1502},
        {1500, 1502, 1000},
        {1200, 1400, 1800}  /* total disagreement */
    };
    int num_triplets = sizeof(triplets) / sizeof(triplets[0]);

    for (int t = 0; t < num_triplets; t++) {
        int32_t a = triplets[t][0], b = triplets[t][1], c = triplets[t][2];
        voter_result_t base = vote_2oo3(a, b, c);

        /* All 6 permutations */
        int32_t p[6][3] = {
            {a, b, c}, {a, c, b},
            {b, a, c}, {b, c, a},
            {c, a, b}, {c, b, a}
        };

        for (int i = 0; i < 6; i++) {
            voter_result_t perm = vote_2oo3(p[i][0], p[i][1], p[i][2]);
            /* Output PWM must be identical across all permutations */
            TEST_ASSERT(perm.final_pwm == base.final_pwm,
                        "Symmetry violated for (%d,%d,%d): base=%d, perm(%d,%d,%d)=%d",
                        a, b, c, base.final_pwm, p[i][0], p[i][1], p[i][2], perm.final_pwm);
        }
    }
}

/* T-VOTE-004: Bounded Grid Exhaustive Test (all triples in 1500 +/- 12) */
static void test_bounded_grid_exhaustive(void) {
    printf("[RUN] T-VOTE-004: Exhaustive bounded grid (1500 +/- 12)\n");
    int range = 12;
    int base = 1500;
    long count = 0;

    for (int o1 = -range; o1 <= range; o1++) {
        for (int o2 = -range; o2 <= range; o2++) {
            for (int o3 = -range; o3 <= range; o3++) {
                int32_t y1 = base + o1;
                int32_t y2 = base + o2;
                int32_t y3 = base + o3;

                voter_result_t act = vote_2oo3(y1, y2, y3);
                ref_result_t exp = ref_vote_2oo3(y1, y2, y3);

                TEST_ASSERT(act.final_pwm == exp.pwm && act.status == exp.status,
                            "Mismatch at (%d, %d, %d): act=(%d, %d), exp=(%d, %d)",
                            y1, y2, y3, act.final_pwm, act.status, exp.pwm, exp.status);
                count++;
            }
        }
    }
    printf("       Verified %ld grid triples against reference model\n", count);
}

/* T-VOTE-005: Randomized & Boundary Tests (including INT32_MIN / INT32_MAX) */
static void test_boundary_and_randomized(void) {
    printf("[RUN] T-VOTE-005: Boundary and randomized stress testing\n");

    /* Boundary vectors */
    int32_t extremes[] = {INT32_MIN, INT32_MIN + 1, -1000000, -1, 0, 1, 1000000, INT32_MAX - 1, INT32_MAX};
    int num_ext = sizeof(extremes) / sizeof(extremes[0]);

    for (int i = 0; i < num_ext; i++) {
        for (int j = 0; j < num_ext; j++) {
            for (int k = 0; k < num_ext; k++) {
                int32_t y1 = extremes[i];
                int32_t y2 = extremes[j];
                int32_t y3 = extremes[k];

                voter_result_t act = vote_2oo3(y1, y2, y3);
                ref_result_t exp = ref_vote_2oo3(y1, y2, y3);

                TEST_ASSERT(act.final_pwm == exp.pwm && act.status == exp.status,
                            "Boundary mismatch at (%d, %d, %d): act=(%d, %d), exp=(%d, %d)",
                            y1, y2, y3, act.final_pwm, act.status, exp.pwm, exp.status);
            }
        }
    }

    /* Deterministic randomized testing */
    uint32_t seed = 0x51F7C001U;
    printf("       Deterministic PRNG seed: 0x%08X\n", seed);
    srand(seed);

    for (int iter = 0; iter < 100000; iter++) {
        int32_t y1 = (int32_t)(((uint32_t)rand() << 16) | (uint32_t)rand());
        int32_t y2 = (int32_t)(((uint32_t)rand() << 16) | (uint32_t)rand());
        int32_t y3 = (int32_t)(((uint32_t)rand() << 16) | (uint32_t)rand());

        /* Inject close correlation in 50% of cases */
        if (iter % 2 == 0) {
            int offset = (rand() % 11) - 5;
            y2 = y1 + offset;
            if (iter % 4 == 0) {
                y3 = y1 + (rand() % 11) - 5;
            }
        }

        voter_result_t act = vote_2oo3(y1, y2, y3);
        ref_result_t exp = ref_vote_2oo3(y1, y2, y3);

        TEST_ASSERT(act.final_pwm == exp.pwm && act.status == exp.status,
                    "Random mismatch at (%d, %d, %d): act=(%d, %d), exp=(%d, %d)",
                    y1, y2, y3, act.final_pwm, act.status, exp.pwm, exp.status);
    }
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Voter Test Runner (Native C Unit & Coverage Harness)\n");
    printf("==============================================================================\n");

    test_named_vectors();
    test_chain_case();
    test_permutation_symmetry();
    test_bounded_grid_exhaustive();
    test_boundary_and_randomized();

    printf("==============================================================================\n");
    printf("  Results: %d assertions executed, %d failed\n", g_tests_run, g_tests_failed);
    printf("==============================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
