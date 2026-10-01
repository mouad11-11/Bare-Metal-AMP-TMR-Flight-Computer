#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "pmu.h"

/* Stubs for UART logging */
void uart_puts(const char *s) { (void)s; }
void uart_printf(const char *fmt, ...) { (void)fmt; }

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_pmu_init_and_cycles(void) {
    printf("[RUN] T-PMU-001: PMU Initialization & Cycle Counter Query\n");
    pmu_init();

    wcet_profile_t prof;
    pmu_get_profile(&prof);
    TEST_ASSERT(prof.total_frame_cycles == 0);
    TEST_ASSERT(prof.max_observed_cycles == 0);

    uint32_t c1 = pmu_get_cycles();
    uint32_t c2 = pmu_get_cycles();
    TEST_ASSERT(c2 > c1);
}

static void test_pmu_record_timing(void) {
    printf("[RUN] T-PMU-002: Frame Segment Timing & Maximum Bound\n");
    pmu_init();

    /* Nominal sequence: t_start=1000, ingest=1200, sync=3000, voter=3500, end=4000 */
    pmu_record_frame_timing(1000, 1200, 3000, 3500, 4000);

    wcet_profile_t prof;
    pmu_get_profile(&prof);
    TEST_ASSERT(prof.ingest_cycles == 200);
    TEST_ASSERT(prof.compute_sync_cycles == 1800);
    TEST_ASSERT(prof.voter_lockstep_cycles == 500);
    TEST_ASSERT(prof.telemetry_cycles == 500);
    TEST_ASSERT(prof.total_frame_cycles == 3000);
    TEST_ASSERT(prof.max_observed_cycles == 3000);

    /* Second smaller frame: max_observed_cycles should remain 3000 */
    pmu_record_frame_timing(1000, 1100, 2000, 2200, 2500);
    pmu_get_profile(&prof);
    TEST_ASSERT(prof.total_frame_cycles == 1500);
    TEST_ASSERT(prof.max_observed_cycles == 3000);

    /* Defensive wrap-around / non-monotonic timestamps */
    pmu_record_frame_timing(5000, 4000, 3000, 2000, 1000);
    pmu_get_profile(&prof);
    TEST_ASSERT(prof.ingest_cycles == 0);
    TEST_ASSERT(prof.compute_sync_cycles == 0);
    TEST_ASSERT(prof.voter_lockstep_cycles == 0);
    TEST_ASSERT(prof.telemetry_cycles == 0);
}

static void test_pmu_telemetry_reporting(void) {
    printf("[RUN] T-PMU-003: WCET Margin Reporting\n");
    pmu_init();

    /* Within budget (margin > 0) */
    pmu_record_frame_timing(0, 5000, 50000, 60000, 100000);
    pmu_print_telemetry();

    /* Exceeds budget (margin = 0) */
    pmu_record_frame_timing(0, 100000, 300000, 400000, 600000);
    pmu_print_telemetry();

    /* NULL pointer safety */
    pmu_get_profile(NULL);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host PMU & WCET Profiler Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_pmu_init_and_cycles();
    test_pmu_record_timing();
    test_pmu_telemetry_reporting();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
