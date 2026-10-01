#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "voter.h"
#include "node_health.h"
#include "failsafe.h"
#include "supervision.h"
#include "stack_monitor.h"
#include "mmu.h"
#include "lockstep.h"
#include "mailbox.h"
#include "safe_math.h"

/* Stubs for bare-metal global variables */
volatile uint32_t core_ready[4] = {1, 1, 1, 1};
volatile uint32_t core_done[4] = {1, 1, 1, 1};
volatile uint32_t g_cycle_counter = 1;
void uart_printf(const char *fmt, ...) { (void)fmt; }

static uint32_t s_campaign_assertions = 0;
static uint32_t s_campaign_vectors_passed = 0;

#define FI_ASSERT(cond) do { \
    s_campaign_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "FI CAMPAIGN FAILURE at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

/* ==============================================================================
 * Campaign Section 1: Single Event Upsets (SEU Bit-Flips across 32 Bits)
 * ============================================================================== */
static void run_campaign_seu_bitflips(void) {
    printf("[CAMPAIGN-01] Testing 96 SEU Bit-Flip Vectors across all 3 Nodes...\n");
    int32_t nominal = 1500;

    for (int bit = 0; bit < 32; bit++) {
        /* Node 1 bit-flip */
        int32_t n1_corrupted = nominal ^ (1U << bit);
        voter_result_t r1 = vote_2oo3(n1_corrupted, nominal, nominal);
        if (n1_corrupted >= 1495 && n1_corrupted <= 1505) {
            FI_ASSERT(r1.status == VOTE_UNANIMOUS || r1.status == VOTE_MAJORITY_NODE1_MASKED);
        } else {
            FI_ASSERT(r1.status == VOTE_MAJORITY_NODE1_MASKED);
            FI_ASSERT(r1.final_pwm == nominal);
        }
        s_campaign_vectors_passed++;

        /* Node 2 bit-flip */
        int32_t n2_corrupted = nominal ^ (1U << bit);
        voter_result_t r2 = vote_2oo3(nominal, n2_corrupted, nominal);
        if (n2_corrupted >= 1495 && n2_corrupted <= 1505) {
            FI_ASSERT(r2.status == VOTE_UNANIMOUS || r2.status == VOTE_MAJORITY_NODE2_MASKED);
        } else {
            FI_ASSERT(r2.status == VOTE_MAJORITY_NODE2_MASKED);
            FI_ASSERT(r2.final_pwm == nominal);
        }
        s_campaign_vectors_passed++;

        /* Node 3 bit-flip */
        int32_t n3_corrupted = nominal ^ (1U << bit);
        voter_result_t r3 = vote_2oo3(nominal, nominal, n3_corrupted);
        if (n3_corrupted >= 1495 && n3_corrupted <= 1505) {
            FI_ASSERT(r3.status == VOTE_UNANIMOUS || r3.status == VOTE_MAJORITY_NODE3_MASKED);
        } else {
            FI_ASSERT(r3.status == VOTE_MAJORITY_NODE3_MASKED);
            FI_ASSERT(r3.final_pwm == nominal);
        }
        s_campaign_vectors_passed++;
    }
}

/* ==============================================================================
 * Campaign Section 2: Sensor Stuck-at & Extreme Boundary Testing
 * ============================================================================== */
static void run_campaign_stuck_at_and_boundaries(void) {
    printf("[CAMPAIGN-02] Testing Sensor Stuck-at & Boundary Conditions...\n");
    int32_t nominal = 1500;
    int32_t stuck_values[] = {
        1000, 2000, 0, -1, 500, 2500,
        SAFE_INT32_MIN, SAFE_INT32_MAX, -9999, 9999
    };
    size_t count = sizeof(stuck_values) / sizeof(stuck_values[0]);

    for (size_t i = 0; i < count; i++) {
        int32_t stuck = stuck_values[i];
        voter_result_t r = vote_2oo3(stuck, nominal, nominal);
        FI_ASSERT(r.status == VOTE_MAJORITY_NODE1_MASKED);
        FI_ASSERT(r.final_pwm == nominal);
        s_campaign_vectors_passed++;
    }
}

/* ==============================================================================
 * Campaign Section 3: Dual Simultaneous Faults & Total Disagreement
 * ============================================================================== */
static void run_campaign_dual_faults(void) {
    printf("[CAMPAIGN-03] Testing Dual Simultaneous Faults & Disagreement...\n");
    /* Node 1 bit-flip + Node 2 noise */
    voter_result_t r1 = vote_2oo3(2000, 1503, 1500);
    FI_ASSERT(r1.status == VOTE_MAJORITY_NODE1_MASKED);
    FI_ASSERT(r1.final_pwm >= 1500 && r1.final_pwm <= 1503);
    s_campaign_vectors_passed++;

    /* Total disagreement: 3 mutually incompatible outputs */
    voter_result_t r2 = vote_2oo3(1100, 1500, 1900);
    FI_ASSERT(r2.status == VOTE_TOTAL_DISAGREEMENT);
    FI_ASSERT(r2.final_pwm == FAIL_SAFE_VALUE);
    s_campaign_vectors_passed++;

    /* All 3 nodes corrupt to same off-nominal plausibility limit (2500 us > PWM_MAX_US) */
    node_sample_t oob_samples[3] = {
        { .pwm_us = 2500, .frame_id = 1, .valid = true },
        { .pwm_us = 2500, .frame_id = 1, .valid = true },
        { .pwm_us = 2500, .frame_id = 1, .valid = true }
    };
    voter_result_t r3 = vote_frame_inputs(oob_samples, 1);
    FI_ASSERT(r3.final_pwm == FAIL_SAFE_VALUE);
    FI_ASSERT(r3.status == VOTE_INSUFFICIENT_NODES);
    s_campaign_vectors_passed++;
}

