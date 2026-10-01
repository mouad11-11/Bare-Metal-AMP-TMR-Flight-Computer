#include "supervision.h"
#include "node_health.h"

static node_supervision_t s_nodes[4];
static uint32_t s_current_frame_id = 0;

void supervision_init(void) {
    s_current_frame_id = 0;
    for (uint32_t i = 0; i < 4; i++) {
        s_nodes[i].heartbeat_counter = 0;
        s_nodes[i].cfi_signature = 0;
        s_nodes[i].consecutive_misses = 0;
        s_nodes[i].is_timed_out = false;
    }
}

void supervision_frame_start(uint32_t expected_frame_id) {
    s_current_frame_id = expected_frame_id;
    for (uint32_t c = 1; c <= 3; c++) {
        s_nodes[c].cfi_signature = CFI_TOKEN_INIT;
        s_nodes[c].is_timed_out = false;
    }
}

void supervision_checkpoint(uint32_t core_id, uint32_t token) {
    if (core_id < 1 || core_id > 3) {
        return;
    }

    uint32_t current = s_nodes[core_id].cfi_signature;
    bool valid_transition = false;

    if (current == CFI_TOKEN_INIT && token == CFI_TOKEN_READ_INPUT) {
        valid_transition = true;
    } else if (current == CFI_TOKEN_READ_INPUT && token == CFI_TOKEN_COMPUTE) {
        valid_transition = true;
    } else if (current == CFI_TOKEN_COMPUTE && token == CFI_TOKEN_WRITE_OUTPUT) {
        valid_transition = true;
    } else if (current == CFI_TOKEN_WRITE_OUTPUT && token == CFI_TOKEN_CANARY_CHECK) {
        valid_transition = true;
    } else if (current == CFI_TOKEN_CANARY_CHECK && token == CFI_TOKEN_COMPLETE) {
        valid_transition = true;
    }

    if (valid_transition) {
        s_nodes[core_id].cfi_signature = token;
    } else {
        s_nodes[core_id].cfi_signature = 0xFFFFFFFFU; /* CFI divergence fault */
    }
}

bool supervision_evaluate_nodes(uint32_t done_mask, uint32_t *fault_mask) {
    if (fault_mask) {
        *fault_mask = 0;
    }

    bool all_ok = true;

    for (uint32_t c = 1; c <= 3; c++) {
        bool done = (done_mask & (1U << c)) != 0;
        bool cfi_intact = (s_nodes[c].cfi_signature == CFI_TOKEN_COMPLETE);

        if (done && cfi_intact) {
            s_nodes[c].heartbeat_counter++;
            s_nodes[c].consecutive_misses = 0;
            s_nodes[c].is_timed_out = false;
        } else {
            all_ok = false;
            if (fault_mask) {
                *fault_mask |= (1U << c);
            }
            s_nodes[c].consecutive_misses++;
            s_nodes[c].is_timed_out = true;

            if (s_nodes[c].consecutive_misses >= NODE_MISS_LIMIT_K) {
                node_health_record_fault(c);
            }
        }
    }

    return all_ok;
}

node_supervision_t supervision_get_node(uint32_t core_id) {
    if (core_id < 1 || core_id > 3) {
        node_supervision_t invalid = { .heartbeat_counter = 0, .cfi_signature = 0, .consecutive_misses = 99, .is_timed_out = true };
        return invalid;
    }
    return s_nodes[core_id];
}

bool supervision_is_node_dead(uint32_t core_id) {
    if (core_id < 1 || core_id > 3) {
        return true;
    }
    return (s_nodes[core_id].consecutive_misses >= NODE_MISS_LIMIT_K);
}
