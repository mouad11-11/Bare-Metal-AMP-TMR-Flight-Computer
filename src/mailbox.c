#include "mailbox.h"

static mailbox_channel_t s_inbound_channels[4];
static mailbox_channel_t s_outbound_channels[4];

void mailbox_init(void) {
    for (uint32_t i = 0; i < 4; i++) {
        s_inbound_channels[i].active_idx = 0;
        s_inbound_channels[i].buffers[0].sequence_id = 0;
        s_inbound_channels[i].buffers[0].payload = 0;
        s_inbound_channels[i].buffers[0].timestamp_token = 0;
        s_inbound_channels[i].buffers[0].crc32 = 0;

        s_inbound_channels[i].buffers[1].sequence_id = 0;
        s_inbound_channels[i].buffers[1].payload = 0;
        s_inbound_channels[i].buffers[1].timestamp_token = 0;
        s_inbound_channels[i].buffers[1].crc32 = 0;

        s_outbound_channels[i].active_idx = 0;
        s_outbound_channels[i].buffers[0].sequence_id = 0;
        s_outbound_channels[i].buffers[0].payload = 0;
        s_outbound_channels[i].buffers[0].timestamp_token = 0;
        s_outbound_channels[i].buffers[0].crc32 = 0;

        s_outbound_channels[i].buffers[1].sequence_id = 0;
        s_outbound_channels[i].buffers[1].payload = 0;
        s_outbound_channels[i].buffers[1].timestamp_token = 0;
        s_outbound_channels[i].buffers[1].crc32 = 0;
    }
    dmb();
}

uint32_t mailbox_calc_crc(const mailbox_msg_t *msg) {
    if (!msg) return 0;
    const uint8_t *data = (const uint8_t *)msg;
    size_t length = sizeof(uint32_t) + sizeof(int32_t) + sizeof(uint32_t); /* 12 bytes */
    uint32_t crc = MAILBOX_CRC_SEED;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

bool mailbox_verify_crc(const mailbox_msg_t *msg) {
    if (!msg) return false;
    return msg->crc32 == mailbox_calc_crc(msg);
}

void mailbox_send_input(uint32_t core_id, uint32_t seq, int32_t sensor_val) {
    if (core_id < 1 || core_id > 3) return;
    mailbox_channel_t *ch = &s_inbound_channels[core_id];
    uint32_t next_idx = 1U - ch->active_idx;

    mailbox_msg_t *msg = &ch->buffers[next_idx];
    msg->sequence_id = seq;
    msg->payload = sensor_val;
    msg->timestamp_token = seq;
    msg->crc32 = mailbox_calc_crc(msg);

    dmb();
    ch->active_idx = next_idx;
    dmb();
}

mailbox_status_t mailbox_read_input(uint32_t core_id, uint32_t expected_seq, int32_t *out_val) {
    if (!out_val) return MAILBOX_ERR_NULL;
    if (core_id < 1 || core_id > 3) return MAILBOX_ERR_INVALID_CORE;

    mailbox_channel_t *ch = &s_inbound_channels[core_id];
    uint32_t active = ch->active_idx;
    dmb();

    mailbox_msg_t msg = ch->buffers[active];
    dmb();

    if (!mailbox_verify_crc(&msg)) {
        return MAILBOX_ERR_CRC;
    }
    if (expected_seq != 0 && msg.sequence_id < expected_seq) {
        return MAILBOX_ERR_SEQUENCE;
    }

    *out_val = msg.payload;
    return MAILBOX_OK;
}

void mailbox_send_output(uint32_t core_id, uint32_t seq, int32_t pwm_val) {
    if (core_id < 1 || core_id > 3) return;
    mailbox_channel_t *ch = &s_outbound_channels[core_id];
    uint32_t next_idx = 1U - ch->active_idx;

    mailbox_msg_t *msg = &ch->buffers[next_idx];
    msg->sequence_id = seq;
    msg->payload = pwm_val;
    msg->timestamp_token = seq;
    msg->crc32 = mailbox_calc_crc(msg);

    dmb();
    ch->active_idx = next_idx;
    dmb();
}

mailbox_status_t mailbox_read_output(uint32_t core_id, uint32_t expected_seq, int32_t *out_val) {
    if (!out_val) return MAILBOX_ERR_NULL;
    if (core_id < 1 || core_id > 3) return MAILBOX_ERR_INVALID_CORE;

    mailbox_channel_t *ch = &s_outbound_channels[core_id];
    uint32_t active = ch->active_idx;
    dmb();

    mailbox_msg_t msg = ch->buffers[active];
    dmb();

    if (!mailbox_verify_crc(&msg)) {
        return MAILBOX_ERR_CRC;
    }
    if (expected_seq != 0 && msg.sequence_id < expected_seq) {
        return MAILBOX_ERR_SEQUENCE;
    }

    *out_val = msg.payload;
    return MAILBOX_OK;
}
