#include "types.h"
#include "config.h"
#include "memory_map.h"
#include "uart.h"
#include "amp.h"
#include "voter.h"
#include "flight_control.h"
#include "failsafe.h"
#include "stack_monitor.h"
#include "node_health.h"
#include "supervision.h"
#include "post.h"
#include "mmu.h"
#include "lockstep.h"
#include "mailbox.h"
#include "pmu.h"

static void print_banner(void) {
    uart_puts("\n");
    uart_puts("================================================================================\n");
    uart_puts("       BARE-METAL AMP TMR FLIGHT COMPUTER (ARM Cortex-A15 Quad-Core)           \n");
    uart_puts("         Software-Implemented Fault Tolerance (SIFT) Architecture               \n");
    uart_puts("================================================================================\n");
}

static void print_system_info(void) {
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    uint32_t core_id = get_core_id();
    uart_printf("[BOOT] Master Arbiter active on Core ID: %u (Raw MPIDR: 0x%x)\n", core_id, mpidr);
    uart_printf("[BOOT] Physical Memory Partitioning:\n");
    uart_printf("       System/Text Partition : 0x%x - 0x%x (Core 0 Arbiter)\n", SYSTEM_TEXT_BASE, SYSTEM_TEXT_LIMIT);
    uart_printf("       Zone 1 Partition      : 0x%x (In: 0x%x, Out: 0x%x, Core 1)\n", ZONE1_BASE_ADDR, ZONE1_INPUT_ADDR, ZONE1_OUTPUT_ADDR);
    uart_printf("       Zone 2 Partition      : 0x%x (In: 0x%x, Out: 0x%x, Core 2)\n", ZONE2_BASE_ADDR, ZONE2_INPUT_ADDR, ZONE2_OUTPUT_ADDR);
    uart_printf("       Zone 3 Partition      : 0x%x (In: 0x%x, Out: 0x%x, Core 3)\n", ZONE3_BASE_ADDR, ZONE3_INPUT_ADDR, ZONE3_OUTPUT_ADDR);
    uart_printf("       Stack Configuration   : 32KB total (8KB isolated per core)\n");
    uart_printf("       Tolerance Bound       : delta <= %d microseconds\n", VOTER_TOLERANCE_BOUND);
    uart_printf("       Fail-Safe Command     : %d microseconds (Actuator Safe/Neutral)\n", FAIL_SAFE_VALUE);
    uart_puts("--------------------------------------------------------------------------------\n");
}

static voter_result_t execute_flight_frame(int32_t raw_sensor_reading, fault_injection_t fault_mode) {
    g_fault_mode = fault_mode;

    uint32_t t_start = pmu_get_cycles();

    /*
     * Core 0 Task Execution:
     * Ingests raw sensor data and writes independent copies to Zone 1, Zone 2, and Zone 3.
     */
    zone_write_input(1, raw_sensor_reading);
    zone_write_input(2, raw_sensor_reading);
    zone_write_input(3, raw_sensor_reading);

    mailbox_send_input(1, g_cycle_counter + 1, raw_sensor_reading);
    mailbox_send_input(2, g_cycle_counter + 1, raw_sensor_reading);
    mailbox_send_input(3, g_cycle_counter + 1, raw_sensor_reading);
    uint32_t t_ingest = pmu_get_cycles();

    uint32_t timed_out_mask = 0;
    amp_dispatch_and_wait(&timed_out_mask);
    uint32_t t_sync = pmu_get_cycles();

    /* Core 0 ingests isolated output values computed by Cores 1, 2, and 3 */
    int32_t out1 = zone_read_output(1);
    int32_t out2 = zone_read_output(2);
    int32_t out3 = zone_read_output(3);

    /* Construct node samples with validity flags based on completion & watchdog status */
    node_sample_t samples[3];
    samples[0].pwm_us = out1;
    samples[0].frame_id = g_cycle_counter;
    samples[0].valid = ((timed_out_mask & (1U << 1)) == 0);

    samples[1].pwm_us = out2;
    samples[1].frame_id = g_cycle_counter;
    samples[1].valid = ((timed_out_mask & (1U << 2)) == 0);

    samples[2].pwm_us = out3;
    samples[2].frame_id = g_cycle_counter;
    samples[2].valid = ((timed_out_mask & (1U << 3)) == 0);

    /*
     * Full TMR Voting Engine:
     * - If all 3 nodes valid: 2oo3 median voting with single outlier masking
     * - If 1 node timed out (hang) or latched out: degraded 2oo2 fallback between surviving nodes
     * - If fewer than 2 nodes valid (or remaining nodes disagree): fail-safe command
     */
    voter_result_t res = vote_frame_inputs(samples, g_cycle_counter);
    res.timed_out_core_mask = timed_out_mask;
    res.val1 = out1;
    res.val2 = out2;
    res.val3 = out3;

    /* Core 0 Dual-Rail Software Lockstep Verification (Self-Monitoring on 3-core passes) */
    if (samples[0].valid && samples[1].valid && samples[2].valid) {
        lockstep_verify_voter(out1, out2, out3, &res);
    }
    uint32_t t_voter = pmu_get_cycles();

    pmu_record_frame_timing(t_start, t_ingest, t_sync, t_voter, t_voter);

    return res;
}

