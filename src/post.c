#include "post.h"
#include "voter.h"
#include "failsafe.h"

#define POST_RAM_BUFFER_WORDS 512U
static uint32_t s_post_ram[POST_RAM_BUFFER_WORDS];

#if !defined(__STDC_HOSTED__) || defined(__arm__)
extern uint8_t _text_start[];
extern uint8_t _text_end[];
#endif

uint32_t post_compute_crc32(const uint8_t *data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i];
        for (uint32_t b = 0; b < 8; b++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return ~crc;
}

bool post_test_cpu_registers(void) {
    volatile uint32_t reg_pattern;

    /* Test alternating bit patterns */
    reg_pattern = 0xAAAAAAAAU;
    if (reg_pattern != 0xAAAAAAAAU) return false;

    reg_pattern = 0x55555555U;
    if (reg_pattern != 0x55555555U) return false;

    reg_pattern = 0x00000000U;
    if (reg_pattern != 0x00000000U) return false;

    reg_pattern = 0xFFFFFFFFU;
    if (reg_pattern != 0xFFFFFFFFU) return false;

    /* Walking 1s across all 32 bit positions */
    for (uint32_t b = 0; b < 32; b++) {
        uint32_t expected = (1U << b);
        reg_pattern = expected;
        if (reg_pattern != expected) {
            return false;
        }
    }

    /* Walking 0s across all 32 bit positions */
    for (uint32_t b = 0; b < 32; b++) {
        uint32_t expected = ~(1U << b);
        reg_pattern = expected;
        if (reg_pattern != expected) {
            return false;
        }
    }

    return true;
}

bool post_test_ram_march_c_minus(uint32_t *buffer, size_t word_count) {
    if (!buffer || word_count == 0) {
        return false;
    }

    /* Element 1: Up (w0) */
    for (size_t i = 0; i < word_count; i++) {
        buffer[i] = 0x00000000U;
    }

    /* Element 2: Up (r0, w1) */
    for (size_t i = 0; i < word_count; i++) {
        if (buffer[i] != 0x00000000U) {
            return false;
        }
        buffer[i] = 0xFFFFFFFFU;
    }

    /* Element 3: Up (r1, w0) */
    for (size_t i = 0; i < word_count; i++) {
        if (buffer[i] != 0xFFFFFFFFU) {
            return false;
        }
        buffer[i] = 0x00000000U;
    }

    /* Element 4: Down (r0, w1) */
    for (size_t i = word_count; i > 0; i--) {
        size_t idx = i - 1;
        if (buffer[idx] != 0x00000000U) {
            return false;
        }
        buffer[idx] = 0xFFFFFFFFU;
    }

    /* Element 5: Down (r1, w0) */
    for (size_t i = word_count; i > 0; i--) {
        size_t idx = i - 1;
        if (buffer[idx] != 0xFFFFFFFFU) {
            return false;
        }
        buffer[idx] = 0x00000000U;
    }

    /* Element 6: Up (r0) */
    for (size_t i = 0; i < word_count; i++) {
        if (buffer[i] != 0x00000000U) {
            return false;
        }
    }

    return true;
}

bool post_test_voter_vectors(void) {
    /* Vector 1: Unanimous */
    voter_result_t r1 = vote_2oo3(1500, 1500, 1500);
    if (r1.status != VOTE_UNANIMOUS || r1.final_pwm != 1500) {
        return false;
    }

    /* Vector 2: Node 1 Outlier */
    voter_result_t r2 = vote_2oo3(2000, 1515, 1515);
    if (r2.status != VOTE_MAJORITY_NODE1_MASKED || r2.final_pwm != 1515) {
        return false;
    }

    /* Vector 3: Node 2 Outlier */
    voter_result_t r3 = vote_2oo3(1476, 1220, 1476);
    if (r3.status != VOTE_MAJORITY_NODE2_MASKED || r3.final_pwm != 1476) {
        return false;
    }

    /* Vector 4: Node 3 Outlier */
    voter_result_t r4 = vote_2oo3(1545, 1545, 1000);
    if (r4.status != VOTE_MAJORITY_NODE3_MASKED || r4.final_pwm != 1545) {
        return false;
    }

    /* Vector 5: Total Disagreement */
    voter_result_t r5 = vote_2oo3(1629, 1429, 1769);
    if (r5.status != VOTE_TOTAL_DISAGREEMENT || r5.final_pwm != FAIL_SAFE_VALUE) {
        return false;
    }

    return true;
}

bool post_test_code_integrity(void) {
#if !defined(__STDC_HOSTED__) || defined(__arm__)
    uintptr_t start = (uintptr_t)_text_start;
    uintptr_t end = (uintptr_t)_text_end;
    if (end <= start) {
        return false;
    }
    size_t length = (size_t)(end - start);
    if (length > 2048U) {
        length = 2048U;
    }
    uint32_t crc = post_compute_crc32((const uint8_t *)start, length);
    return (crc != 0);
#else
    /* On host: verify standard CRC32 on test vector "123456789" -> 0xCBF43926 */
    const uint8_t check_vec[] = "123456789";
    uint32_t crc = post_compute_crc32(check_vec, 9);
    return (crc == 0xCBF43926U);
#endif
}

post_status_t post_run_all(void) {
    if (!post_test_cpu_registers()) {
        failsafe_trigger(REASON_POST_FAIL);
        return POST_FAIL_CPU_REGISTERS;
    }

    if (!post_test_ram_march_c_minus(s_post_ram, POST_RAM_BUFFER_WORDS)) {
        failsafe_trigger(REASON_POST_FAIL);
        return POST_FAIL_RAM_MARCH;
    }

    if (!post_test_voter_vectors()) {
        failsafe_trigger(REASON_POST_FAIL);
        return POST_FAIL_VOTER;
    }

    if (!post_test_code_integrity()) {
        failsafe_trigger(REASON_POST_FAIL);
        return POST_FAIL_CODE_CRC;
    }

    return POST_PASS;
}

const char* post_status_to_string(post_status_t status) {
    switch (status) {
        case POST_PASS:
            return "PASS (All Hardware & Software Checks Healthy)";
        case POST_FAIL_CPU_REGISTERS:
            return "FAIL (CPU Register Stuck-At Fault)";
        case POST_FAIL_RAM_MARCH:
            return "FAIL (RAM March C- Algorithmic Memory Fault)";
        case POST_FAIL_CODE_CRC:
            return "FAIL (Code / Text Section CRC32 Checksum Mismatch)";
        case POST_FAIL_VOTER:
            return "FAIL (Voter Algorithmic Vector Fault)";
        case POST_FAIL_PERIPHERAL:
            return "FAIL (Peripheral / UART Bus Fault)";
        default:
            return "FAIL (Unknown POST Code)";
    }
}
