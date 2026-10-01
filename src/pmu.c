#include "pmu.h"
#include "uart.h"

static wcet_profile_t s_wcet_profile = {0, 0, 0, 0, 0, 0};
#if !defined(__arm__) && !defined(__thumb__)
static uint32_t s_host_sim_cycles = 1000;
#endif

void pmu_init(void) {
#if defined(__arm__) || defined(__thumb__)
    /* Enable PMU (bit 0 = E) and reset cycle counter (bit 2 = C) in PMCR */
    uint32_t pmcr = (1U << 0) | (1U << 2);
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(pmcr));

    /* Enable PMCCNTR cycle counter in PMCNTENSET (bit 31) */
    uint32_t cntenset = (1U << 31);
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(cntenset));
    isb();
#else
    s_host_sim_cycles = 1000;
#endif

    s_wcet_profile.ingest_cycles = 0;
    s_wcet_profile.compute_sync_cycles = 0;
    s_wcet_profile.voter_lockstep_cycles = 0;
    s_wcet_profile.telemetry_cycles = 0;
    s_wcet_profile.total_frame_cycles = 0;
    s_wcet_profile.max_observed_cycles = 0;
}

uint32_t pmu_get_cycles(void) {
#if defined(__arm__) || defined(__thumb__)
    uint32_t cycles;
    __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycles));
    return cycles;
#else
    s_host_sim_cycles += 500;
    return s_host_sim_cycles;
#endif
}

void pmu_record_frame_timing(uint32_t t_start, uint32_t t_ingest, uint32_t t_sync, uint32_t t_voter, uint32_t t_end) {
    s_wcet_profile.ingest_cycles = (t_ingest >= t_start) ? (t_ingest - t_start) : 0;
    s_wcet_profile.compute_sync_cycles = (t_sync >= t_ingest) ? (t_sync - t_ingest) : 0;
    s_wcet_profile.voter_lockstep_cycles = (t_voter >= t_sync) ? (t_voter - t_sync) : 0;
    s_wcet_profile.telemetry_cycles = (t_end >= t_voter) ? (t_end - t_voter) : 0;
    s_wcet_profile.total_frame_cycles = (t_end >= t_start) ? (t_end - t_start) : 0;

    if (s_wcet_profile.total_frame_cycles > s_wcet_profile.max_observed_cycles) {
        s_wcet_profile.max_observed_cycles = s_wcet_profile.total_frame_cycles;
    }
}

void pmu_get_profile(wcet_profile_t *out_profile) {
    if (out_profile) {
        *out_profile = s_wcet_profile;
    }
}

void pmu_print_telemetry(void) {
    uint32_t total = s_wcet_profile.total_frame_cycles;
    uint32_t margin_pct = 100;
    if (total < FRAME_DEADLINE_CYCLES) {
        margin_pct = 100 - (total * 100 / FRAME_DEADLINE_CYCLES);
    } else {
        margin_pct = 0;
    }

    uart_puts("\n[WCET] Performance Monitor Unit (PMU) Timing Profile:\n");
    uart_printf("       Sensor Ingest & Mailbox: %5u cycles\n", s_wcet_profile.ingest_cycles);
    uart_printf("       AMP Node Compute & Sync: %5u cycles\n", s_wcet_profile.compute_sync_cycles);
    uart_printf("       2oo3 Voter & Lockstep  : %5u cycles\n", s_wcet_profile.voter_lockstep_cycles);
    uart_printf("       Telemetry Formatting   : %5u cycles\n", s_wcet_profile.telemetry_cycles);
    uart_printf("       Total Frame Execution  : %5u cycles\n", total);
    uart_printf("       Frame Deadline Budget  : %5u cycles\n", FRAME_DEADLINE_CYCLES);
    uart_printf("       Timing Safety Margin   : %3u%% Headroom\n", margin_pct);
}
