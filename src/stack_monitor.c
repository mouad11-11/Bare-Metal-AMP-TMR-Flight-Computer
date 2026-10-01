#include "stack_monitor.h"

#if defined(__STDC_HOSTED__) && !defined(__arm__)
uint8_t host_stack_memory[STACK_TOTAL_SIZE] __attribute__((aligned(4096)));
static uint8_t * const stack_top_ptr = &host_stack_memory[STACK_TOTAL_SIZE];
#else
extern uint8_t _stack_top[];
extern uint8_t _stack_bottom[];
static uint8_t * const stack_top_ptr = _stack_top;
#endif

void stack_monitor_init(void) {
    for (uint32_t c = 0; c < 4; c++) {
        uintptr_t partition_top = (uintptr_t)stack_top_ptr - (c * STACK_PER_CORE_SIZE);
        uintptr_t partition_bottom = partition_top - STACK_PER_CORE_SIZE;
        uintptr_t svc_stack_bottom = partition_top - STACK_SVC_SIZE;

        /* Write canary at bottom of core partition */
        volatile uint32_t *p_canary_bottom = (volatile uint32_t *)partition_bottom;
        for (uint32_t i = 0; i < STACK_CANARY_WORDS; i++) {
            p_canary_bottom[i] = STACK_CANARY_VALUE;
        }

        /* Write canary at bottom of SVC stack */
        volatile uint32_t *p_canary_svc = (volatile uint32_t *)svc_stack_bottom;
        for (uint32_t i = 0; i < STACK_CANARY_WORDS; i++) {
            p_canary_svc[i] = STACK_CANARY_VALUE;
        }

        /* Paint watermark pattern */
        uintptr_t paint_limit = partition_top;
        if (c == 0) {
#if defined(__arm__)
            uintptr_t current_sp;
            __asm__ volatile("mov %0, sp" : "=r"(current_sp));
            if (current_sp > (svc_stack_bottom + 256U)) {
                paint_limit = (current_sp - 128U) & ~3U;
            } else {
                paint_limit = svc_stack_bottom + (STACK_CANARY_WORDS * 4U);
            }
#else
            paint_limit = partition_top - 512U;
#endif
        }

        /* Watermark SVC usable stack region */
        volatile uint32_t *p_watermark = (volatile uint32_t *)(svc_stack_bottom + (STACK_CANARY_WORDS * 4U));
        while ((uintptr_t)p_watermark < paint_limit) {
            *p_watermark = STACK_WATERMARK_VALUE;
            p_watermark++;
        }
    }
}

bool stack_canary_check_core(uint32_t core_id) {
    if (core_id >= 4) {
        return false;
    }

    uintptr_t partition_top = (uintptr_t)stack_top_ptr - (core_id * STACK_PER_CORE_SIZE);
    uintptr_t partition_bottom = partition_top - STACK_PER_CORE_SIZE;
    uintptr_t svc_stack_bottom = partition_top - STACK_SVC_SIZE;

    volatile uint32_t *p_canary_bottom = (volatile uint32_t *)partition_bottom;
    for (uint32_t i = 0; i < STACK_CANARY_WORDS; i++) {
        if (p_canary_bottom[i] != STACK_CANARY_VALUE) {
            return false;
        }
    }

    volatile uint32_t *p_canary_svc = (volatile uint32_t *)svc_stack_bottom;
    for (uint32_t i = 0; i < STACK_CANARY_WORDS; i++) {
        if (p_canary_svc[i] != STACK_CANARY_VALUE) {
            return false;
        }
    }

    return true;
}

bool stack_canary_check_all(void) {
    for (uint32_t c = 0; c < 4; c++) {
        if (!stack_canary_check_core(c)) {
            return false;
        }
    }
    return true;
}

uint32_t stack_get_high_water_mark(uint32_t core_id) {
    if (core_id >= 4) {
        return 0;
    }

    uintptr_t partition_top = (uintptr_t)stack_top_ptr - (core_id * STACK_PER_CORE_SIZE);
    uintptr_t svc_stack_bottom = partition_top - STACK_SVC_SIZE;
    uintptr_t canary_end = svc_stack_bottom + (STACK_CANARY_WORDS * 4U);

    volatile uint32_t *p_scan = (volatile uint32_t *)canary_end;
    while ((uintptr_t)p_scan < partition_top) {
        if (*p_scan != STACK_WATERMARK_VALUE) {
            break;
        }
        p_scan++;
    }

    if ((uintptr_t)p_scan >= partition_top) {
        return 0;
    }

    return (uint32_t)(partition_top - (uintptr_t)p_scan);
}

uint32_t stack_get_headroom(uint32_t core_id) {
    if (core_id >= 4) {
        return 0;
    }

    uint32_t used = stack_get_high_water_mark(core_id);
    uint32_t usable_svc = STACK_SVC_SIZE - (STACK_CANARY_WORDS * 4U);
    if (used > usable_svc) {
        return 0;
    }
    return usable_svc - used;
}
