// SPDX-License-Identifier: MIT
/*
 * signal_stats — running statistics: mean, variance, min, max, count
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Welford's online algorithm for numerically stable variance.
 * Pure C99, no heap, no OS, fully reentrant per-context.
 */

#ifndef SIGNAL_STATS_H
#define SIGNAL_STATS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIGNAL_STATS_OK        =  0,
    SIGNAL_STATS_ERR_PARAM = -1,
} signal_stats_err_t;

typedef struct {
    float    mean;     /* internal: running mean */
    float    m2;       /* internal: sum of squared differences (Welford) */
    float    min_val;  /* internal: running minimum */
    float    max_val;  /* internal: running maximum */
    uint32_t count;    /* internal: sample count */
} signal_stats_t;

/** @thread_safe: no @isr_safe: yes */
signal_stats_err_t signal_stats_init(signal_stats_t *ctx);

/** Feed a sample.  Returns SIGNAL_STATS_ERR_PARAM, leaving the state unchanged,
 *  for NaN, Inf, or a sample that would overflow the mean or the variance sum.
 *  @thread_safe: no @isr_safe: yes */
signal_stats_err_t signal_stats_feed(signal_stats_t *ctx, float sample);

/** Get mean. Returns 0 if no samples. @thread_safe: no @isr_safe: yes */
float signal_stats_mean(const signal_stats_t *ctx);

/** Get population variance. Returns 0 if < 1 sample. @thread_safe: no @isr_safe: yes */
float signal_stats_variance(const signal_stats_t *ctx);

/** Get min. Returns 0 if no samples. @thread_safe: no @isr_safe: yes */
float signal_stats_min(const signal_stats_t *ctx);

/** Get max. Returns 0 if no samples. @thread_safe: no @isr_safe: yes */
float signal_stats_max(const signal_stats_t *ctx);

/** Get sample count. @thread_safe: no @isr_safe: yes */
uint32_t signal_stats_count(const signal_stats_t *ctx);

/** Get range (max - min). @thread_safe: no @isr_safe: yes */
float signal_stats_range(const signal_stats_t *ctx);

/** Reset. @thread_safe: no @isr_safe: yes */
signal_stats_err_t signal_stats_reset(signal_stats_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_STATS_H */
