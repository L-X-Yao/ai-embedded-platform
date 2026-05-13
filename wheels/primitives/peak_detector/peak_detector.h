// SPDX-License-Identifier: MIT
/*
 * peak_detector — track signal peak and valley with configurable hold/decay
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Tracks running maximum (peak) and minimum (valley) of a signal stream.
 * Supports optional decay: after each sample, the held peak decays toward
 * the current value at a configurable rate.
 * Pure C99, no heap, no OS, fully reentrant per-context.
 */

#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PEAK_DETECTOR_OK        =  0,
    PEAK_DETECTOR_ERR_PARAM = -1,
} peak_detector_err_t;

typedef struct {
    float peak;         /* internal: current peak (max) */
    float valley;       /* internal: current valley (min) */
    float decay_rate;   /* internal: 0.0 = infinite hold, 1.0 = no hold */
    bool  primed;       /* internal: first sample seen? */
} peak_detector_t;

/**
 * Initialize instance.
 * @param decay_rate  0.0 = peaks held forever (classic peak hold),
 *                    (0.0, 1.0) = exponential decay toward current value,
 *                    1.0 = peak always equals latest sample (no hold).
 * @thread_safe: no @isr_safe: yes
 */
peak_detector_err_t peak_detector_init(peak_detector_t *ctx, float decay_rate);

/**
 * Feed a new sample. Updates peak and valley.
 * @thread_safe: no @isr_safe: yes
 */
peak_detector_err_t peak_detector_feed(peak_detector_t *ctx, float sample);

/**
 * Get current peak (max).
 * @thread_safe: no @isr_safe: yes
 */
float peak_detector_get_peak(const peak_detector_t *ctx);

/**
 * Get current valley (min).
 * @thread_safe: no @isr_safe: yes
 */
float peak_detector_get_valley(const peak_detector_t *ctx);

/**
 * Get peak-to-valley range.
 * @thread_safe: no @isr_safe: yes
 */
float peak_detector_get_range(const peak_detector_t *ctx);

/**
 * Reset to unprimed state.
 * @thread_safe: no @isr_safe: yes
 */
peak_detector_err_t peak_detector_reset(peak_detector_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PEAK_DETECTOR_H */
