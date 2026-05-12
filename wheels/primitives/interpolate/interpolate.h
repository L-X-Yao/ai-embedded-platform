// SPDX-License-Identifier: MIT
/*
 * interpolate -- linear, table, and bilinear interpolation
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Designed for sensor calibration curves and temperature compensation.
 * All functions are pure / reentrant, no global state.
 */

#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    INTERPOLATE_OK        =  0,
    INTERPOLATE_ERR_PARAM = -1,
} interpolate_err_t;

/* ---------- lookup table ---------- */

/**
 * Lookup table for piecewise-linear interpolation.
 *
 * x values must be sorted in strictly ascending order.
 * len must be >= 2.
 */
typedef struct {
    const float *x;    /* x values, sorted ascending */
    const float *y;    /* corresponding y values */
    uint16_t     len;  /* number of points, >= 2 */
} interpolate_table_t;

/* ---------- API ---------- */

/**
 * Single-segment linear interpolation.
 *
 * Computes y at position x given two endpoints (x0,y0) and (x1,y1).
 *
 * @param x0   First x coordinate.
 * @param y0   First y coordinate.
 * @param x1   Second x coordinate. Must differ from x0.
 * @param y1   Second y coordinate.
 * @param x    Query x position.
 * @param out  Output interpolated y. Must not be NULL.
 * @return     INTERPOLATE_OK on success.
 *             INTERPOLATE_ERR_PARAM if x0==x1, out is NULL, or NaN input.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
interpolate_err_t interpolate_linear(float x0, float y0, float x1, float y1,
                                     float x, float *out);

/**
 * Piecewise-linear table interpolation with boundary clamping.
 *
 * Uses binary search to find the bracket, then linearly interpolates.
 * Values outside the table range are clamped to the nearest endpoint.
 *
 * @param table  Lookup table. Must not be NULL, len >= 2, x sorted ascending.
 * @param x      Query x position.
 * @param out    Output interpolated y. Must not be NULL.
 * @return       INTERPOLATE_OK on success.
 *               INTERPOLATE_ERR_PARAM on NULL, len < 2, or unsorted table.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
interpolate_err_t interpolate_table(const interpolate_table_t *table,
                                    float x, float *out);

/**
 * Bilinear interpolation on a 2D grid.
 *
 * Given four corner values q00, q10, q01, q11 at positions
 * (x0,y0), (x1,y0), (x0,y1), (x1,y1), interpolates at (x,y).
 *
 * Common use: temperature-compensated sensor calibration.
 *
 * @param x0   Left x bound.
 * @param y0   Bottom y bound.
 * @param x1   Right x bound. Must differ from x0.
 * @param y1   Top y bound. Must differ from y0.
 * @param q00  Value at (x0, y0).
 * @param q10  Value at (x1, y0).
 * @param q01  Value at (x0, y1).
 * @param q11  Value at (x1, y1).
 * @param x    Query x position.
 * @param y    Query y position.
 * @param out  Output interpolated value. Must not be NULL.
 * @return     INTERPOLATE_OK on success.
 *             INTERPOLATE_ERR_PARAM if x0==x1, y0==y1, out is NULL, or NaN.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
interpolate_err_t interpolate_bilinear(float x0, float y0, float x1, float y1,
                                       float q00, float q10, float q01, float q11,
                                       float x, float y, float *out);

#ifdef __cplusplus
}
#endif

#endif /* INTERPOLATE_H */
