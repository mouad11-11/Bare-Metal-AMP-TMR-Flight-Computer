#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "mmu.h"
#include "memory_map.h"

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_core0_spatial_permissions(void) {
    printf("[RUN] T-MMU-001: Core 0 (Master Arbiter) Full Spatial Access\n");
    mmu_init_tables();

    /* System RAM / Code */
    TEST_ASSERT(mmu_check_permission(0, SYSTEM_TEXT_BASE, false) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, SYSTEM_TEXT_BASE + 0x100000, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, SYSTEM_TEXT_LIMIT, false) == MMU_ACCESS_OK);

    /* Peripherals */
    TEST_ASSERT(mmu_check_permission(0, 0x10000000U, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, 0x1C000000U, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, 0x2C000000U, true) == MMU_ACCESS_OK);

    /* All 3 Zones accessible to Core 0 */
    TEST_ASSERT(mmu_check_permission(0, ZONE1_BASE_ADDR, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, ZONE2_BASE_ADDR, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(0, ZONE3_BASE_ADDR, true) == MMU_ACCESS_OK);
}

static void test_core1_spatial_isolation(void) {
    printf("[RUN] T-MMU-002: Core 1 (Node 1) Spatial Isolation & Zone Access\n");
    /* Core 1 can access Zone 1 */
    TEST_ASSERT(mmu_check_permission(1, ZONE1_BASE_ADDR, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(1, ZONE1_INPUT_ADDR, false) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(1, ZONE1_OUTPUT_ADDR, true) == MMU_ACCESS_OK);

    /* Core 1 CANNOT access Zone 2 or Zone 3 (Hardware Section Translation Fault) */
    TEST_ASSERT(mmu_check_permission(1, ZONE2_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(1, ZONE2_BASE_ADDR, true) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(1, ZONE3_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(1, ZONE3_BASE_ADDR, true) == MMU_ACCESS_FAULT);
}

static void test_core2_spatial_isolation(void) {
    printf("[RUN] T-MMU-003: Core 2 (Node 2) Spatial Isolation & Zone Access\n");
    /* Core 2 can access Zone 2 */
    TEST_ASSERT(mmu_check_permission(2, ZONE2_BASE_ADDR, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(2, ZONE2_INPUT_ADDR, false) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(2, ZONE2_OUTPUT_ADDR, true) == MMU_ACCESS_OK);

    /* Core 2 CANNOT access Zone 1 or Zone 3 */
    TEST_ASSERT(mmu_check_permission(2, ZONE1_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(2, ZONE1_BASE_ADDR, true) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(2, ZONE3_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(2, ZONE3_BASE_ADDR, true) == MMU_ACCESS_FAULT);
}

static void test_core3_spatial_isolation(void) {
    printf("[RUN] T-MMU-004: Core 3 (Node 3) Spatial Isolation & Zone Access\n");
    /* Core 3 can access Zone 3 */
    TEST_ASSERT(mmu_check_permission(3, ZONE3_BASE_ADDR, true) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(3, ZONE3_INPUT_ADDR, false) == MMU_ACCESS_OK);
    TEST_ASSERT(mmu_check_permission(3, ZONE3_OUTPUT_ADDR, true) == MMU_ACCESS_OK);

    /* Core 3 CANNOT access Zone 1 or Zone 2 */
    TEST_ASSERT(mmu_check_permission(3, ZONE1_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(3, ZONE1_BASE_ADDR, true) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(3, ZONE2_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(3, ZONE2_BASE_ADDR, true) == MMU_ACCESS_FAULT);
}

static void test_boundary_and_descriptors(void) {
    printf("[RUN] T-MMU-005: Boundary Checks & Descriptor Query\n");
    /* Invalid core IDs */
    TEST_ASSERT(mmu_check_permission(4, ZONE1_BASE_ADDR, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(99, ZONE1_BASE_ADDR, true) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_get_descriptor(4, ZONE1_BASE_ADDR) == 0);

    /* Unmapped general addresses */
    TEST_ASSERT(mmu_check_permission(0, 0x00000000U, false) == MMU_ACCESS_FAULT);
    TEST_ASSERT(mmu_check_permission(0, 0x90000000U, true) == MMU_ACCESS_FAULT);

    /* Descriptors */
    uint32_t d0_z1 = mmu_get_descriptor(0, ZONE1_BASE_ADDR);
    TEST_ASSERT((d0_z1 & 0x3) == MMU_DESC_SECTION);
    TEST_ASSERT((d0_z1 & 0xFFF00000U) == ZONE1_BASE_ADDR);

    uint32_t d1_z2 = mmu_get_descriptor(1, ZONE2_BASE_ADDR);
    TEST_ASSERT(d1_z2 == MMU_DESC_FAULT);

    /* Enable function and status */
    mmu_enable_core(0);
    mmu_enable_core(1);
    mmu_enable_core(2);
    mmu_enable_core(3);
    mmu_enable_core(4); /* Boundary check */
    TEST_ASSERT(mmu_is_enabled() == true);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host MMU & Spatial Protection Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_core0_spatial_permissions();
    test_core1_spatial_isolation();
    test_core2_spatial_isolation();
    test_core3_spatial_isolation();
    test_boundary_and_descriptors();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
