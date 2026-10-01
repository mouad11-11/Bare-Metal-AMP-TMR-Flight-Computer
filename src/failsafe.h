#ifndef FAILSAFE_H
#define FAILSAFE_H

#include "types.h"
#include "config.h"

/* Command Status Values */
typedef enum {
    CMD_STATUS_OK = 0,
    CMD_STATUS_DEGRADED = 1,
    CMD_STATUS_FAILSAFE = 2,
    CMD_STATUS_INVALID = 3
} command_status_t;

/* Fail-Safe Reason Codes (Tree 1 & Tree 6) */
typedef enum {
    REASON_NONE = 0,
    REASON_TOTAL_DISAGREEMENT,
    REASON_INSUFFICIENT_NODES,
    REASON_WATCHDOG,
    REASON_EXCEPTION,
    REASON_POST_FAIL,
    REASON_INTEGRITY_FAIL,
    REASON_DEADLINE,
    REASON_CANNOT_ARBITRATE
} failsafe_reason_t;

/* Actuator Command Structure (Decouples status from numeric value) */
typedef struct {
    int32_t pwm_us;
    uint32_t status;    /* command_status_t */
    uint32_t frame_id;
    uint32_t crc;
} command_t;

/* Hardware Exception Types */
typedef enum {
    EXC_TYPE_NONE = 0,
    EXC_TYPE_UNDEF = 1,
    EXC_TYPE_SVC = 2,
    EXC_TYPE_PABT = 3,
    EXC_TYPE_DABT = 4,
    EXC_TYPE_IRQ = 5,
    EXC_TYPE_FIQ = 6
} exception_type_t;

/* Per-Core Hardware Fault Diagnostic Record */
typedef struct {
    uint32_t core_id;
    uint32_t exc_type;      /* exception_type_t */
    uint32_t fault_pc;      /* Program counter where exception occurred */
    uint32_t spsr;          /* Saved Processor Status Register */
    uint32_t dfsr_ifsr;     /* Data/Instruction Fault Status Register */
    uint32_t dfar_ifar;     /* Data/Instruction Fault Address Register */
} fault_record_t;

/* Fail-Safe and Latching State Machine API */
void failsafe_init(void);
command_t failsafe_trigger(failsafe_reason_t reason);
bool failsafe_is_latched(void);
failsafe_reason_t failsafe_get_latched_reason(void);
const char* failsafe_reason_to_string(failsafe_reason_t reason);
const char* command_status_to_string(uint32_t status);

/* Exception Handler Integration */
void failsafe_record_fault(const fault_record_t *rec);
void handle_core_exception(const fault_record_t *rec);
const fault_record_t* failsafe_get_core_fault(uint32_t core_id);

#endif /* FAILSAFE_H */
