// SPDX-License-Identifier: MIT
/*
 * hysteresis — threshold comparator with hysteresis band
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Implements a Schmitt-trigger-style comparator in software.
 * Switches HIGH when value >= thresh_high, LOW when value <= thresh_low.
 * Values inside the dead band keep the previous state.
 *
 * Pure C99, no heap, no OS.
 * Thread safety: caller owns the hysteresis_t struct.
 */

#ifndef HYSTERESIS_H
#define HYSTERESIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HYSTERESIS_OK        =  0,
    HYSTERESIS_ERR_PARAM = -1,
} hysteresis_err_t;

/* ---------- instance ---------- */

typedef struct {
    float thresh_high;  /* internal: upper switching threshold */
    float thresh_low;   /* internal: lower switching threshold */
    bool  state;        /* internal: current output (false=LOW, true=HIGH) */
    bool  primed;       /* internal: first sample seen? */
} hysteresis_t;

/* ---------- API ---------- */

/**
 * Initialize instance.
 * @param thresh_low   lower threshold (must be < thresh_high, must not be NaN)
 * @param thresh_high  upper threshold (must not be NaN)
 * @thread_safe: no @isr_safe: yes
 */
hysteresis_err_t hysteresis_init(hysteresis_t *ctx,
                                 float thresh_low, float thresh_high);

/**
 * Feed a new sample and return the current output state.
 * NaN samples are silently ignored (state unchanged).
 * First call primes state based on proximity to thresholds.
 * @thread_safe: no @isr_safe: yes
 */
bool hysteresis_feed(hysteresis_t *ctx, float value);

/**
 * Return the current output state without feeding.
 * @thread_safe: no @isr_safe: yes
 */
bool hysteresis_get_state(const hysteresis_t *ctx);

/**
 * Reset to unprimed state.
 * @thread_safe: no @isr_safe: yes
 */
hysteresis_err_t hysteresis_reset(hysteresis_t *ctx);

/**
 * Update thresholds dynamically. thresh_low must be < thresh_high.
 * Does not reset primed state.
 * @thread_safe: no @isr_safe: yes
 */
hysteresis_err_t hysteresis_set_thresholds(hysteresis_t *ctx,
                                           float thresh_low,
                                           float thresh_high);

#ifdef __cplusplus
}
#endif

#endif /* HYSTERESIS_H */
