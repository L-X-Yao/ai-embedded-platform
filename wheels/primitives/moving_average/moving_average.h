// SPDX-License-Identifier: MIT
/*
 * moving_average — sliding window and exponential moving average filters
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Two filter types:
 *   - SMA (Simple Moving Average): exact window average, uses caller-provided buffer
 *   - EMA (Exponential Moving Average): single-pole IIR, no buffer needed
 *
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOVING_AVERAGE_OK        =  0,
    MOVING_AVERAGE_ERR_PARAM = -1,
} moving_average_err_t;

/* --- Simple Moving Average (SMA) --- */

typedef struct {
    float   *buf;
    size_t   window;
    size_t   count;
    size_t   head;
    float    sum;
} moving_average_sma_t;

/** @thread_safe: no @isr_safe: no */
moving_average_err_t moving_average_sma_init(moving_average_sma_t *ctx,
                                             float *buf, size_t window);

/** @thread_safe: no @isr_safe: no */
moving_average_err_t moving_average_sma_reset(moving_average_sma_t *ctx);

/** @thread_safe: no @isr_safe: yes */
float moving_average_sma_feed(moving_average_sma_t *ctx, float sample);

/** @thread_safe: no @isr_safe: yes */
float moving_average_sma_get(const moving_average_sma_t *ctx);

/** @thread_safe: no @isr_safe: yes */
bool moving_average_sma_is_full(const moving_average_sma_t *ctx);

/* --- Exponential Moving Average (EMA) --- */

typedef struct {
    float alpha;
    float value;
    bool  primed;
} moving_average_ema_t;

/** @thread_safe: no @isr_safe: no */
moving_average_err_t moving_average_ema_init(moving_average_ema_t *ctx, float alpha);

/** @thread_safe: no @isr_safe: no */
moving_average_err_t moving_average_ema_reset(moving_average_ema_t *ctx);

/** @thread_safe: no @isr_safe: yes */
float moving_average_ema_feed(moving_average_ema_t *ctx, float sample);

/** @thread_safe: no @isr_safe: yes */
float moving_average_ema_get(const moving_average_ema_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MOVING_AVERAGE_H */
