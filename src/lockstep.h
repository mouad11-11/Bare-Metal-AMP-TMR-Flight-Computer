#ifndef LOCKSTEP_H
#define LOCKSTEP_H

#include "types.h"
#include "voter.h"

typedef struct {
    int32_t rail_a_pwm;
    int32_t rail_b_pwm;
    bool match;
    uint32_t verification_count;
    uint32_t failure_count;
} lockstep_diag_t;

/**
 * @brief Initialize Core 0 Dual-Rail Lockstep and self-monitoring subsystem.
 */
void lockstep_init(void);

/**
 * @brief Software Dual-Rail Lockstep Verification for Core 0 Arbiter.
 * Executes diverse voting rail using branchless min-max algebraic formulation
 * and compares against primary voter result to detect Core 0 internal SEUs/ALU corruption.
 *
 * @param y1 Input channel 1
 * @param y2 Input channel 2
 * @param y3 Input channel 3
 * @param res Pointer to primary voter result (modified to fail-safe if divergence detected)
 * @return true if both rails agree, false if internal divergence detected
 */
bool lockstep_verify_voter(int32_t y1, int32_t y2, int32_t y3, voter_result_t *res);

/**
 * @brief Retrieve lockstep self-monitoring diagnostics.
 */
void lockstep_get_diagnostics(lockstep_diag_t *diag_out);

#endif /* LOCKSTEP_H */