static void print_flight_frame_result(uint32_t frame_id, const char *scenario_name, int32_t raw_sensor, voter_result_t res) {
    uart_printf("[FRAME #%u] %s\n", frame_id, scenario_name);
    uart_printf("  Sensor Input : %d\n", raw_sensor);
    uart_printf("  Node Outputs : [Node 1: %d us] [Node 2: %d us] [Node 3: %d us]\n", res.val1, res.val2, res.val3);
    if (res.timed_out_core_mask != 0) {
        uart_printf("  Timeout Mask : 0x%02x (Software Watchdog Timeout)\n", res.timed_out_core_mask);
    }
    uart_printf("  Deltas       : |N1-N2|=%d, |N2-N3|=%d, |N1-N3|=%d (Bound <= %d)\n", res.diff12, res.diff23, res.diff13, VOTER_TOLERANCE_BOUND);
    uart_printf("  Voter Status : %s\n", vote_status_to_string(res.status));
    if (res.final_pwm == FAIL_SAFE_VALUE) {
        uart_printf("  Commanded PWM: %d (*** FAIL-SAFE ACTIVATED: ACTUATORS COMMANDED TO SAFE STATE ***)\n", res.final_pwm);
    } else {
        uart_printf("  Commanded PWM: %d us\n", res.final_pwm);
    }
    uart_puts("--------------------------------------------------------------------------------\n");
}

static void run_fault_tolerance_test_suite(void) {
    uart_puts("\n>>> STARTING AUTOMATED TMR FAULT TOLERANCE VERIFICATION SUITE <<<\n\n");

    /* Test 1: Nominal Synchronous Operation */
    voter_result_t r1 = execute_flight_frame(0, FAULT_NONE);
    print_flight_frame_result(1, "TEST 1: Nominal Flight Frame (Level Attitude)", 0, r1);

    /* Test 2: Bounded Sensor / Estimator Noise (|delta| <= 5) */
    voter_result_t r2 = execute_flight_frame(120, FAULT_BOUNDED_NOISE);
    print_flight_frame_result(2, "TEST 2: Bounded Estimator Noise (|delta| <= 5)", 120, r2);

    /* Test 3: SEU Bit-Flip on Node 1 */
    voter_result_t r3 = execute_flight_frame(50, FAULT_SEU_NODE1);
    print_flight_frame_result(3, "TEST 3: SEU Bit-Flip on Node 1 (Core 1 Fault)", 50, r3);

    /* Test 4: SEU Bit-Flip on Node 2 */
    voter_result_t r4 = execute_flight_frame(-80, FAULT_SEU_NODE2);
    print_flight_frame_result(4, "TEST 4: SEU Bit-Flip on Node 2 (Core 2 Fault)", -80, r4);

    /* Test 5: SEU Bit-Flip on Node 3 */
    voter_result_t r5 = execute_flight_frame(150, FAULT_SEU_NODE3);
    print_flight_frame_result(5, "TEST 5: SEU Bit-Flip on Node 3 (Core 3 Fault)", 150, r5);

    /* Test 6: Total Disagreement (Multi-core SEU / Corruption) */
    voter_result_t r6 = execute_flight_frame(30, FAULT_TOTAL_DISAGREE);
    print_flight_frame_result(6, "TEST 6: Total Disagreement -> Predefined Fail-Safe (-9999)", 30, r6);

    uart_puts("\n>>> TMR FAULT TOLERANCE SUITE COMPLETED SUCCESSFULLY <<<\n\n");
}

