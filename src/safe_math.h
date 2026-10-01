#ifndef SAFE_MATH_H
#define SAFE_MATH_H

#include "types.h"

#define SAFE_INT32_MAX ((int32_t)0x7FFFFFFF)
#define SAFE_INT32_MIN ((int32_t)(-2147483647 - 1))

/**
 * @brief Saturating 32-bit signed addition.
 * Guarantees no signed integer overflow undefined behavior (C99 / MISRA C:2012 Rule 12.4).
 */
static inline int32_t safe_add_i32(int32_t a, int32_t b) {
    int64_t res = (int64_t)a + (int64_t)b;
    if (res > (int64_t)SAFE_INT32_MAX) {
        return SAFE_INT32_MAX;
    }
    if (res < (int64_t)SAFE_INT32_MIN) {
        return SAFE_INT32_MIN;
    }
    return (int32_t)res;
}

/**
 * @brief Saturating 32-bit signed subtraction.
 */
static inline int32_t safe_sub_i32(int32_t a, int32_t b) {
    int64_t res = (int64_t)a - (int64_t)b;
    if (res > (int64_t)SAFE_INT32_MAX) {
        return SAFE_INT32_MAX;
    }
    if (res < (int64_t)SAFE_INT32_MIN) {
        return SAFE_INT32_MIN;
    }
    return (int32_t)res;
}

/**
 * @brief Saturating 32-bit signed multiplication.
 */
static inline int32_t safe_mul_i32(int32_t a, int32_t b) {
    int64_t res = (int64_t)a * (int64_t)b;
    if (res > (int64_t)SAFE_INT32_MAX) {
        return SAFE_INT32_MAX;
    }
    if (res < (int64_t)SAFE_INT32_MIN) {
        return SAFE_INT32_MIN;
    }
    return (int32_t)res;
}

/**
 * @brief Safe 32-bit signed division with divide-by-zero protection.
 */
static inline int32_t safe_div_i32(int32_t num, int32_t den, int32_t fallback) {
    if (den == 0) {
        return fallback;
    }
    if (num == SAFE_INT32_MIN && den == -1) {
        return SAFE_INT32_MAX; /* Saturate on INT32_MIN / -1 */
    }
    return num / den;
}

/**
 * @brief Saturating absolute value of 32-bit signed integer.
 * Maps SAFE_INT32_MIN to SAFE_INT32_MAX without overflow.
 */
static inline int32_t safe_abs_i32(int32_t a) {
    if (a == SAFE_INT32_MIN) {
        return SAFE_INT32_MAX;
    }
    return (a < 0) ? -a : a;
}

/**
 * @brief Safe clamping of 32-bit signed integer between min_val and max_val.
 * Defensively handles inverted bounds (min_val > max_val).
 */
static inline int32_t safe_clamp_i32(int32_t val, int32_t min_val, int32_t max_val) {
    if (min_val > max_val) {
        int32_t tmp = min_val;
        min_val = max_val;
        max_val = tmp;
    }
    if (val < min_val) {
        return min_val;
    }
    if (val > max_val) {
        return max_val;
    }
    return val;
}

/**
 * @brief Safe absolute difference between two 32-bit signed integers.
 * Clamps to SAFE_INT32_MAX if difference exceeds positive range.
 */
static inline int32_t safe_diff_i32(int32_t a, int32_t b) {
    int64_t diff = (int64_t)a - (int64_t)b;
    if (diff < 0) {
        diff = -diff;
    }
    if (diff > (int64_t)SAFE_INT32_MAX) {
        return SAFE_INT32_MAX;
    }
    return (int32_t)diff;
}

#endif /* SAFE_MATH_H */
