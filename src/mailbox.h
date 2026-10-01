#ifndef MAILBOX_H
#define MAILBOX_H

#include "types.h"

#define MAILBOX_CRC_SEED 0xFFFFFFFFU

typedef struct {
    uint32_t sequence_id;     /* Frame sequence counter */
    int32_t  payload;         /* Data value (sensor reading or PWM command) */
    uint32_t timestamp_token; /* Frame cycle / timestamp token */
    uint32_t crc32;           /* CRC-32 checksum of above 3 fields */
} mailbox_msg_t;

typedef struct {
    mailbox_msg_t buffers[2];
    volatile uint32_t active_idx;
} mailbox_channel_t;

typedef enum {
    MAILBOX_OK = 0,
    MAILBOX_ERR_CRC = 1,
    MAILBOX_ERR_SEQUENCE = 2,
    MAILBOX_ERR_INVALID_CORE = 3,
    MAILBOX_ERR_NULL = 4
} mailbox_status_t;

/**
 * @brief Initialize inter-core double-buffered mailboxes.
 */
void mailbox_init(void);

/**
 * @brief Compute standard CRC32 checksum over mailbox packet fields.
 */
uint32_t mailbox_calc_crc(const mailbox_msg_t *msg);

/**
 * @brief Verify CRC32 checksum of a mailbox packet.
 */
bool mailbox_verify_crc(const mailbox_msg_t *msg);

/**
 * @brief Transmit sensor input to secondary node via double-buffered CRC mailbox.
 */
void mailbox_send_input(uint32_t core_id, uint32_t seq, int32_t sensor_val);

/**
 * @brief Ingest sensor input on secondary node with CRC and sequence validation.
 */
mailbox_status_t mailbox_read_input(uint32_t core_id, uint32_t expected_seq, int32_t *out_val);

/**
 * @brief Transmit computed actuator PWM to Core 0 via double-buffered CRC mailbox.
 */
void mailbox_send_output(uint32_t core_id, uint32_t seq, int32_t pwm_val);

/**
 * @brief Ingest computed actuator PWM on Core 0 with CRC and sequence validation.
 */
mailbox_status_t mailbox_read_output(uint32_t core_id, uint32_t expected_seq, int32_t *out_val);

#endif /* MAILBOX_H */
