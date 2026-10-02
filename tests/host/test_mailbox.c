#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "mailbox.h"

static uint32_t s_assertions = 0;

#define TEST_ASSERT(cond) do { \
    s_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

static void test_nominal_transfer(void) {
    printf("[RUN] T-MBOX-001: Nominal Double-Buffered CRC Transfer\n");
    mailbox_init();

    int32_t val = 0;

    /* Send input to Cores 1, 2, 3 */
    mailbox_send_input(1, 1, 1500);
    mailbox_send_input(2, 1, 1500);
    mailbox_send_input(3, 1, 1500);

    TEST_ASSERT(mailbox_read_input(1, 1, &val) == MAILBOX_OK && val == 1500);
    TEST_ASSERT(mailbox_read_input(2, 1, &val) == MAILBOX_OK && val == 1500);
    TEST_ASSERT(mailbox_read_input(3, 1, &val) == MAILBOX_OK && val == 1500);

    /* Send output from Cores 1, 2, 3 */
    mailbox_send_output(1, 1, 1515);
    mailbox_send_output(2, 1, 1485);
    mailbox_send_output(3, 1, 1500);

    TEST_ASSERT(mailbox_read_output(1, 1, &val) == MAILBOX_OK && val == 1515);
    TEST_ASSERT(mailbox_read_output(2, 1, &val) == MAILBOX_OK && val == 1485);
    TEST_ASSERT(mailbox_read_output(3, 1, &val) == MAILBOX_OK && val == 1500);
}

static void test_tri_buffering_and_overrun_immunity(void) {
    printf("[RUN] T-MBOX-002: Tri-Buffering Lock-Free Rotation & Overrun Immunity\n");
    mailbox_init();
    int32_t val = 0;

    mailbox_channel_t *ch1 = mailbox_get_channel(1, true);
    TEST_ASSERT(ch1 != NULL);

    /* Frame 1: Writer writes to slot 1 (since read=0, latest=0) */
    mailbox_send_input(1, 1, 100);
    TEST_ASSERT(ch1->latest_slot == 1);
    TEST_ASSERT(mailbox_read_input(1, 1, &val) == MAILBOX_OK && val == 100);
    TEST_ASSERT(ch1->read_slot == 1);

    /* Frame 2: Writer writes to next available slot */
    mailbox_send_input(1, 2, 200);
    TEST_ASSERT(mailbox_read_input(1, 2, &val) == MAILBOX_OK && val == 200);

    /* Overrun / Writer-Laps-Reader Race Test:
     * Simulate consumer currently holding read_slot = 1 */
    ch1->read_slot = 1;
    ch1->latest_slot = 2;
    /* Producer sends frame 10: free slot must not be 1 (read_slot) nor 2 (latest_slot) -> free=0 */
    mailbox_send_input(1, 10, 1000);
    TEST_ASSERT(ch1->write_slot == 0);
    TEST_ASSERT(ch1->latest_slot == 0);
    TEST_ASSERT(ch1->read_slot == 1); /* Read slot untouched */

    /* Producer immediately sends frame 11 before consumer updates read_slot:
     * read_slot is still 1, latest_slot is 0 -> free slot must be 2! */
    mailbox_send_input(1, 11, 1100);
    TEST_ASSERT(ch1->write_slot == 2);
    TEST_ASSERT(ch1->latest_slot == 2);
    TEST_ASSERT(ch1->read_slot == 1); /* Read slot 1 STILL untouched! Zero corruption */

    /* Consumer now reads: fetches latest (slot 2, value 1100) */
    TEST_ASSERT(mailbox_read_input(1, 11, &val) == MAILBOX_OK && val == 1100);
    TEST_ASSERT(ch1->read_slot == 2);

    /* Same for outbound channel */
    mailbox_channel_t *ch_out = mailbox_get_channel(2, false);
    TEST_ASSERT(ch_out != NULL);
    mailbox_send_output(2, 1, 500);
    TEST_ASSERT(mailbox_read_output(2, 1, &val) == MAILBOX_OK && val == 500);
    mailbox_send_output(2, 2, 600);
    TEST_ASSERT(mailbox_read_output(2, 2, &val) == MAILBOX_OK && val == 600);
}

static void test_crc_and_sequence_errors(void) {
    printf("[RUN] T-MBOX-003: CRC Corruption and Sequence Validation\n");
    int32_t val = 0;

    /* Stale sequence rejection */
    mailbox_send_input(1, 5, 1234);
    TEST_ASSERT(mailbox_read_input(1, 10, &val) == MAILBOX_ERR_SEQUENCE);

    mailbox_send_output(1, 5, 1234);
    TEST_ASSERT(mailbox_read_output(1, 10, &val) == MAILBOX_ERR_SEQUENCE);

    /* CRC verification calculation */
    mailbox_msg_t msg;
    msg.sequence_id = 42;
    msg.payload = 1500;
    msg.timestamp_token = 42;
    msg.crc32 = mailbox_calc_crc(&msg);
    TEST_ASSERT(mailbox_verify_crc(&msg) == true);

    /* Corrupted CRC */
    msg.crc32 ^= 0x01;
    TEST_ASSERT(mailbox_verify_crc(&msg) == false);

    /* Corrupted payload */
    msg.crc32 = mailbox_calc_crc(&msg);
    msg.payload ^= 0x80;
    TEST_ASSERT(mailbox_verify_crc(&msg) == false);
}

static void test_boundary_checks(void) {
    printf("[RUN] T-MBOX-004: Boundary & NULL Pointer Defensive Handling\n");
    int32_t val = 0;

    /* Invalid core IDs */
    mailbox_send_input(0, 1, 100); /* Core 0 is sender, invalid target */
    mailbox_send_input(4, 1, 100);
    TEST_ASSERT(mailbox_read_input(0, 1, &val) == MAILBOX_ERR_INVALID_CORE);
    TEST_ASSERT(mailbox_read_input(4, 1, &val) == MAILBOX_ERR_INVALID_CORE);

    mailbox_send_output(0, 1, 100);
    mailbox_send_output(4, 1, 100);
    TEST_ASSERT(mailbox_read_output(0, 1, &val) == MAILBOX_ERR_INVALID_CORE);
    TEST_ASSERT(mailbox_read_output(4, 1, &val) == MAILBOX_ERR_INVALID_CORE);

    /* NULL pointers */
    TEST_ASSERT(mailbox_read_input(1, 1, NULL) == MAILBOX_ERR_NULL);
    TEST_ASSERT(mailbox_read_output(1, 1, NULL) == MAILBOX_ERR_NULL);
    TEST_ASSERT(mailbox_calc_crc(NULL) == 0);
    TEST_ASSERT(mailbox_verify_crc(NULL) == false);
}

int main(void) {
    printf("==============================================================================\n");
    printf("  Host Tri-Buffered Lock-Free CRC Mailbox Test Runner (Native C Unit Harness)\n");
    printf("==============================================================================\n");

    test_nominal_transfer();
    test_tri_buffering_and_overrun_immunity();
    test_crc_and_sequence_errors();
    test_boundary_checks();

    printf("==============================================================================\n");
    printf("  Results: %u assertions executed, 0 failed\n", s_assertions);
    printf("==============================================================================\n");

    return 0;
}
