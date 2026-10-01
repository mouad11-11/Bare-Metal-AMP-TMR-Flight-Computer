#ifndef TYPES_H
#define TYPES_H

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
/* Standard fixed-width integer types for freestanding environment */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef unsigned int        size_t;
typedef signed int          ssize_t;
typedef unsigned int        uintptr_t;
typedef signed int          intptr_t;

#define NULL ((void *)0)

#ifndef __cplusplus
typedef enum { false = 0, true = 1 } bool;
#endif
#endif

/* ARMv7-A Architectural Barrier and Event Primitives */
#if defined(__arm__) || defined(__thumb__)
static inline void dmb(void) {
    __asm__ volatile("dmb" ::: "memory");
}

static inline void dsb(void) {
    __asm__ volatile("dsb" ::: "memory");
}

static inline void isb(void) {
    __asm__ volatile("isb" ::: "memory");
}

static inline void sev(void) {
    __asm__ volatile("sev" ::: "memory");
}

static inline void wfe(void) {
    __asm__ volatile("wfe" ::: "memory");
}

static inline void wfi(void) {
    __asm__ volatile("wfi" ::: "memory");
}

/* Retrieve CPU Core ID from MPIDR (Multiprocessor Affinity Register) */
static inline uint32_t get_core_id(void) {
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    uint32_t cluster = (mpidr >> 8) & 0xFF;
    uint32_t cpu = mpidr & 0xFF;
    return (cluster * 4) + cpu;
}
#else
/* Host stub barriers for unit testing */
static inline void dmb(void) { __asm__ volatile("" ::: "memory"); }
static inline void dsb(void) { __asm__ volatile("" ::: "memory"); }
static inline void isb(void) { __asm__ volatile("" ::: "memory"); }
static inline void sev(void) { }
static inline void wfe(void) { }
static inline void wfi(void) { }
static inline uint32_t get_core_id(void) { return 0; }
#endif

#endif /* TYPES_H */
