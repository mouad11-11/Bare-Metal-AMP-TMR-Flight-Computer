#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "failsafe.h"

/* Host test stubs for embedded symbols */
volatile core_flag_t core_ready[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
volatile core_flag_t core_done[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
volatile uint32_t g_cycle_counter = 0;
void uart_printf(const char *fmt, ...) { (void)fmt; }

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg, ...) do { \
    g_tests_run++; \
    if (!(cond)) { \
        g_tests_failed++; \
        fprintf(stderr, "[FAIL] %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
} while(0)

/* T-FS-001: Latching and First Reason Retention (Tree 6) */
static void test_failsafe_latching(void) {
    printf("[RUN] T-FS-001: Fail-safe latching and first-reason retention\n");
    failsafe_init();
    TEST_ASSERT(!failsafe_is_latched(), "Failsafe should be unlatched initially");
    TEST_ASSERT(failsafe_get_latched_reason() == REASON_NONE, "Initial reason should be NONE");

    /* First trigger: WATCHDOG */
    command_t cmd1 = failsafe_trigger(REASON_WATCHDOG);
    TEST_ASSERT(failsafe_is_latched(), "Failsafe should be latched after trigger");
    TEST_ASSERT(cmd1.status == CMD_STATUS_FAILSAFE, "Command status must be FAILSAFE");
    TEST_ASSERT(cmd1.pwm_us == FAILSAFE_PWM_US, "Command PWM must be -9999");
    TEST_ASSERT(failsafe_get_latched_reason() == REASON_WATCHDOG, "Latched reason must be WATCHDOG");

    /* Second trigger: TOTAL_DISAGREEMENT - must retain first reason (Tree 6) */
    command_t cmd2 = failsafe_trigger(REASON_TOTAL_DISAGREEMENT);
    TEST_ASSERT(cmd2.status == CMD_STATUS_FAILSAFE, "Subsequent command status must be FAILSAFE");
    TEST_ASSERT(failsafe_get_latched_reason() == REASON_WATCHDOG, "First reason must be retained");
}

/* T-FS-002: All Reason Codes Coverage */
static void test_all_reason_codes(void) {
    printf("[RUN] T-FS-002: All fail-safe reason codes string coverage\n");
    failsafe_reason_t reasons[] = {
        REASON_NONE,
        REASON_TOTAL_DISAGREEMENT,
        REASON_INSUFFICIENT_NODES,
        REASON_WATCHDOG,
        REASON_EXCEPTION,
        REASON_POST_FAIL,
        REASON_INTEGRITY_FAIL,
        REASON_DEADLINE,
        REASON_CANNOT_ARBITRATE
    };
    int num_reasons = sizeof(reasons) / sizeof(reasons[0]);

    for (int i = 0; i < num_reasons; i++) {
        const char *str = failsafe_reason_to_string(reasons[i]);
        TEST_ASSERT(str != NULL && strlen(str) > 0, "Reason string must not be empty");
        TEST_ASSERT(strcmp(str, "UNKNOWN_REASON") != 0, "Known reason returned UNKNOWN");
    }

    /* Verify unknown code fallback */
    TEST_ASSERT(strcmp(failsafe_reason_to_string((failsafe_reason_t)999), "UNKNOWN_REASON") == 0,
                "Unknown reason fallback failed");
}

/* T-FS-003: Command Status String Coverage */
static void test_command_status_strings(void) {
    printf("[RUN] T-FS-003: Command status string coverage\n");
    TEST_ASSERT(strcmp(command_status_to_string(CMD_STATUS_OK), "OK") == 0, "OK status string");
    TEST_ASSERT(strcmp(command_status_to_string(CMD_STATUS_DEGRADED), "DEGRADED") == 0, "DEGRADED status string");
    TEST_ASSERT(strcmp(command_status_to_string(CMD_STATUS_FAILSAFE), "FAILSAFE") == 0, "FAILSAFE status string");
    TEST_ASSERT(strcmp(command_status_to_string(CMD_STATUS_INVALID), "INVALID") == 0, "INVALID status string");
    TEST_ASSERT(strcmp(command_status_to_string(999), "UNKNOWN") == 0, "UNKNOWN status string");
}

/* T-FS-004: Exception Diagnostics Recording (Tree 4) */
static void test_exception_diagnostics(void) {
    printf("[RUN] T-FS-004: Exception diagnostics recording\n");
    failsafe_init();

    fault_record_t dummy_rec;
    dummy_rec.core_id = 1;
    dummy_rec.exc_type = EXC_TYPE_DABT;
    dummy_rec.fault_pc = 0x81000020;
    dummy_rec.spsr = 0x60000010;
    dummy_rec.dfsr_ifsr = 0x00000005;
    dummy_rec.dfar_ifar = 0x82000000;

    failsafe_record_fault(&dummy_rec);

    /* Verify recording retrieval */
    const fault_record_t *p = failsafe_get_core_fault(1);
    TEST_ASSERT(p != NULL, "Fault record 1 must exist");
    TEST_ASSERT(p->fault_pc == 0x81000020, "Fault PC mismatch");
    TEST_ASSERT(p->exc_type == EXC_TYPE_DABT, "Fault type mismatch");
    TEST_ASSERT(p->dfar_ifar == 0x82000000, "Fault address mismatch");

    /* Out of bounds core_id check */
    TEST_ASSERT(failsafe_get_core_fault(4) == NULL, "Invalid core fault must return NULL");
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Fail-Safe Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_failsafe_latching();
    test_all_reason_codes();
    test_command_status_strings();
    test_exception_diagnostics();

    printf("==============================================================================\n");
    printf("  Results: %d assertions executed, %d failed\n", g_tests_run, g_tests_failed);
    printf("==============================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
