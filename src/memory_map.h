#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "types.h"

/*
 * ============================================================================
 * Spatial Memory Partitioning (Memory Map)
 * Strictly partitioned to prevent Common-Cause Failures (CCF) and SEU propagation.
 * ============================================================================
 */

/* System / Text Partition: Core 0 (0x80000000 - 0x80FFFFFF) */
#define SYSTEM_TEXT_BASE        0x80000000U
#define SYSTEM_TEXT_LIMIT       0x80FFFFFFU

/* Zone 1: Assigned exclusively to Core 1 */
#define ZONE1_BASE_ADDR         0x81000000U
#define ZONE1_INPUT_ADDR        0x81000000U
#define ZONE1_OUTPUT_ADDR       0x81000004U

/* Zone 2: Assigned exclusively to Core 2 */
#define ZONE2_BASE_ADDR         0x82000000U
#define ZONE2_INPUT_ADDR        0x82000000U
#define ZONE2_OUTPUT_ADDR       0x82000004U

/* Zone 3: Assigned exclusively to Core 3 */
#define ZONE3_BASE_ADDR         0x83000000U
#define ZONE3_INPUT_ADDR        0x83000000U
#define ZONE3_OUTPUT_ADDR       0x83000004U

/* Peripherals */
#define PL011_UART0_BASE        0x1C090000U
#define VEXPRESS_SYSREGS_BASE   0x1C010000U
#define VEXPRESS_SYS_FLAGS      (VEXPRESS_SYSREGS_BASE + 0x30U)
#define VEXPRESS_SYS_FLAGSSET   (VEXPRESS_SYSREGS_BASE + 0x30U)
#define VEXPRESS_SYS_FLAGSCLR   (VEXPRESS_SYSREGS_BASE + 0x34U)

/* GIC Distributor Base (Versatile Express CA15) */
#define GIC_DIST_BASE           0x2C001000U
#define GICD_CTLR               (GIC_DIST_BASE + 0x000U)
#define GICD_SGIR               (GIC_DIST_BASE + 0xF00U)

/* Strict 32-bit Volatile Memory Accessors */
static inline volatile int32_t* get_zone_input_ptr(uint32_t core_id) {
    switch (core_id) {
        case 1:  return (volatile int32_t*)ZONE1_INPUT_ADDR;
        case 2:  return (volatile int32_t*)ZONE2_INPUT_ADDR;
        case 3:  return (volatile int32_t*)ZONE3_INPUT_ADDR;
        default: return NULL;
    }
}

static inline volatile int32_t* get_zone_output_ptr(uint32_t core_id) {
    switch (core_id) {
        case 1:  return (volatile int32_t*)ZONE1_OUTPUT_ADDR;
        case 2:  return (volatile int32_t*)ZONE2_OUTPUT_ADDR;
        case 3:  return (volatile int32_t*)ZONE3_OUTPUT_ADDR;
        default: return NULL;
    }
}

static inline void zone_write_input(uint32_t core_id, int32_t value) {
    volatile int32_t *ptr = get_zone_input_ptr(core_id);
    if (ptr) {
        *ptr = value;
        dmb();
    }
}

static inline int32_t zone_read_input(uint32_t core_id) {
    volatile int32_t *ptr = get_zone_input_ptr(core_id);
    if (ptr) {
        return *ptr;
    }
    return 0;
}

static inline void zone_write_output(uint32_t core_id, int32_t value) {
    volatile int32_t *ptr = get_zone_output_ptr(core_id);
    if (ptr) {
        *ptr = value;
        dmb();
    }
}

static inline int32_t zone_read_output(uint32_t core_id) {
    volatile int32_t *ptr = get_zone_output_ptr(core_id);
    if (ptr) {
        return *ptr;
    }
    return 0;
}

#endif /* MEMORY_MAP_H */
