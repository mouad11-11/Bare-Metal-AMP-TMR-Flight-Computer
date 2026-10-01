#ifndef AMP_H
#define AMP_H

#include "types.h"

/* Mailbox function pointer monitored by secondary cores */
extern void (* volatile secondary_spin_addr)(void);

/* Completion flags set by Cores 1, 2, and 3 (cache-line padded) */
extern volatile core_flag_t core_done[4];

/* Boot / readiness flags set by Cores 1, 2, and 3 (cache-line padded) */
extern volatile core_flag_t core_ready[4];

/* Flight computer frame cycle token */
extern volatile uint32_t g_cycle_counter;

/* Primary entry function for secondary cores dispatched by Core 0 */
void secondary_core_entry(void);

/* Secondary core boot registration */
void secondary_core_boot_notify(void);

/* Wake secondary cores from QEMU boot holding pen */
void amp_release_qemu_secondary_cores(void);

/* Dispatch task to secondary cores and await completion with watchdog */
bool amp_dispatch_and_wait(uint32_t *timed_out_mask);

#endif /* AMP_H */
