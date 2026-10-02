#ifndef SUPERVISION_H
#define SUPERVISION_H

#include "types.h"
#include "config.h"

/*
 * ==============================================================================
 * Frame Supervision, Heartbeat & Control Flow Integrity (Decision Tree 3)
 *
 * Implements deterministic frame deadline supervision, per-node heartbeat tracking,
 * consecutive missed deadline latching (NODE_MISS_LIMIT_K), and Control Flow
 * Integrity (CFI) signature chaining across critical dispatch checkpoints.
 * ==============================================================================
 */

/* Control Flow Integrity (CFI) Checkpoint Tokens */
#define CFI_TOKEN_INIT          0x5A1F0001U
#define CFI_TOKEN_READ_INPUT    0x5A1F0002U
#define CFI_TOKEN_COMPUTE       0x5A1F0003U
#define CFI_TOKEN_WRITE_OUTPUT  0x5A1F0004U
#define CFI_TOKEN_CANARY_CHECK  0x5A1F0005U
#define CFI_TOKEN_COMPLETE      0x5A1F0006U

/* Heartbeat and CFI state per core */
typedef struct {
    uint32_t heartbeat_counter;
    uint32_t cfi_signature;
    uint32_t consecutive_misses;
    bool is_timed_out;
} node_supervision_t;

/* Initialize supervision subsystem */
void supervision_init(void);

/* Called by Arbiter prior to frame dispatch to prime CFI and supervision tokens */
void supervision_frame_start(uint32_t expected_frame_id);

/* Reset CFI signature and state for a single node prior to dispatch */
void supervision_reset_frame(uint32_t core_id);

/* Called by worker nodes at distinct execution checkpoints to advance signature */
void supervision_checkpoint(uint32_t core_id, uint32_t token);

/* Called by Arbiter after dispatch to evaluate worker node deadlines and CFI */
bool supervision_evaluate_nodes(uint32_t done_mask, uint32_t *fault_mask);

/* Retrieve snapshot of node supervision metrics */
node_supervision_t supervision_get_node(uint32_t core_id);

/* Check if node has exceeded consecutive missed deadline limit */
bool supervision_is_node_dead(uint32_t core_id);

#endif /* SUPERVISION_H */
