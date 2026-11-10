// SPDX-License-Identifier: MIT
/*
 * ewma — integer-only exponential weighted moving average (bit-shift alpha)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Complements the float-based EMA in moving_average/. Uses bit-shift
 * alpha (1/2^shift) for FPU-less MCUs. Pure C99, no heap, no OS.
 *
 * Accuracy: the output is the filter state rounded down. It stays within
 * one count of the real-valued EWMA at every step and equals a constant
 * input once settled, in either direction, about 2^shift * (ln(step) + 1)
 * feeds after a step.
 *
 * Thread safety: caller owns the ewma_t struct. One context feeds and
 * resets. Another context, such as main while an ISR feeds, may call
 * ewma_get(): it reads one aligned 32-bit word and so cannot see half an
 * update, which a read of the 64-bit accumulator could on a 32-bit core.
 */

#ifndef EWMA_H
#define EWMA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    EWMA_OK        =  0,
    EWMA_ERR_PARAM = -1,
} ewma_err_t;

/* ---------- instance ---------- */

typedef struct {
    int64_t           acc;     /* internal: filter state * 2^shift       */
    volatile int32_t  value;   /* internal: last output, acc >> shift    */
    uint8_t           shift;   /* internal: alpha = 1/2^shift            */
    bool              primed;  /* internal: first sample seen?           */
} ewma_t;

/* ---------- API ---------- */

/**
 * Initialize instance.
 * @param shift  1..16, represents alpha = 1/(2^shift). Larger shift = slower tracking.
 * @thread_safe: no @isr_safe: yes
 */
ewma_err_t ewma_init(ewma_t *ctx, uint8_t shift);

/**
 * Feed a new sample and return the filtered value.
 * First sample primes the accumulator to the exact sample value.
 * @thread_safe: no @isr_safe: yes
 */
int32_t ewma_feed(ewma_t *ctx, int32_t sample);

/**
 * Return the current filtered value without feeding.
 * Returns 0 if not yet primed.
 * May run in a different context from ewma_feed(), see the file header.
 * @thread_safe: no @isr_safe: yes
 */
int32_t ewma_get(const ewma_t *ctx);

/**
 * Reset to unprimed state.
 * @thread_safe: no @isr_safe: yes
 */
ewma_err_t ewma_reset(ewma_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EWMA_H */
