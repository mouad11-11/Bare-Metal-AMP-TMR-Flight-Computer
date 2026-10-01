#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "supervision.h"
#include "node_health.h"

static uint32_t g_assertions = 0;

#define TEST_ASSERT(cond, msg, ...) do { \
    g_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(1); \
    } \
} while (0)

static void test_initialization_and_nominal_flow(void) {
    printf("[RUN] T-SUP-001: Nominal CFI checkpoint progression & heartbeat\n");
    supervision_init();
    node_health_init();

    supervision_frame_start(1);

    /* Advance cores 1, 2, 3 through complete CFI pipeline */
    for (uint32_t c = 1; c <= 3; c++) {
        supervision_checkpoint(c, CFI_TOKEN_READ_INPUT);
        supervision_checkpoint(c, CFI_TOKEN_COMPUTE);
        supervision_checkpoint(c, CFI_TOKEN_WRITE_OUTPUT);
        supervision_checkpoint(c, CFI_TOKEN_CANARY_CHECK);
        supervision_checkpoint(c, CFI_TOKEN_COMPLETE);
    }

    uint32_t done_mask = (1 << 1) | (1 << 2) | (1 << 3);
    uint32_t fault_mask = 0;
    bool ok = supervision_evaluate_nodes(done_mask, &fault_mask);

    TEST_ASSERT(ok == true, "Expected all nodes healthy");
    TEST_ASSERT(fault_mask == 0, "Fault mask should be 0");

    for (uint32_t c = 1; c <= 3; c++) {
        node_supervision_t s = supervision_get_node(c);
        TEST_ASSERT(s.heartbeat_counter == 1, "Heartbeat should be 1");
        TEST_ASSERT(s.consecutive_misses == 0, "Misses should be 0");
        TEST_ASSERT(!s.is_timed_out, "Should not be timed out");
        TEST_ASSERT(!supervision_is_node_dead(c), "Node should be alive");
    }
}

static void test_cfi_divergence(void) {
    printf("[RUN] T-SUP-002: CFI signature divergence detection\n");
    supervision_init();
    node_health_init();

    supervision_frame_start(2);

    /* Core 1: Normal */
    supervision_checkpoint(1, CFI_TOKEN_READ_INPUT);
    supervision_checkpoint(1, CFI_TOKEN_COMPUTE);
    supervision_checkpoint(1, CFI_TOKEN_WRITE_OUTPUT);
    supervision_checkpoint(1, CFI_TOKEN_CANARY_CHECK);
    supervision_checkpoint(1, CFI_TOKEN_COMPLETE);

    /* Core 2: Illegally jumps from READ_INPUT directly to COMPLETE (bypassing compute) */
    supervision_checkpoint(2, CFI_TOKEN_READ_INPUT);
    supervision_checkpoint(2, CFI_TOKEN_COMPLETE); /* Invalid transition */

    /* Core 3: Normal */
    supervision_checkpoint(3, CFI_TOKEN_READ_INPUT);
    supervision_checkpoint(3, CFI_TOKEN_COMPUTE);
    supervision_checkpoint(3, CFI_TOKEN_WRITE_OUTPUT);
    supervision_checkpoint(3, CFI_TOKEN_CANARY_CHECK);
    supervision_checkpoint(3, CFI_TOKEN_COMPLETE);

    uint32_t done_mask = (1 << 1) | (1 << 2) | (1 << 3);
    uint32_t fault_mask = 0;
    bool ok = supervision_evaluate_nodes(done_mask, &fault_mask);

    TEST_ASSERT(ok == false, "Expected evaluation failure due to Core 2 CFI divergence");
    TEST_ASSERT(fault_mask == (1 << 2), "Fault mask should isolate Core 2");

    node_supervision_t s2 = supervision_get_node(2);
    TEST_ASSERT(s2.is_timed_out == true, "Core 2 should be flagged as timed out / fault");
}

static void test_consecutive_misses_and_dead_latch(void) {
    printf("[RUN] T-SUP-003: Consecutive missed deadlines (NODE_MISS_LIMIT_K = 2)\n");
    supervision_init();
    node_health_init();

    /* Frame 1: Core 3 misses deadline (done_mask missing bit 3) */
    supervision_frame_start(1);
    for (uint32_t c = 1; c <= 2; c++) {
        supervision_checkpoint(c, CFI_TOKEN_READ_INPUT);
        supervision_checkpoint(c, CFI_TOKEN_COMPUTE);
        supervision_checkpoint(c, CFI_TOKEN_WRITE_OUTPUT);
        supervision_checkpoint(c, CFI_TOKEN_CANARY_CHECK);
        supervision_checkpoint(c, CFI_TOKEN_COMPLETE);
    }
    uint32_t done_mask = (1 << 1) | (1 << 2);
    uint32_t fault_mask = 0;
    supervision_evaluate_nodes(done_mask, &fault_mask);

    TEST_ASSERT(fault_mask == (1 << 3), "Core 3 should be faulted");
    TEST_ASSERT(!supervision_is_node_dead(3), "Core 3 should not be dead after 1 miss");
    TEST_ASSERT(!node_health_is_latched(3), "Core 3 should not be health-latched yet");

    /* Frame 2: Core 3 misses deadline again */
    supervision_frame_start(2);
    for (uint32_t c = 1; c <= 2; c++) {
        supervision_checkpoint(c, CFI_TOKEN_READ_INPUT);
        supervision_checkpoint(c, CFI_TOKEN_COMPUTE);
        supervision_checkpoint(c, CFI_TOKEN_WRITE_OUTPUT);
        supervision_checkpoint(c, CFI_TOKEN_CANARY_CHECK);
        supervision_checkpoint(c, CFI_TOKEN_COMPLETE);
    }
    supervision_evaluate_nodes(done_mask, &fault_mask);

    TEST_ASSERT(supervision_is_node_dead(3), "Core 3 should be declared dead after 2 misses");
}

static void test_bounds(void) {
    printf("[RUN] T-SUP-004: Boundary conditions and invalid core handling\n");
    supervision_checkpoint(0, CFI_TOKEN_READ_INPUT);
    supervision_checkpoint(4, CFI_TOKEN_READ_INPUT);

    node_supervision_t s0 = supervision_get_node(0);
    TEST_ASSERT(s0.is_timed_out == true, "Invalid core 0 returns timed out");

    TEST_ASSERT(supervision_is_node_dead(0) == true, "Core 0 considered dead/invalid");
    TEST_ASSERT(supervision_is_node_dead(4) == true, "Core 4 considered dead/invalid");
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Supervision Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_initialization_and_nominal_flow();
    test_cfi_divergence();
    test_consecutive_misses_and_dead_latch();
    test_bounds();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", g_assertions);
    printf("==============================================================================\n");

    return 0;
}
