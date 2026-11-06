// SPDX-License-Identifier: MIT
/*
 * moving_window — sliding window statistics (min/max/mean/variance)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Maintains min, max, mean, and variance over a fixed-size sliding window.
 * Caller provides float storage array. Pure C99, no heap, fully reentrant.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef MOVING_WINDOW_H
#define MOVING_WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOVING_WINDOW_OK        =  0,
    MOVING_WINDOW_ERR_PARAM = -1,
} moving_window_err_t;

typedef struct {
    float   *buf;       /* caller-provided storage */
    size_t   capacity;  /* max number of samples */
    size_t   count;     /* current number of samples (<= capacity) */
    size_t   head;      /* next write index (circular) */
    float    sum;       /* running sum for mean */
    float    sum_sq;    /* running sum of squares for variance */
} moving_window_t;

/** @thread_safe: no @isr_safe: yes
 *  Initialize context with caller-provided storage.
 *  @param ctx      pointer to context
 *  @param buf      float array of at least @p capacity elements
 *  @param capacity window size (must be > 0)
 */
moving_window_err_t moving_window_init(moving_window_t *ctx,
                                       float *buf, size_t capacity);

/** @thread_safe: no @isr_safe: yes
 *  Push a new sample into the window. Oldest sample is evicted when full.
 *  @return MOVING_WINDOW_ERR_PARAM, leaving the window unchanged, for NaN,
 *          Inf, or a value whose square would overflow the running sum of
 *          squares (any |value| above about 1.8e19).
 */
moving_window_err_t moving_window_push(moving_window_t *ctx, float value);

/** @thread_safe: no  — Returns arithmetic mean, or 0.0f if empty. @isr_safe: yes */
float moving_window_get_mean(const moving_window_t *ctx);

/** @thread_safe: no  — Returns minimum value, or 0.0f if empty. @isr_safe: yes */
float moving_window_get_min(const moving_window_t *ctx);

/** @thread_safe: no  — Returns maximum value, or 0.0f if empty. @isr_safe: yes */
float moving_window_get_max(const moving_window_t *ctx);

/** @thread_safe: no  — Returns population variance, or 0.0f if empty. @isr_safe: yes */
float moving_window_get_variance(const moving_window_t *ctx);

/** @thread_safe: no  — Returns number of samples currently in window. @isr_safe: yes */
size_t moving_window_get_count(const moving_window_t *ctx);

/** @thread_safe: no  — Returns true when window has reached capacity. @isr_safe: yes */
bool moving_window_is_full(const moving_window_t *ctx);

/** @thread_safe: no  — Reset to empty state (keeps buf and capacity). @isr_safe: no */
moving_window_err_t moving_window_reset(moving_window_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MOVING_WINDOW_H */
