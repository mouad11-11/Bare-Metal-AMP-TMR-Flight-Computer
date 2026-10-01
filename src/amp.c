#include "amp.h"
#include "memory_map.h"
#include "flight_control.h"
#include "voter.h"
#include "uart.h"

void (* volatile secondary_spin_addr)(void) = NULL;
volatile uint32_t core_done[4] = {0, 0, 0, 0};
volatile uint32_t core_ready[4] = {0, 0, 0, 0};
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
        core_ready[core_id] = 1;
        dmb();
        sev();
    }
}

void secondary_core_entry(void) {
    uint32_t core_id = get_core_id();
    if (core_id < 1 || core_id > 3) return;

    /*
     * Spatial Partitioning:
     * Core 1 reads exclusively from Zone 1 (0x81000000)
     * Core 2 reads exclusively from Zone 2 (0x82000000)
     * Core 3 reads exclusively from Zone 3 (0x83000000)
     */
    int32_t sensor_in = zone_read_input(core_id);

    /* Execute deterministic flight control algorithm */
    int32_t pwm_out = flight_control_compute(core_id, sensor_in);

    /*
     * Spatial Partitioning:
     * Core 1 writes exclusively to Zone 1 (0x81000004)
     * Core 2 writes exclusively to Zone 2 (0x82000004)
     * Core 3 writes exclusively to Zone 3 (0x83000004)
     */
    zone_write_output(core_id, pwm_out);

    /* Flag completion */
    dmb();
    core_done[core_id] = 1;
    dmb();
    sev();
}

bool amp_dispatch_and_wait(uint32_t *timed_out_mask) {
    if (timed_out_mask) *timed_out_mask = 0;

    core_done[1] = 0;
    core_done[2] = 0;
    core_done[3] = 0;
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
     * Guarded by a hardware watchdog cycle timeout.
     */
    uint32_t timeout = WATCHDOG_MAX_CYCLES;
    while ((!core_done[1] || !core_done[2] || !core_done[3]) && --timeout > 0) {
        __asm__ volatile("nop");
    }

    /* Clear mailbox so secondary cores sleep in WFE until next dispatch */
    secondary_spin_addr = NULL;
    dmb();

    if (timeout == 0) {
        uint32_t mask = 0;
        if (!core_done[1]) mask |= (1 << 1);
        if (!core_done[2]) mask |= (1 << 2);
        if (!core_done[3]) mask |= (1 << 3);
        if (timed_out_mask) *timed_out_mask = mask;
        return false;
    }

    return true;
}
