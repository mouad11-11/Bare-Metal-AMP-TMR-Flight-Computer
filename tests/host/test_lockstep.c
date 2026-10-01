#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "lockstep.h"
#include "voter.h"
#include "failsafe.h"
#include "node_health.h"

/* Host test stubs for embedded symbols */
volatile uint32_t core_ready[4] = {0, 0, 0, 0};
volatile uint32_t core_done[4] = {0, 0, 0, 0};
volatile uint32_t g_cycle_counter = 0;
void uart_printf(const char *fmt, ...) { (void)fmt; }

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_nominal_lockstep_scenarios(void) {
    printf("[RUN] T-LOCK-001: Nominal Dual-Rail Consensus Matching\n");
    lockstep_init();
    node_health_init();
    failsafe_init();

    /* 1. Unanimous consensus */
    voter_result_t r1 = vote_2oo3(1500, 1500, 1500);
    TEST_ASSERT(lockstep_verify_voter(1500, 1500, 1500, &r1) == true);
    TEST_ASSERT(r1.final_pwm == 1500);

    /* 2. Majority Node 1 outlier */
    voter_result_t r2 = vote_2oo3(1800, 1510, 1512);
    TEST_ASSERT(lockstep_verify_voter(1800, 1510, 1512, &r2) == true);
    TEST_ASSERT(r2.final_pwm == 1511);

    /* 3. Majority Node 2 outlier */
    voter_result_t r3 = vote_2oo3(1510, 1200, 1512);
    TEST_ASSERT(lockstep_verify_voter(1510, 1200, 1512, &r3) == true);
    TEST_ASSERT(r3.final_pwm == 1511);

    /* 4. Majority Node 3 outlier */
    voter_result_t r4 = vote_2oo3(1510, 1512, 1900);
    TEST_ASSERT(lockstep_verify_voter(1510, 1512, 1900, &r4) == true);
    TEST_ASSERT(r4.final_pwm == 1511);

    /* 5. Total Disagreement */
    voter_result_t r5 = vote_2oo3(1200, 1500, 1800);
    TEST_ASSERT(lockstep_verify_voter(1200, 1500, 1800, &r5) == true);
    TEST_ASSERT(r5.final_pwm == FAIL_SAFE_VALUE);

    /* Other status pass-through (e.g. timeout) */
    voter_result_t rt = { .status = VOTE_TIMEOUT_ERROR, .final_pwm = FAIL_SAFE_VALUE, .diff12 = 0, .diff23 = 0, .diff13 = 0 };
    TEST_ASSERT(lockstep_verify_voter(1500, 1500, 1500, &rt) == true);
}

static void test_divergence_detection(void) {
    printf("[RUN] T-LOCK-002: Internal SEU / ALU Divergence Detection\n");
    lockstep_init();
    failsafe_init();

    /* Corrupted pairwise diff */
    voter_result_t bad_diff = vote_2oo3(1500, 1500, 1500);
    bad_diff.diff12 = 999; /* Injected ALU bit-flip */
    TEST_ASSERT(lockstep_verify_voter(1500, 1500, 1500, &bad_diff) == false);
    TEST_ASSERT(bad_diff.final_pwm == FAIL_SAFE_VALUE);
    TEST_ASSERT(failsafe_is_latched() == true);
    TEST_ASSERT(failsafe_get_latched_reason() == REASON_INTEGRITY_FAIL);

    /* Corrupted unanimous median */
    failsafe_init();
    voter_result_t bad_median = vote_2oo3(1500, 1502, 1504);
    bad_median.final_pwm = 1509; /* Bit-flip in median result */
    TEST_ASSERT(lockstep_verify_voter(1500, 1502, 1504, &bad_median) == false);

    /* Corrupted unanimous delta check */
    failsafe_init();
    voter_result_t bad_unanimous_delta = vote_2oo3(1500, 1500, 1500);
    bad_unanimous_delta.status = VOTE_UNANIMOUS;
    bad_unanimous_delta.diff12 = 100;
    TEST_ASSERT(lockstep_verify_voter(1500, 1500, 1500, &bad_unanimous_delta) == false);

    /* Corrupted majority average */
    failsafe_init();
    voter_result_t bad_maj = vote_2oo3(1510, 1512, 1900);
    bad_maj.final_pwm = 1600; /* Corrupted pair average */
    TEST_ASSERT(lockstep_verify_voter(1510, 1512, 1900, &bad_maj) == false);

    /* Corrupted majority node 1 outlier */
    failsafe_init();
    voter_result_t bad_maj1 = vote_2oo3(1800, 1510, 1512);
    bad_maj1.final_pwm = 1999;
    TEST_ASSERT(lockstep_verify_voter(1800, 1510, 1512, &bad_maj1) == false);

    /* Corrupted majority node 2 outlier */
    failsafe_init();
    voter_result_t bad_maj2 = vote_2oo3(1510, 1200, 1512);
    bad_maj2.final_pwm = 1999;
    TEST_ASSERT(lockstep_verify_voter(1510, 1200, 1512, &bad_maj2) == false);

    /* Invalid majority state */
    failsafe_init();
    voter_result_t bad_maj_state = vote_2oo3(1510, 1512, 1900);
    bad_maj_state.diff12 = 10;
    bad_maj_state.diff23 = 10;
    bad_maj_state.diff13 = 10;
    TEST_ASSERT(lockstep_verify_voter(1500, 1510, 1520, &bad_maj_state) == false);

    /* Corrupted disagreement fail-safe command */
    failsafe_init();
    voter_result_t bad_disagree = vote_2oo3(1200, 1500, 1800);
    bad_disagree.final_pwm = 1500; /* Should be FAIL_SAFE_VALUE */
    TEST_ASSERT(lockstep_verify_voter(1200, 1500, 1800, &bad_disagree) == false);

    /* NULL res test */
    TEST_ASSERT(lockstep_verify_voter(1500, 1500, 1500, NULL) == false);
}

static void test_diagnostics(void) {
    printf("[RUN] T-LOCK-003: Lockstep Diagnostics Query\n");
    lockstep_diag_t diag;
    lockstep_get_diagnostics(&diag);
    TEST_ASSERT(diag.failure_count > 0);
    lockstep_get_diagnostics(NULL); /* Defensive NULL test */
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Dual-Rail Lockstep Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_nominal_lockstep_scenarios();
    test_divergence_detection();
    test_diagnostics();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
