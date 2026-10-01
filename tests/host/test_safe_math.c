#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "safe_math.h"

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_safe_add(void) {
    printf("[RUN] T-MATH-001: Safe Saturating Addition\n");
    TEST_ASSERT(safe_add_i32(100, 200) == 300);
    TEST_ASSERT(safe_add_i32(-100, -200) == -300);
    TEST_ASSERT(safe_add_i32(0, 0) == 0);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MAX, 0) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MIN, 0) == SAFE_INT32_MIN);

    /* Saturation to MAX */
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MAX, 1) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MAX, 1000) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MAX, SAFE_INT32_MAX) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_add_i32(2000000000, 2000000000) == SAFE_INT32_MAX);

    /* Saturation to MIN */
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MIN, -1) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MIN, -1000) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_add_i32(SAFE_INT32_MIN, SAFE_INT32_MIN) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_add_i32(-2000000000, -2000000000) == SAFE_INT32_MIN);
}

static void test_safe_sub(void) {
    printf("[RUN] T-MATH-002: Safe Saturating Subtraction\n");
    TEST_ASSERT(safe_sub_i32(500, 200) == 300);
    TEST_ASSERT(safe_sub_i32(-100, 200) == -300);
    TEST_ASSERT(safe_sub_i32(0, 0) == 0);

    /* Saturation to MAX */
    TEST_ASSERT(safe_sub_i32(SAFE_INT32_MAX, -1) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_sub_i32(SAFE_INT32_MAX, -1000) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_sub_i32(1000, SAFE_INT32_MIN) == SAFE_INT32_MAX);

    /* Saturation to MIN */
    TEST_ASSERT(safe_sub_i32(SAFE_INT32_MIN, 1) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_sub_i32(SAFE_INT32_MIN, 1000) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_sub_i32(-2000000000, 2000000000) == SAFE_INT32_MIN);
}

static void test_safe_mul(void) {
    printf("[RUN] T-MATH-003: Safe Saturating Multiplication\n");
    TEST_ASSERT(safe_mul_i32(10, 20) == 200);
    TEST_ASSERT(safe_mul_i32(-10, 20) == -200);
    TEST_ASSERT(safe_mul_i32(-10, -20) == 200);
    TEST_ASSERT(safe_mul_i32(0, 5000) == 0);
    TEST_ASSERT(safe_mul_i32(5000, 0) == 0);
    TEST_ASSERT(safe_mul_i32(SAFE_INT32_MAX, 1) == SAFE_INT32_MAX);

    /* Saturation to MAX */
    TEST_ASSERT(safe_mul_i32(SAFE_INT32_MAX, 2) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_mul_i32(1000000, 3000) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_mul_i32(-1000000, -3000) == SAFE_INT32_MAX);

    /* Saturation to MIN */
    TEST_ASSERT(safe_mul_i32(SAFE_INT32_MIN, 2) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_mul_i32(1000000, -3000) == SAFE_INT32_MIN);
    TEST_ASSERT(safe_mul_i32(-1000000, 3000) == SAFE_INT32_MIN);
}

static void test_safe_div(void) {
    printf("[RUN] T-MATH-004: Safe Division & Zero Handling\n");
    TEST_ASSERT(safe_div_i32(100, 10, -1) == 10);
    TEST_ASSERT(safe_div_i32(-100, 10, -1) == -10);
    TEST_ASSERT(safe_div_i32(0, 5, -1) == 0);

    /* Zero denominator fallback */
    TEST_ASSERT(safe_div_i32(100, 0, 999) == 999);
    TEST_ASSERT(safe_div_i32(-50, 0, -9999) == -9999);

    /* INT32_MIN / -1 saturation */
    TEST_ASSERT(safe_div_i32(SAFE_INT32_MIN, -1, 0) == SAFE_INT32_MAX);
}

static void test_safe_abs(void) {
    printf("[RUN] T-MATH-005: Safe Absolute Value\n");
    TEST_ASSERT(safe_abs_i32(0) == 0);
    TEST_ASSERT(safe_abs_i32(42) == 42);
    TEST_ASSERT(safe_abs_i32(-42) == 42);
    TEST_ASSERT(safe_abs_i32(SAFE_INT32_MAX) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_abs_i32(SAFE_INT32_MIN) == SAFE_INT32_MAX);
}

static void test_safe_clamp(void) {
    printf("[RUN] T-MATH-006: Safe Clamping & Inverted Bounds Defense\n");
    TEST_ASSERT(safe_clamp_i32(1500, 1000, 2000) == 1500);
    TEST_ASSERT(safe_clamp_i32(500, 1000, 2000) == 1000);
    TEST_ASSERT(safe_clamp_i32(2500, 1000, 2000) == 2000);
    TEST_ASSERT(safe_clamp_i32(1000, 1000, 2000) == 1000);
    TEST_ASSERT(safe_clamp_i32(2000, 1000, 2000) == 2000);

    /* Inverted bounds defensive swap: min=2000, max=1000 */
    TEST_ASSERT(safe_clamp_i32(1500, 2000, 1000) == 1500);
    TEST_ASSERT(safe_clamp_i32(500, 2000, 1000) == 1000);
    TEST_ASSERT(safe_clamp_i32(2500, 2000, 1000) == 2000);
}

static void test_safe_diff(void) {
    printf("[RUN] T-MATH-007: Safe Absolute Difference\n");
    TEST_ASSERT(safe_diff_i32(10, 5) == 5);
    TEST_ASSERT(safe_diff_i32(5, 10) == 5);
    TEST_ASSERT(safe_diff_i32(0, 0) == 0);
    TEST_ASSERT(safe_diff_i32(-5, 5) == 10);
    TEST_ASSERT(safe_diff_i32(SAFE_INT32_MAX, 0) == SAFE_INT32_MAX);

    /* Exceeds INT32_MAX difference */
    TEST_ASSERT(safe_diff_i32(SAFE_INT32_MAX, SAFE_INT32_MIN) == SAFE_INT32_MAX);
    TEST_ASSERT(safe_diff_i32(SAFE_INT32_MIN, SAFE_INT32_MAX) == SAFE_INT32_MAX);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Safe Math Test Runner (Native C Unit & Coverage Harness)\n");
    printf("==============================================================================\n");

    test_safe_add();
    test_safe_sub();
    test_safe_mul();
    test_safe_div();
    test_safe_abs();
    test_safe_clamp();
    test_safe_diff();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
