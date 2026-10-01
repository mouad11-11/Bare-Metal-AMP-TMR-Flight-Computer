#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "post.h"
#include "failsafe.h"

/* Host test stubs */
volatile core_flag_t core_ready[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
volatile core_flag_t core_done[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
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

static void test_cpu_registers(void) {
    printf("[RUN] T-POST-001: CPU register test\n");
    TEST_ASSERT(post_test_cpu_registers() == true, "CPU register test should pass");
}

static void test_ram_march(void) {
    printf("[RUN] T-POST-002: RAM March C- test\n");
    uint32_t buf[256];
    TEST_ASSERT(post_test_ram_march_c_minus(buf, 256) == true, "March C- should pass on healthy memory");

    /* Null / zero size */
    TEST_ASSERT(post_test_ram_march_c_minus(NULL, 256) == false, "Null buffer should return false");
    TEST_ASSERT(post_test_ram_march_c_minus(buf, 0) == false, "Zero length should return false");
}

static void test_crc32(void) {
    printf("[RUN] T-POST-003: CRC32 standard test vector\n");
    /* Standard test vector: "123456789" -> 0xCBF43926 */
    const uint8_t test_vec[] = "123456789";
    uint32_t crc = post_compute_crc32(test_vec, 9);
    TEST_ASSERT(crc == 0xCBF43926U, "Expected CRC 0xCBF43926, got 0x%08X", crc);

    TEST_ASSERT(post_compute_crc32(NULL, 10) == 0, "Null data should return 0");
    TEST_ASSERT(post_compute_crc32(test_vec, 0) == 0, "Zero length should return 0");

    TEST_ASSERT(post_test_code_integrity() == true, "Code integrity test should pass on host");
}

static void test_voter_vectors(void) {
    printf("[RUN] T-POST-004: Voter vector test\n");
    TEST_ASSERT(post_test_voter_vectors() == true, "Voter vectors should pass");
}

static void test_post_run_all(void) {
    printf("[RUN] T-POST-005: Full post_run_all suite\n");
    failsafe_init();
    post_status_t status = post_run_all();
    TEST_ASSERT(status == POST_PASS, "Expected POST_PASS, got %d", status);
    TEST_ASSERT(!failsafe_is_latched(), "Failsafe should not be latched on successful POST");
}

static void test_post_strings(void) {
    printf("[RUN] T-POST-006: POST status strings\n");
    TEST_ASSERT(strstr(post_status_to_string(POST_PASS), "PASS") != NULL, "PASS string");
    TEST_ASSERT(strstr(post_status_to_string(POST_FAIL_CPU_REGISTERS), "CPU") != NULL, "CPU string");
    TEST_ASSERT(strstr(post_status_to_string(POST_FAIL_RAM_MARCH), "RAM") != NULL, "RAM string");
    TEST_ASSERT(strstr(post_status_to_string(POST_FAIL_CODE_CRC), "CRC") != NULL, "CRC string");
    TEST_ASSERT(strstr(post_status_to_string(POST_FAIL_VOTER), "Voter") != NULL, "Voter string");
    TEST_ASSERT(strstr(post_status_to_string(POST_FAIL_PERIPHERAL), "Peripheral") != NULL, "Peripheral string");
    TEST_ASSERT(strstr(post_status_to_string((post_status_t)99), "Unknown") != NULL, "Unknown string");
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Power-On Self-Test (POST) Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_cpu_registers();
    test_ram_march();
    test_crc32();
    test_voter_vectors();
    test_post_run_all();
    test_post_strings();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", g_assertions);
    printf("==============================================================================\n");

    return 0;
}
