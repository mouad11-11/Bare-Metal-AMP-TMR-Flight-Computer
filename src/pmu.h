#ifndef PMU_H
#define PMU_H

#include "types.h"

/* Real-Time Frame Deadline Budget (in clock cycles) */
#define FRAME_DEADLINE_CYCLES       500000U /* 500,000 cycles (~5ms at 100MHz) */

typedef struct {
    uint32_t ingest_cycles;
    uint32_t compute_sync_cycles;
    uint32_t voter_lockstep_cycles;
    uint32_t telemetry_cycles;
    uint32_t total_frame_cycles;
    uint32_t max_observed_cycles;
} wcet_profile_t;

/**
 * @brief Initialize ARM Cortex-A15 Performance Monitor Unit (PMU) cycle counter.
 */
void pmu_init(void);

/**
 * @brief Read current 32-bit PMU cycle counter (PMCCNTR).
 */
uint32_t pmu_get_cycles(void);

/**
 * @brief Record WCET frame segment timings and update maximum observed profile.
 */
void pmu_record_frame_timing(uint32_t t_start, uint32_t t_ingest, uint32_t t_sync, uint32_t t_voter, uint32_t t_end);

/**
 * @brief Retrieve current WCET profile.
 */
void pmu_get_profile(wcet_profile_t *out_profile);

/**
 * @brief Print WCET benchmark and timing margin telemetry to UART console.
 */
void pmu_print_telemetry(void);

#endif /* PMU_H */