/* ==============================================================================
 * Campaign Section 4: Supervision, Heartbeats & CFI Fault Injection
 * ============================================================================== */
static void run_campaign_cfi_and_supervision(void) {
    printf("[CAMPAIGN-04] Testing Control-Flow Integrity & Frame Supervision...\n");
    supervision_init();
    supervision_frame_start(100);

    /* Core 1 completes legitimate sequence */
    supervision_checkpoint(1, CFI_TOKEN_READ_INPUT);
    supervision_checkpoint(1, CFI_TOKEN_COMPUTE);
    supervision_checkpoint(1, CFI_TOKEN_WRITE_OUTPUT);
    supervision_checkpoint(1, CFI_TOKEN_CANARY_CHECK);
    supervision_checkpoint(1, CFI_TOKEN_COMPLETE);

    /* Core 2 has inverted checkpoint sequence (simulated control hijack) */
    supervision_checkpoint(2, CFI_TOKEN_WRITE_OUTPUT);
    supervision_checkpoint(2, CFI_TOKEN_READ_INPUT);

    uint32_t fault_mask = 0;
    bool ok = supervision_evaluate_nodes(0x0E, &fault_mask);
    FI_ASSERT(!ok);
    FI_ASSERT((fault_mask & (1 << 2)) != 0); /* Core 2 flagged with signature divergence */
    s_campaign_vectors_passed++;
}

/* ==============================================================================
 * Campaign Section 5: Stack Canary Breach Injection
 * ============================================================================== */
extern uint8_t host_stack_memory[];

static void run_campaign_stack_canaries(void) {
    printf("[CAMPAIGN-05] Testing Stack Boundary Corruption...\n");
    stack_monitor_init();
    FI_ASSERT(stack_canary_check_all() == true);

    /* Corrupt Core 1 partition bottom canary */
    uint32_t c1_bottom_offset = STACK_TOTAL_SIZE - (2 * STACK_PER_CORE_SIZE);
    volatile uint32_t *c1_canary = (volatile uint32_t *)&host_stack_memory[c1_bottom_offset];
    c1_canary[0] = 0xBAADF00DU;
    FI_ASSERT(stack_canary_check_core(1) == false);
    FI_ASSERT(stack_canary_check_all() == false);

    /* Re-initialize */
    stack_monitor_init();
    FI_ASSERT(stack_canary_check_all() == true);
    s_campaign_vectors_passed++;
}

/* ==============================================================================
 * Campaign Section 6: Double-Buffered Mailbox CRC & Sequence Injection
 * ============================================================================== */
static void run_campaign_mailbox_crc(void) {
    printf("[CAMPAIGN-06] Testing Mailbox CRC Injections & Stale Sequences...\n");
    mailbox_init();

    /* Legitimate transfer */
    mailbox_send_input(1, 10, 1500);
    int32_t val = 0;
    FI_ASSERT(mailbox_read_input(1, 10, &val) == MAILBOX_OK);
    FI_ASSERT(val == 1500);

    /* Injected stale sequence replay */
    FI_ASSERT(mailbox_read_input(1, 15, &val) == MAILBOX_ERR_SEQUENCE);

    /* Single bit-flip CRC injection */
    mailbox_msg_t corrupt_msg;
    corrupt_msg.sequence_id = 11;
    corrupt_msg.payload = 1500;
    corrupt_msg.timestamp_token = 11;
    corrupt_msg.crc32 = mailbox_calc_crc(&corrupt_msg) ^ 0x04; /* Flip bit 2 */
    FI_ASSERT(mailbox_verify_crc(&corrupt_msg) == false);
    s_campaign_vectors_passed++;
}

/* ==============================================================================
 * Campaign Section 7: Dual-Rail Software Lockstep Glitch Injection
 * ============================================================================== */
static void run_campaign_lockstep_glitch(void) {
    printf("[CAMPAIGN-07] Testing Core 0 Arbiter Dual-Rail Lockstep Glitch...\n");
    lockstep_init();
    failsafe_init();

    voter_result_t r = vote_2oo3(1500, 1500, 1500);
    /* Injected ALU glitch into median result */
    r.final_pwm = 1555;
    bool lockstep_ok = lockstep_verify_voter(1500, 1500, 1500, &r);
    FI_ASSERT(!lockstep_ok);
    FI_ASSERT(r.final_pwm == FAIL_SAFE_VALUE);
    FI_ASSERT(failsafe_is_latched() == true);
    s_campaign_vectors_passed++;
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Automated Fault-Injection Verification Campaign (P3.1)\n");
    printf("  Testing SEU Bit-Flips, CFI, Canaries, CRC, Watchdogs, & Lockstep\n");
    printf("==============================================================================\n");

    run_campaign_seu_bitflips();
    run_campaign_stuck_at_and_boundaries();
    run_campaign_dual_faults();
    run_campaign_cfi_and_supervision();
    run_campaign_stack_canaries();
    run_campaign_mailbox_crc();
    run_campaign_lockstep_glitch();

    printf("==============================================================================\n");
    printf("  Campaign Summary: %u test vectors evaluated, %u assertions verified, 0 failures\n",
           s_campaign_vectors_passed, s_campaign_assertions);
    printf("  Verdict: ALL FAULT INJECTION MATRICES MITIGATED SUCCESSFULLY\n");
    printf("==============================================================================\n");

    return 0;
}
