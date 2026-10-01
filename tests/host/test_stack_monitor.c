#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "stack_monitor.h"

static uint32_t g_assertions = 0;

#define TEST_ASSERT(cond) do { \
    g_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: assertion '%s' failed\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

extern uint8_t host_stack_memory[];

static void test_initialization_and_canaries(void) {
    printf("[RUN] T-STACK-001: Stack canary initialization and baseline integrity\n");
    stack_monitor_init();

    for (uint32_t c = 0; c < 4; c++) {
        TEST_ASSERT(stack_canary_check_core(c) == true);
    }
    TEST_ASSERT(stack_canary_check_all() == true);
}

static void test_canary_corruption_detection(void) {
    printf("[RUN] T-STACK-002: Canary corruption detection\n");
    stack_monitor_init();

    /* Intentionally corrupt partition canary on Core 1 */
    /* Core 1 partition bottom = host_stack_memory + STACK_TOTAL_SIZE - 2 * STACK_PER_CORE_SIZE */
    uint32_t c1_bottom_offset = STACK_TOTAL_SIZE - (2 * STACK_PER_CORE_SIZE);
    volatile uint32_t *c1_canary = (volatile uint32_t *)&host_stack_memory[c1_bottom_offset];
    c1_canary[0] = 0x00000000U;

    TEST_ASSERT(stack_canary_check_core(1) == false);
    TEST_ASSERT(stack_canary_check_all() == false);
    /* Other cores should still be valid */
    TEST_ASSERT(stack_canary_check_core(0) == true);
    TEST_ASSERT(stack_canary_check_core(2) == true);
    TEST_ASSERT(stack_canary_check_core(3) == true);

    /* Restore and recheck */
    c1_canary[0] = STACK_CANARY_VALUE;
    TEST_ASSERT(stack_canary_check_core(1) == true);
    TEST_ASSERT(stack_canary_check_all() == true);

    /* Corrupt SVC stack canary on Core 2 */
    /* Core 2 top = host_stack_memory + STACK_TOTAL_SIZE - 2 * STACK_PER_CORE_SIZE */
    /* Core 2 SVC bottom = Core 2 top - STACK_SVC_SIZE */
    uint32_t c2_svc_offset = (STACK_TOTAL_SIZE - (2 * STACK_PER_CORE_SIZE)) - STACK_SVC_SIZE;
    volatile uint32_t *c2_svc_canary = (volatile uint32_t *)&host_stack_memory[c2_svc_offset];
    c2_svc_canary[2] = 0xBAADF00DU;

    TEST_ASSERT(stack_canary_check_core(2) == false);
    TEST_ASSERT(stack_canary_check_all() == false);

    /* Invalid core id bounds check */
    TEST_ASSERT(stack_canary_check_core(4) == false);
    TEST_ASSERT(stack_canary_check_core(99) == false);
}

static void test_high_water_mark(void) {
    printf("[RUN] T-STACK-003: Stack high-water mark computation\n");
    stack_monitor_init();

    /* High water mark initially on clean core */
    uint32_t used_c3 = stack_get_high_water_mark(3);
    TEST_ASSERT(used_c3 == 0);
    uint32_t headroom_c3 = stack_get_headroom(3);
    TEST_ASSERT(headroom_c3 == (STACK_SVC_SIZE - (STACK_CANARY_WORDS * 4U)));

    /* Simulate stack usage by writing non-watermark data into Core 3 stack */
    /* Core 3 partition top = host_stack_memory + STACK_PER_CORE_SIZE */
    uint32_t c3_top_offset = STACK_PER_CORE_SIZE;
    volatile uint32_t *simulated_stack_frame = (volatile uint32_t *)&host_stack_memory[c3_top_offset - 128];
    *simulated_stack_frame = 0x12345678U;

    uint32_t measured_used = stack_get_high_water_mark(3);
    TEST_ASSERT(measured_used >= 128);

    /* Invalid core checks */
    TEST_ASSERT(stack_get_high_water_mark(4) == 0);
    TEST_ASSERT(stack_get_headroom(4) == 0);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Stack Monitor Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_initialization_and_canaries();
    test_canary_corruption_detection();
    test_high_water_mark();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", g_assertions);
    printf("==============================================================================\n");

    return 0;
}
