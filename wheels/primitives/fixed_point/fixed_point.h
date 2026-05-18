// SPDX-License-Identifier: MIT
/*
 * fixed_point — Q16.16 fixed-point math library
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Format: signed 32-bit, 16 bits integer + 16 bits fraction.
 * Range:  -32768.0 to +32767.99998 (approx).
 * Resolution: 1/65536 ~ 0.0000153.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Q16.16 fixed-point type. */
typedef int32_t fp_t;

#define FP_FRAC_BITS  16
#define FP_FRAC_MASK  0x0000FFFF
#define FP_ONE        ((fp_t)0x00010000)      /* 1.0  */
#define FP_HALF       ((fp_t)0x00008000)      /* 0.5  */
#define FP_PI         ((fp_t)205887)          /* 3.14159 in Q16.16 */
#define FP_MAX        ((fp_t)INT32_MAX)
#define FP_MIN        ((fp_t)INT32_MIN)

/* ── Conversion ───────────────────────────────────────────────── */

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t    fp_from_int(int16_t val);

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t    fp_from_float(float val);

/** Truncate toward zero. @thread_safe: yes @isr_safe: yes */
int16_t fp_to_int(fp_t val);

/** Round to nearest integer. @thread_safe: yes @isr_safe: no */
int16_t fp_to_int_rounded(fp_t val);

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
float   fp_to_float(fp_t val);

/* ── Arithmetic (overflow-safe via int64_t intermediates) ──── */

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t fp_add(fp_t a, fp_t b);

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t fp_sub(fp_t a, fp_t b);

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t fp_mul(fp_t a, fp_t b);

/** Division. b==0 returns FP_MAX or FP_MIN based on sign of a.
 *  @thread_safe: yes (pure function, no state) @isr_safe: yes */
fp_t fp_div(fp_t a, fp_t b);

/* ── Comparison ───────────────────────────────────────────────── */

/** @thread_safe: yes @isr_safe: yes */
bool fp_gt(fp_t a, fp_t b);

/** @thread_safe: yes @isr_safe: yes */
bool fp_lt(fp_t a, fp_t b);

/** @thread_safe: yes @isr_safe: yes */
bool fp_eq(fp_t a, fp_t b);

/* ── Math functions ───────────────────────────────────────────── */

/** @thread_safe: yes @isr_safe: yes */
fp_t fp_abs(fp_t val);

/** @thread_safe: yes @isr_safe: yes */
fp_t fp_min(fp_t a, fp_t b);

/** @thread_safe: yes @isr_safe: yes */
fp_t fp_max(fp_t a, fp_t b);

/** Clamp val to [lo, hi]. @thread_safe: yes @isr_safe: yes */
fp_t fp_clamp(fp_t val, fp_t lo, fp_t hi);

/** Linear interpolation: a + t*(b-a), t in [0, FP_ONE]. @thread_safe: yes @isr_safe: yes */
fp_t fp_lerp(fp_t a, fp_t b, fp_t t);

/* ── String formatting ────────────────────────────────────────── */

/**
 * Format fp_t to string: "[-]integer.fraction".
 *
 * @param val             Value to format.
 * @param buf             Output buffer (NUL-terminated on success).
 * @param buf_size        Size of buf in bytes.
 * @param decimal_places  Number of fractional digits (0-5).
 * @return chars written (excluding NUL), or -1 on error.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
int fp_to_string(fp_t val, char *buf, uint16_t buf_size, uint8_t decimal_places);

#ifdef __cplusplus
}
#endif

#endif /* FIXED_POINT_H */
