#include "mmu.h"
#include "memory_map.h"

/* 4 translation tables (16KB each; 16KB aligned on ARM ELF, 4KB on host) */
#if defined(__arm__) || defined(__thumb__)
#define MMU_ALIGN_BYTES 16384
#else
#define MMU_ALIGN_BYTES 4096
#endif

static uint32_t s_mmu_tables[4][MMU_TABLE_ENTRIES] __attribute__((aligned(MMU_ALIGN_BYTES)));
static bool s_mmu_initialized = false;

static void mmu_map_section(uint32_t core_id, uint32_t paddr, uint32_t vaddr, uint32_t attr) {
    if (core_id >= 4) return;
    uint32_t idx = vaddr >> 20;
    if (idx < MMU_TABLE_ENTRIES) {
        s_mmu_tables[core_id][idx] = (paddr & 0xFFF00000U) | attr;
    }
}

void mmu_init_tables(void) {
    /* Initialize all translation tables to fault (unmapped) */
    for (uint32_t c = 0; c < 4; c++) {
        for (uint32_t i = 0; i < MMU_TABLE_ENTRIES; i++) {
            s_mmu_tables[c][i] = MMU_DESC_FAULT;
        }

        /* Identity map shared device peripherals */
        mmu_map_section(c, 0x10000000U, 0x10000000U, MMU_ATTR_DEVICE); /* Motherboard Sysregs */
        mmu_map_section(c, 0x1C000000U, 0x1C000000U, MMU_ATTR_DEVICE); /* Daughterboard Sysregs, UART */
        mmu_map_section(c, 0x2C000000U, 0x2C000000U, MMU_ATTR_DEVICE); /* GIC Distributor & CPU Interface */

        /* Identity map System RAM (16MB: 0x80000000 - 0x80FFFFFF) for code, data, stacks */
        for (uint32_t mb = 0; mb < 16; mb++) {
            uint32_t addr = SYSTEM_TEXT_BASE + (mb * MMU_SECTION_SIZE);
            mmu_map_section(c, addr, addr, MMU_ATTR_NORMAL_RWX);
        }
    }

    /*
     * Hardware Spatial Partitioning Rules:
     * Core 0 (Master Arbiter): Read/Write access to all 3 zones.
     * Core 1 (Node 1): Dedicated access to Zone 1. Zones 2 & 3 remain unmapped (fault).
     * Core 2 (Node 2): Dedicated access to Zone 2. Zones 1 & 3 remain unmapped (fault).
     * Core 3 (Node 3): Dedicated access to Zone 3. Zones 1 & 2 remain unmapped (fault).
     */
    /* Core 0 */
    mmu_map_section(0, ZONE1_BASE_ADDR, ZONE1_BASE_ADDR, MMU_ATTR_NORMAL_RW);
    mmu_map_section(0, ZONE2_BASE_ADDR, ZONE2_BASE_ADDR, MMU_ATTR_NORMAL_RW);
    mmu_map_section(0, ZONE3_BASE_ADDR, ZONE3_BASE_ADDR, MMU_ATTR_NORMAL_RW);

    /* Core 1 */
    mmu_map_section(1, ZONE1_BASE_ADDR, ZONE1_BASE_ADDR, MMU_ATTR_NORMAL_RW);

    /* Core 2 */
    mmu_map_section(2, ZONE2_BASE_ADDR, ZONE2_BASE_ADDR, MMU_ATTR_NORMAL_RW);

    /* Core 3 */
    mmu_map_section(3, ZONE3_BASE_ADDR, ZONE3_BASE_ADDR, MMU_ATTR_NORMAL_RW);

    s_mmu_initialized = true;
    dmb();
}

void mmu_enable_core(uint32_t core_id) {
    if (core_id >= 4) return;
    if (!s_mmu_initialized) {
        mmu_init_tables();
    }

#if defined(__arm__) || defined(__thumb__)
    uint32_t ttbr = (uint32_t)&s_mmu_tables[core_id][0];

    /* TTBCR = 0 (use TTBR0 for all 4GB) */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(0));

    /* TTBR0 = table physical base address */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttbr));

    /* DACR = 0x00000001 (Domain 0 = Client: checks permission bits) */
    __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(0x00000001U));

    /* Invalidate unified TLB */
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(0));
    dsb();
    isb();

    /* Enable MMU in SCTLR (bit 0 = 1) */
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr |= 1U;
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr));
    isb();
#endif
}

mmu_access_status_t mmu_check_permission(uint32_t core_id, uint32_t vaddr, bool is_write) {
    if (core_id >= 4) {
        return MMU_ACCESS_FAULT;
    }
    if (!s_mmu_initialized) {
        mmu_init_tables();
    }

    uint32_t idx = vaddr >> 20;
    if (idx >= MMU_TABLE_ENTRIES) {
        return MMU_ACCESS_FAULT;
    }

    uint32_t desc = s_mmu_tables[core_id][idx];
    if ((desc & 0x3U) == MMU_DESC_FAULT) {
        return MMU_ACCESS_FAULT;
    }

    if (is_write) {
        /* In short-descriptor format, AP[2] (bit 15) indicates read-only if 1 */
        if (desc & (1U << 15)) {
            return MMU_ACCESS_PERMISSION_DENIED;
        }
    }

    return MMU_ACCESS_OK;
}

uint32_t mmu_get_descriptor(uint32_t core_id, uint32_t vaddr) {
    if (core_id >= 4) return 0;
    if (!s_mmu_initialized) {
        mmu_init_tables();
    }
    uint32_t idx = vaddr >> 20;
    if (idx >= MMU_TABLE_ENTRIES) return 0;
    return s_mmu_tables[core_id][idx];
}

bool mmu_is_enabled(void) {
#if defined(__arm__) || defined(__thumb__)
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    return (sctlr & 1U) != 0;
#else
    return s_mmu_initialized;
#endif
}
