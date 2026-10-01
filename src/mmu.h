#ifndef MMU_H
#define MMU_H

#include "types.h"

/* ARMv7-A 1MB Section Translation Table Constants */
#define MMU_TABLE_ENTRIES       4096U
#define MMU_SECTION_SIZE        0x00100000U /* 1MB */

/* Section Descriptor Bits */
#define MMU_DESC_FAULT          0x00000000U
#define MMU_DESC_SECTION        0x00000002U

/* Device memory attributes (Shared Device, XN=1, RW) */
#define MMU_ATTR_DEVICE         (MMU_DESC_SECTION | 0x00010000U | 0x00000C00U | 0x00000010U | 0x00000004U)

/* Normal memory attributes (Executable RWX for System Code & Stack) */
#define MMU_ATTR_NORMAL_RWX     (MMU_DESC_SECTION | 0x00010000U | 0x00001000U | 0x00000C00U)

/* Normal memory attributes (Execute-Never RW for Data Zones) */
#define MMU_ATTR_NORMAL_RW      (MMU_DESC_SECTION | 0x00010000U | 0x00001000U | 0x00000C00U | 0x00000010U)

typedef enum {
    MMU_ACCESS_OK = 0,
    MMU_ACCESS_FAULT = 1,
    MMU_ACCESS_PERMISSION_DENIED = 2
} mmu_access_status_t;

/**
 * @brief Initialize translation tables for all 4 cores.
 * Configures spatial memory partitioning:
 * - Core 0 has access to System RAM, Peripherals, and Zones 1, 2, 3.
 * - Core 1 has access to System RAM, Peripherals, and Zone 1 only (Zones 2, 3 unmapped).
 * - Core 2 has access to System RAM, Peripherals, and Zone 2 only (Zones 1, 3 unmapped).
 * - Core 3 has access to System RAM, Peripherals, and Zone 3 only (Zones 1, 2 unmapped).
 */
void mmu_init_tables(void);

/**
 * @brief Enable MMU on the calling core with its dedicated table.
 */
void mmu_enable_core(uint32_t core_id);

/**
 * @brief Software audit check for memory address accessibility by a core.
 * Used for host verification, pre-flight audit, and boundary validation.
 */
mmu_access_status_t mmu_check_permission(uint32_t core_id, uint32_t vaddr, bool is_write);

/**
 * @brief Retrieve the raw translation table descriptor for a given core and address.
 */
uint32_t mmu_get_descriptor(uint32_t core_id, uint32_t vaddr);

/**
 * @brief Returns true if MMU is enabled on the current CPU core.
 */
bool mmu_is_enabled(void);

#endif /* MMU_H */
