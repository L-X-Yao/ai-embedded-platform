// SPDX-License-Identifier: MIT
/*
 * moving_rms — running root-mean-square over a sliding window
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Maintains sum-of-squares in a circular buffer for O(1) RMS updates.
 * Caller provides the window buffer. Useful for vibration and AC signals.
 * Pure C99, no heap, no OS.
 */

#ifndef MOVING_RMS_H
#define MOVING_RMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOVING_RMS_OK        =  0,
    MOVING_RMS_ERR_PARAM = -1,
} moving_rms_err_t;

typedef struct {
    float   *buf;       /* internal: caller-provided sample² buffer */
    size_t   cap;       /* internal: window size */
    size_t   head;      /* internal: write index */
    size_t   count;     /* internal: samples stored */
    float    sum_sq;    /* internal: running sum of squares */
} moving_rms_t;

/** Init with caller-provided buffer. @thread_safe: no @isr_safe: yes */
moving_rms_err_t moving_rms_init(moving_rms_t *ctx, float *buf, size_t window);

/** Feed a sample and return current RMS.  A sample that is NaN or Inf, or whose
 *  square would overflow the running sum (any |sample| above about 1.8e19), is
 *  ignored.  @thread_safe: no @isr_safe: yes */
float moving_rms_feed(moving_rms_t *ctx, float sample);

/** Get current RMS without feeding. @thread_safe: no @isr_safe: yes */
float moving_rms_get(const moving_rms_t *ctx);

/** Reset. @thread_safe: no @isr_safe: yes */
moving_rms_err_t moving_rms_reset(moving_rms_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MOVING_RMS_H */
