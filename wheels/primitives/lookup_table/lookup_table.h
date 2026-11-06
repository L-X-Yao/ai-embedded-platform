// SPDX-License-Identifier: MIT
/*
 * lookup_table — interpolated lookup table for sensor linearization
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Caller provides sorted x/y arrays; lut_interpolate performs linear
 * interpolation between nearest points.  Out-of-range inputs are clamped
 * to the first/last y value (no extrapolation beyond endpoints).
 *
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOOKUP_TABLE_OK            =  0,
    LOOKUP_TABLE_ERR_PARAM     = -1,
    LOOKUP_TABLE_ERR_NOT_SORTED = -2,
} lookup_table_err_t;

typedef struct {
    const float *x;      /* borrowed pointer to caller's x array */
    const float *y;      /* borrowed pointer to caller's y array */
    size_t       count;  /* number of points (must be >= 1)      */
} lookup_table_t;

/**
 * Initialise a lookup table context.
 *
 * @param table   context to initialise (must not be NULL)
 * @param x       array of x (input) values, must be strictly ascending
 * @param y       array of y (output) values
 * @param count   number of points (>= 1)
 * @return LOOKUP_TABLE_OK on success,
 *         LOOKUP_TABLE_ERR_PARAM if any pointer is NULL or count == 0,
 *         LOOKUP_TABLE_ERR_NOT_SORTED if x is not strictly ascending
 *         or holds a NaN
 *
 * @thread_safe: no @isr_safe: no
 */
lookup_table_err_t lut_init(lookup_table_t *table,
                            const float *x, const float *y,
                            size_t count);

/**
 * Linearly interpolate y for a given x.
 *
 * If x is below the first point, returns y[0] (clamp).
 * If x is above the last point, returns y[count-1] (clamp).
 * If x matches an exact table point, returns that y directly.
 * Otherwise, linearly interpolates between the two nearest points.
 *
 * @param table   initialised context (must not be NULL)
 * @param x       input value
 * @return interpolated y, or 0.0f if table is NULL or uninitialised
 *
 * @thread_safe: no @isr_safe: yes
 */
float lut_interpolate(const lookup_table_t *table, float x);

#ifdef __cplusplus
}
#endif

#endif /* LOOKUP_TABLE_H */
