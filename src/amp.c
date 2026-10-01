#include "amp.h"
#include "memory_map.h"
#include "flight_control.h"
#include "voter.h"
#include "uart.h"
#include "failsafe.h"
#include "stack_monitor.h"
#include "supervision.h"
#include "mailbox.h"

void (* volatile secondary_spin_addr)(void) = NULL;
volatile core_flag_t core_done[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
volatile core_flag_t core_ready[4] = { {0, {0}}, {0, {0}}, {0, {0}}, {0, {0}} };
static volatile uint32_t core_cycle[4] = {0, 0, 0, 0};
volatile uint32_t g_cycle_counter = 0;

void amp_release_qemu_secondary_cores(void) {
    extern void _start(void);
    
    /*
     * 1. Write the secondary reset address into SYS_FLAGS registers:
     * - Versatile Express A15 daughterboard: 0x1C010030
     * - Versatile Express motherboard: 0x10000030
     */
    volatile uint32_t *sys_flags1 = (volatile uint32_t *)0x1C010030U;
    volatile uint32_t *sys_flags2 = (volatile uint32_t *)0x10000030U;
    *sys_flags1 = (uint32_t)_start;
    *sys_flags2 = (uint32_t)_start;
    dmb();

    /*
     * 2. Initialize GIC Distributor to forward SGIs:
     * GICD_CTLR (0x2C001000) = 1 (Enable Distributor)
     */
    volatile uint32_t *gic_ctlr = (volatile uint32_t *)GICD_CTLR;
    *gic_ctlr = 1;
    dmb();

    /* Enable SGIs 0-15 in GICD_ISENABLER0 (0x2C001100) */
    volatile uint32_t *gic_isenabler = (volatile uint32_t *)(GIC_DIST_BASE + 0x100U);
    *gic_isenabler = 0xFFFF;
    dmb();

    /* Configure GIC CPU Interface for CPU 0 */
    volatile uint32_t *gic_cpu_ctlr = (volatile uint32_t *)0x2C002000U;
    *gic_cpu_ctlr = 1;
    volatile uint32_t *gic_cpu_pmr = (volatile uint32_t *)0x2C002004U;
    *gic_cpu_pmr = 0xFF;
    dmb();

    /*
     * 3. Send SGI 0 to Cores 1, 2, 3 via GICD_SGIR (0x2C001F00):
     * TargetListFilter = 0b01 (forward to all CPUs except CPU 0)
     */
    volatile uint32_t *gic_sgir = (volatile uint32_t *)GICD_SGIR;
    *gic_sgir = (1U << 24); /* Forward to all other CPUs */
    dsb();
    sev();

    /* Also fire explicit CPU target list [23:16] = 0x0E (CPUs 1, 2, 3) */
    *gic_sgir = (0x0EU << 16);
    dsb();
    sev();
}

void secondary_core_boot_notify(void) {
    uint32_t core_id = get_core_id();
    if (core_id >= 1 && core_id <= 3) {
        core_ready[core_id].value = 1;
        dmb();
        sev();
    }
}

void secondary_core_entry(void) {
    uint32_t core_id = get_core_id();
    /* Prevent duplicate execution for the same frame cycle token */
    if (core_cycle[core_id] == g_cycle_counter) {
        return;
    }
    core_cycle[core_id] = g_cycle_counter;

    /* Checkpoint 1: Ingest sensor input */
    supervision_checkpoint(core_id, CFI_TOKEN_READ_INPUT);
    int32_t sensor_in = zone_read_input(core_id);
    int32_t mbox_in = 0;
    if (mailbox_read_input(core_id, g_cycle_counter, &mbox_in) == MAILBOX_OK) {
        sensor_in = mbox_in;
    }

    /* Checkpoint 2: Compute flight control algorithm */
    supervision_checkpoint(core_id, CFI_TOKEN_COMPUTE);
    int32_t pwm_out = flight_control_compute(core_id, sensor_in);

    /* Checkpoint 3: Write spatial output zone and double-buffered mailbox */
    supervision_checkpoint(core_id, CFI_TOKEN_WRITE_OUTPUT);
    zone_write_output(core_id, pwm_out);
    mailbox_send_output(core_id, g_cycle_counter, pwm_out);

    /* Checkpoint 4: Stack canary boundary verification */
    supervision_checkpoint(core_id, CFI_TOKEN_CANARY_CHECK);
    if (!stack_canary_check_core(core_id)) {
        return; /* Fail silent upon stack exhaustion / boundary breach */
    }

    /* Checkpoint 5: Frame completion token */
    supervision_checkpoint(core_id, CFI_TOKEN_COMPLETE);

    /* Flag completion */
    dmb();
    core_done[core_id].value = 1;
    dmb();
    dsb();
    sev();
}

bool amp_dispatch_and_wait(uint32_t *timed_out_mask) {
    if (timed_out_mask) *timed_out_mask = 0;

    /* Verify Arbiter stack integrity prior to frame dispatch */
    if (!stack_canary_check_core(0)) {
        failsafe_trigger(REASON_INTEGRITY_FAIL);
        return false;
    }

    /* Prime supervision subsystem and CFI signatures for new frame cycle */
    supervision_frame_start(g_cycle_counter + 1);

    core_done[1].value = 0;
    core_done[2].value = 0;
    core_done[3].value = 0;
    dmb();

    /* Publish entry point to mailbox and increment cycle token */
    secondary_spin_addr = secondary_core_entry;
    g_cycle_counter++;
    dmb();

    /* Trigger Inter-Processor Event (IPI) via sev instruction */
    dsb();
    sev();

    /*
     * Core 0 spin-locks while polling volatile core_done array.
     * Guarded by a software watchdog cycle timeout.
     */
    uint32_t timeout = WATCHDOG_MAX_CYCLES;
    while ((!core_done[1].value || !core_done[2].value || !core_done[3].value) && --timeout > 0) {
        yield_cpu();
    }
    dmb();

    /* Clear mailbox so secondary cores sleep in WFE until next dispatch */
    secondary_spin_addr = NULL;
    dmb();
    dsb();
    sev();

    /* Evaluate deadlines, completion flags, and CFI signatures across nodes */
    uint32_t done_mask = 0;
    if (core_done[1].value) done_mask |= (1 << 1);
    if (core_done[2].value) done_mask |= (1 << 2);
    if (core_done[3].value) done_mask |= (1 << 3);

    uint32_t fault_mask = 0;
    bool eval_ok = supervision_evaluate_nodes(done_mask, &fault_mask);

    if (timeout == 0 || !eval_ok) {
        if (timed_out_mask) {
            *timed_out_mask = fault_mask ? fault_mask : (~done_mask & 0x0E);
        }
        return false;
    }

    /* Post-frame stack canary audit across all cores */
    if (!stack_canary_check_all()) {
        failsafe_trigger(REASON_INTEGRITY_FAIL);
        return false;
    }

    return true;
}
