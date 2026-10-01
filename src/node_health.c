#include "node_health.h"

static node_health_t s_health[4];

void node_health_init(void) {
    for (uint32_t i = 0; i < 4; i++) {
        s_health[i].fault_count = 0;
        s_health[i].good_streak = 0;
        s_health[i].is_latched = false;
    }
}

void node_health_record_fault(uint32_t node_id) {
    if (node_id < 1 || node_id > 3) {
        return;
    }

    if (s_health[node_id].is_latched) {
        return;
    }

    s_health[node_id].fault_count++;
    s_health[node_id].good_streak = 0;

    if (s_health[node_id].fault_count >= NODE_FAULT_LATCH_N) {
        s_health[node_id].is_latched = true;
    }
}

void node_health_record_success(uint32_t node_id) {
    if (node_id < 1 || node_id > 3) {
        return;
    }

    if (s_health[node_id].is_latched) {
        return;
    }

    s_health[node_id].good_streak++;
    if (s_health[node_id].good_streak >= NODE_GOOD_STREAK_M) {
        if (s_health[node_id].fault_count > 0) {
            s_health[node_id].fault_count--;
        }
        s_health[node_id].good_streak = 0;
    }
}

bool node_health_is_latched(uint32_t node_id) {
    if (node_id < 1 || node_id > 3) {
        return true;
    }
    return s_health[node_id].is_latched;
}

node_health_t node_health_get(uint32_t node_id) {
    if (node_id < 1 || node_id > 3) {
        node_health_t invalid_node = { .fault_count = 0, .good_streak = 0, .is_latched = true };
        return invalid_node;
    }
    return s_health[node_id];
}

void node_health_reset(uint32_t node_id) {
    if (node_id < 1 || node_id > 3) {
        return;
    }
    s_health[node_id].fault_count = 0;
    s_health[node_id].good_streak = 0;
    s_health[node_id].is_latched = false;
}
