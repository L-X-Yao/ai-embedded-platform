// SPDX-License-Identifier: MIT
/*
 * median_filter — sliding window median filter
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Caller provides the window buffer. Uses insertion sort on each sample
 * for O(N) update. Window sizes 3-31 (odd recommended for true median).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the median_filter_t struct.
 */

#ifndef MEDIAN_FILTER_H
#define MEDIAN_FILTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEDIAN_FILTER_MAX_SIZE
#define MEDIAN_FILTER_MAX_SIZE 31
#endif

typedef enum {
    MEDIAN_FILTER_OK         =  0,
    MEDIAN_FILTER_ERR_PARAM  = -1,
} median_filter_err_t;

typedef struct {
    int32_t  window[MEDIAN_FILTER_MAX_SIZE];
    int32_t  sorted[MEDIAN_FILTER_MAX_SIZE];
    uint8_t  size;
    uint8_t  count;
    uint8_t  index;
} median_filter_t;

/** @thread_safe: no @isr_safe: no */
median_filter_err_t median_filter_init(median_filter_t *f, uint8_t size);

/** Feed a sample and return the current median value. @thread_safe: no @isr_safe: yes */
int32_t median_filter_update(median_filter_t *f, int32_t sample);

/** Return the current median without adding a new sample. @thread_safe: no @isr_safe: yes */
int32_t median_filter_get(const median_filter_t *f);

/** Clear all samples and reset to empty state. @thread_safe: no @isr_safe: no */
void median_filter_reset(median_filter_t *f);

#ifdef __cplusplus
}
#endif

#endif /* MEDIAN_FILTER_H */
