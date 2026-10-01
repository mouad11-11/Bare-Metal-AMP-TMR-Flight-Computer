#ifndef POST_H
#define POST_H

#include "types.h"
#include "config.h"

/*
 * ==============================================================================
 * Power-On Self-Test (POST) Subsystem (Decision Tree 5)
 *
 * Implements pre-flight hardware and software sanity checks:
 * - CPU register stuck-at test (0xAAAAAAAA, 0x55555555, walking bits)
 * - RAM March C- algorithmic test (stuck-at, transition, coupling faults)
 * - Code & constants section CRC32 integrity check
 * - Voter algorithmic vector self-test
 * - PL011 UART register accessibility
 * ==============================================================================
 */

typedef enum {
    POST_PASS = 0,
    POST_FAIL_CPU_REGISTERS,
    POST_FAIL_RAM_MARCH,
    POST_FAIL_CODE_CRC,
    POST_FAIL_VOTER,
    POST_FAIL_PERIPHERAL
} post_status_t;

/* Execute complete pre-flight Power-On Self-Test suite */
post_status_t post_run_all(void);

/* Diagnostic sub-tests */
bool post_test_cpu_registers(void);
bool post_test_ram_march_c_minus(uint32_t *buffer, size_t word_count);
bool post_test_voter_vectors(void);
uint32_t post_compute_crc32(const uint8_t *data, size_t length);
bool post_test_code_integrity(void);

/* Convert POST status code to diagnostic string */
const char* post_status_to_string(post_status_t status);

#endif /* POST_H */
