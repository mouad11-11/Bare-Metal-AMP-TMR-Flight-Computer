#ifndef NODE_HEALTH_H
#define NODE_HEALTH_H

#include "types.h"
#include "config.h"

/*
 * ==============================================================================
 * Node Health & Latching Subsystem (Decision Tree 2)
 *
 * Implements leaky-bucket fault accounting and permanent latch-out for compute nodes.
 * When a node accumulates NODE_FAULT_LATCH_N consecutive faults, it is latched out.
 * Healthy frames decay the fault counter after NODE_GOOD_STREAK_M consecutive passes.
 * ==============================================================================
 */

typedef struct {
    uint32_t fault_count;
    uint32_t good_streak;
    bool is_latched;
} node_health_t;

/* Initialize health state for all compute nodes */
void node_health_init(void);

/* Record successful agreement frame for a node (leaky bucket decay) */
void node_health_record_success(uint32_t node_id);

/* Record fault / outlier / plausibility violation for a node */
void node_health_record_fault(uint32_t node_id);

/* Check if a node is permanently latched out */
bool node_health_is_latched(uint32_t node_id);

/* Retrieve snapshot of node health metrics */
node_health_t node_health_get(uint32_t node_id);

/* Reset health counters for a node (maintenance / recovery) */
void node_health_reset(uint32_t node_id);

#endif /* NODE_HEALTH_H */
