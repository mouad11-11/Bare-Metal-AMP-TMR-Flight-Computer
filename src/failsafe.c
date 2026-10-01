#include "failsafe.h"
#include "uart.h"
#include "amp.h"

/* Latched Fail-Safe State Variables (Tree 6) */
static volatile bool g_failsafe_latched = false;
static volatile failsafe_reason_t g_latched_reason = REASON_NONE;
static volatile int32_t g_last_actuator_pwm = PWM_NEUTRAL_US;

/* Per-Core Fault Diagnostic Record Table (Tree 4) */
static fault_record_t g_core_faults[4];

void failsafe_init(void) {
    g_failsafe_latched = false;
    g_latched_reason = REASON_NONE;
    g_last_actuator_pwm = PWM_NEUTRAL_US;
    for (int i = 0; i < 4; i++) {
        g_core_faults[i].core_id = (uint32_t)i;
        g_core_faults[i].exc_type = EXC_TYPE_NONE;
        g_core_faults[i].fault_pc = 0;
        g_core_faults[i].spsr = 0;
        g_core_faults[i].dfsr_ifsr = 0;
        g_core_faults[i].dfar_ifar = 0;
    }
}

command_t failsafe_trigger(failsafe_reason_t reason) {
    command_t cmd;
    cmd.pwm_us = FAILSAFE_PWM_US;
    cmd.status = CMD_STATUS_FAILSAFE;
    cmd.frame_id = g_cycle_counter;
    cmd.crc = 0; /* Updated by caller if CRC enabled */

    if (g_failsafe_latched) {
        /* Already latched into SAFE: keep first reason, do not overwrite */
        return cmd;
    }

    /* First entry into SAFE: latch state */
    g_failsafe_latched = true;
    g_latched_reason = reason;
    g_last_actuator_pwm = FAILSAFE_PWM_US;

    /* Read-back confirmation */
    if (g_last_actuator_pwm != FAILSAFE_PWM_US) {
        /* Hardware actuation read-back failure: escalate */
        uart_printf("[CRITICAL] Actuator read-back mismatch during SAFE transition!\n");
    }

    uart_printf("[FAILSAFE] Actuators transitioned to SAFE state. Reason: %s\n",
                failsafe_reason_to_string(reason));

    return cmd;
}

bool failsafe_is_latched(void) {
    return g_failsafe_latched;
}

failsafe_reason_t failsafe_get_latched_reason(void) {
    return g_latched_reason;
}

const char* failsafe_reason_to_string(failsafe_reason_t reason) {
    switch (reason) {
        case REASON_NONE:
            return "NONE";
        case REASON_TOTAL_DISAGREEMENT:
            return "TOTAL_DISAGREEMENT";
        case REASON_INSUFFICIENT_NODES:
            return "INSUFFICIENT_NODES";
        case REASON_WATCHDOG:
            return "WATCHDOG";
        case REASON_EXCEPTION:
            return "EXCEPTION";
        case REASON_POST_FAIL:
            return "POST_FAIL";
        case REASON_INTEGRITY_FAIL:
            return "INTEGRITY_FAIL";
        case REASON_DEADLINE:
            return "DEADLINE";
        case REASON_CANNOT_ARBITRATE:
            return "CANNOT_ARBITRATE";
        default:
            return "UNKNOWN_REASON";
    }
}

const char* command_status_to_string(uint32_t status) {
    switch (status) {
        case CMD_STATUS_OK:
            return "OK";
        case CMD_STATUS_DEGRADED:
            return "DEGRADED";
        case CMD_STATUS_FAILSAFE:
            return "FAILSAFE";
        case CMD_STATUS_INVALID:
            return "INVALID";
        default:
            return "UNKNOWN";
    }
}

const fault_record_t* failsafe_get_core_fault(uint32_t core_id) {
    if (core_id < 4) {
        return &g_core_faults[core_id];
    }
    return NULL;
}

void failsafe_record_fault(const fault_record_t *rec) {
    if (!rec) return;
    uint32_t cid = rec->core_id;
    if (cid < 4) {
        g_core_faults[cid] = *rec;
    }
}

void handle_core_exception(const fault_record_t *rec) {
    if (!rec) return;

    failsafe_record_fault(rec);
    uint32_t cid = rec->core_id;

    const char *exc_name = "UNKNOWN";
    switch (rec->exc_type) {
        case EXC_TYPE_UNDEF: exc_name = "UNDEFINED_INSTRUCTION"; break;
        case EXC_TYPE_SVC:   exc_name = "SUPERVISOR_CALL"; break;
        case EXC_TYPE_PABT:  exc_name = "PREFETCH_ABORT"; break;
        case EXC_TYPE_DABT:  exc_name = "DATA_ABORT"; break;
        case EXC_TYPE_IRQ:   exc_name = "UNEXPECTED_IRQ"; break;
        case EXC_TYPE_FIQ:   exc_name = "UNEXPECTED_FIQ"; break;
        default: break;
    }

    uart_printf("[EXCEPTION] Core %u: %s at PC=0x%08x SPSR=0x%08x FSR=0x%08x FAR=0x%08x\n",
                cid, exc_name, rec->fault_pc, rec->spsr, rec->dfsr_ifsr, rec->dfar_ifar);

    if (cid != 0) {
        /* Node core (1..3): Fail-silent according to Tree 4 */
        core_ready[cid] = 0;
        core_done[cid] = 0;
        dmb();
        uart_printf("[FAULT] Node %u isolated and parked in fail-silent WFE loop.\n", cid);
        while (1) {
            wfe();
        }
    } else {
        /* Core 0 (Master Arbiter): Trigger SAFE immediately */
        failsafe_trigger(REASON_EXCEPTION);
        uart_printf("[FATAL] Master Arbiter exception! Halting in SAFE mode.\n");
        while (1) {
            wfe();
        }
    }
}
