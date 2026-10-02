#include "mailbox.h"

#if defined(__arm__) || defined(__thumb__)
#include "memory_map.h"
static mailbox_channel_t s_dummy_channel;

static inline mailbox_channel_t* get_inbound_channel(uint32_t core_id) {
    switch (core_id) {
        case 1:  return (mailbox_channel_t*)(ZONE1_BASE_ADDR + 0x100U);
        case 2:  return (mailbox_channel_t*)(ZONE2_BASE_ADDR + 0x100U);
        case 3:  return (mailbox_channel_t*)(ZONE3_BASE_ADDR + 0x100U);
        default: return &s_dummy_channel;
    }
}

static inline mailbox_channel_t* get_outbound_channel(uint32_t core_id) {
    switch (core_id) {
        case 1:  return (mailbox_channel_t*)(ZONE1_BASE_ADDR + 0x200U);
        case 2:  return (mailbox_channel_t*)(ZONE2_BASE_ADDR + 0x200U);
        case 3:  return (mailbox_channel_t*)(ZONE3_BASE_ADDR + 0x200U);
        default: return &s_dummy_channel;
    }
}
#else
static mailbox_channel_t s_inbound_ch1;
static mailbox_channel_t s_outbound_ch1;
static mailbox_channel_t s_inbound_ch2;
static mailbox_channel_t s_outbound_ch2;
static mailbox_channel_t s_inbound_ch3;
static mailbox_channel_t s_outbound_ch3;
static mailbox_channel_t s_dummy_channel;

static inline mailbox_channel_t* get_inbound_channel(uint32_t core_id) {
    switch (core_id) {
        case 1:  return &s_inbound_ch1;
        case 2:  return &s_inbound_ch2;
        case 3:  return &s_inbound_ch3;
        default: return &s_dummy_channel;
    }
}

static inline mailbox_channel_t* get_outbound_channel(uint32_t core_id) {
    switch (core_id) {
        case 1:  return &s_outbound_ch1;
        case 2:  return &s_outbound_ch2;
        case 3:  return &s_outbound_ch3;
        default: return &s_dummy_channel;
    }
}
#endif

static inline uint32_t get_free_slot(uint32_t read_slot, uint32_t latest_slot) {
    for (uint32_t i = 0; i < 3; i++) {
        if (i != read_slot && i != latest_slot) {
            return i;
        }
    }
    return 0;
}

static void init_channel(mailbox_channel_t *ch) {
    ch->write_slot = 0;
    ch->latest_slot = 0;
    ch->read_slot = 0;
    for (uint32_t b = 0; b < 3; b++) {
        ch->buffers[b].sequence_id = 0;
        ch->buffers[b].payload = 0;
        ch->buffers[b].timestamp_token = 0;
        ch->buffers[b].crc32 = 0;
    }
}

void mailbox_init(void) {
    for (uint32_t c = 1; c <= 3; c++) {
        init_channel(get_inbound_channel(c));
        init_channel(get_outbound_channel(c));
    }
    init_channel(&s_dummy_channel);
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
    mailbox_channel_t *ch = get_inbound_channel(core_id);
    uint32_t free_slot = get_free_slot(ch->read_slot, ch->latest_slot);
    ch->write_slot = free_slot;

    mailbox_msg_t *msg = &ch->buffers[free_slot];
    msg->sequence_id = seq;
    msg->payload = sensor_val;
    msg->timestamp_token = seq;
    msg->crc32 = mailbox_calc_crc(msg);

    dmb();
    ch->latest_slot = free_slot;
    dmb();
}

mailbox_status_t mailbox_read_input(uint32_t core_id, uint32_t expected_seq, int32_t *out_val) {
    if (!out_val) return MAILBOX_ERR_NULL;
    if (core_id < 1 || core_id > 3) return MAILBOX_ERR_INVALID_CORE;

    mailbox_channel_t *ch = get_inbound_channel(core_id);
    uint32_t local_slot = ch->latest_slot;
    dmb();

    mailbox_msg_t msg = ch->buffers[local_slot];
    ch->read_slot = local_slot;
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
    mailbox_channel_t *ch = get_outbound_channel(core_id);
    uint32_t free_slot = get_free_slot(ch->read_slot, ch->latest_slot);
    ch->write_slot = free_slot;

    mailbox_msg_t *msg = &ch->buffers[free_slot];
    msg->sequence_id = seq;
    msg->payload = pwm_val;
    msg->timestamp_token = seq;
    msg->crc32 = mailbox_calc_crc(msg);

    dmb();
    ch->latest_slot = free_slot;
    dmb();
}

mailbox_status_t mailbox_read_output(uint32_t core_id, uint32_t expected_seq, int32_t *out_val) {
    if (!out_val) return MAILBOX_ERR_NULL;
    if (core_id < 1 || core_id > 3) return MAILBOX_ERR_INVALID_CORE;

    mailbox_channel_t *ch = get_outbound_channel(core_id);
    uint32_t local_slot = ch->latest_slot;
    dmb();

    mailbox_msg_t msg = ch->buffers[local_slot];
    ch->read_slot = local_slot;
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

mailbox_channel_t* mailbox_get_channel(uint32_t core_id, bool is_inbound) {
    if (core_id < 1 || core_id > 3) return NULL;
    return is_inbound ? get_inbound_channel(core_id) : get_outbound_channel(core_id);
}