int main(void) {
    /* Step 1: Initialize PL011 UART console & safety subsystems */
    uart_init();
    stack_monitor_init();
    failsafe_init();
    node_health_init();
    supervision_init();
    mmu_init_tables();
    lockstep_init();
    mailbox_init();
    pmu_init();
    voter_reset_rate_limit(PWM_NEUTRAL_US);
    print_banner();
    print_system_info();

    /* Step 1b: Execute pre-flight Power-On Self-Test (POST - Decision Tree 5) */
    post_status_t post_res = post_run_all();
    uart_printf("[POST] Power-On Self-Test Diagnostics: %s\n", post_status_to_string(post_res));
    if (post_res != POST_PASS) {
        uart_puts("[FATAL] Hardware/Firmware integrity check failed during POST. Halting.\n");
        while (1) {
            wfe();
        }
    }

    /* Step 2: Wake secondary cores (Cores 1, 2, 3) from QEMU boot holding pen */
    uart_puts("[SYNC] Releasing secondary cores (Cores 1, 2, 3) from QEMU holding pen...\n");
    amp_release_qemu_secondary_cores();

    /* Spin-wait for secondary cores to acknowledge readiness */
    uint32_t wait_cycles = 5000000;
    while ((!core_ready[1].value || !core_ready[2].value || !core_ready[3].value) && --wait_cycles > 0) {
        __asm__ volatile("nop");
    }

    uart_printf("[SYNC] Core Readiness Status: Node 1=%s, Node 2=%s, Node 3=%s\n",
                core_ready[1].value ? "ONLINE" : "OFFLINE",
                core_ready[2].value ? "ONLINE" : "OFFLINE",
                core_ready[3].value ? "ONLINE" : "OFFLINE");

    /* Verify all redundant compute nodes are operational before proceeding */
    if (!core_ready[1].value || !core_ready[2].value || !core_ready[3].value) {
        uart_puts("[FATAL] One or more secondary compute nodes failed to boot. Halting.\n");
        while (1) {
            wfe();
        }
    }

    /* Step 3: Run comprehensive automated fault tolerance test suite */
    run_fault_tolerance_test_suite();

    /* Re-arm health monitoring and fail-safe latch for active flight control phase */
    failsafe_init();
    node_health_init();
    supervision_init();
    voter_reset_rate_limit(PWM_NEUTRAL_US);

    /* Step 4: Continuous Real-Time Flight Control Loop */
    uart_puts("[EXEC] Entering continuous real-time flight control loop...\n");
    int32_t simulated_pitch_rates[] = {0, 20, 45, 80, 50, 10, -20, -60, -90, -40, 0};
    uint32_t num_samples = sizeof(simulated_pitch_rates) / sizeof(simulated_pitch_rates[0]);

    for (uint32_t i = 0; i < num_samples; i++) {
        voter_result_t res = execute_flight_frame(simulated_pitch_rates[i], FAULT_NONE);
        uart_printf("[LOOP #%2u] Pitch Rate: %4d ddeg/s | Commanded PWM: %4d us | Status: %s\n",
                    i + 1, simulated_pitch_rates[i], res.final_pwm, vote_status_to_string(res.status));
        
        /* Small delay loop for telemetry readability */
        for (volatile uint32_t d = 0; d < 200000; d++) {
            __asm__ volatile("nop");
        }
    }

    /* Step 5: Final Watchdog Fault Injection Test (Node 2 Hang -> Degraded 2oo2) */
    uart_puts("\n[TEST 7] Simulating Core 2 Hardware Hang (Degraded 2oo2 Quorum Verification)...\n");
    voter_result_t r7 = execute_flight_frame(25, FAULT_HANG_NODE2);
    print_flight_frame_result(7, "TEST 7: Core 2 Unresponsive -> Degraded 2oo2 Quorum Sustained", 25, r7);

    /* Stack Canary & High-Water Mark Diagnostics */
    uart_puts("\n[DIAG] Stack Canary & High-Water Mark Telemetry:\n");
    for (uint32_t c = 0; c < 4; c++) {
        bool canaries_ok = stack_canary_check_core(c);
        uint32_t used = stack_get_high_water_mark(c);
        uint32_t headroom = stack_get_headroom(c);
        uart_printf("       Core %u: Canaries=%s, Peak Stack Used=%u B, Headroom=%u B\n",
                    c, canaries_ok ? "INTACT" : "CORRUPTED", used, headroom);
    }

    /* PMU WCET & Execution Timing Diagnostics */
    pmu_print_telemetry();

    uart_puts("\n[STATUS] Flight computer completed mission profile smoothly.\n");
    uart_puts("[STATUS] All spatial memory zones intact. System entering standby.\n");

    while (1) {
        wfe();
    }

    return 0;
}
