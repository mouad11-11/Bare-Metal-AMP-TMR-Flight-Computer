#ifndef STACK_MONITOR_H
#define STACK_MONITOR_H

#include "types.h"
#include "config.h"

/*
 * ==============================================================================
 * Stack Overflow Protection & Canary Monitoring Subsystem
 *
 * Implements hardware stack partition boundary canaries and watermark painting
 * for deterministic detection of stack exhaustion and inter-core memory corruption.
 * ==============================================================================
 */

/* Initialize stack canaries and watermark memory across all cores */
void stack_monitor_init(void);

/* Check canaries for a specific core (0 = Arbiter, 1-3 = Worker nodes) */
bool stack_canary_check_core(uint32_t core_id);

/* Check canaries across all cores */
bool stack_canary_check_all(void);

/* Retrieve measured maximum stack usage in bytes for a given core */
uint32_t stack_get_high_water_mark(uint32_t core_id);

/* Retrieve remaining unused stack headroom in bytes for a given core */
uint32_t stack_get_headroom(uint32_t core_id);

#endif /* STACK_MONITOR_H */
