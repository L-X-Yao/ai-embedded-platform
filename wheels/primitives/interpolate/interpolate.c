// SPDX-License-Identifier: MIT
/*
 * interpolate -- linear, table, and bilinear interpolation
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "interpolate.h"
#include "contract.h"

/* ── NaN check helper ───────────────────────────────────────────── */

static int is_nan(float v)
{
    /* IEEE 754: NaN != NaN */
    return v != v;  /* NOLINT: intentional NaN check */
}

/* ── Linear interpolation ───────────────────────────────────────── */

interpolate_err_t interpolate_linear(float x0, float y0, float x1, float y1,
                                     float x, float *out)
{
    REQUIRE(out != NULL);
    if (!out) return INTERPOLATE_ERR_PARAM;

    /* NaN guard on inputs */
    if (is_nan(x0) || is_nan(y0) || is_nan(x1) || is_nan(y1) || is_nan(x)) {
        return INTERPOLATE_ERR_PARAM;
    }

    /* Division by zero guard */
    float dx = x1 - x0;
    if (dx == 0.0f) return INTERPOLATE_ERR_PARAM;

    float t = (x - x0) / dx;
    float result = y0 + t * (y1 - y0);

    /* NaN guard on output */
    ENSURE(result == result);
    if (is_nan(result)) return INTERPOLATE_ERR_PARAM;

    *out = result;
    return INTERPOLATE_OK;
}

/* ── Table interpolation (binary search + linear) ───────────────── */

interpolate_err_t interpolate_table(const interpolate_table_t *table,
                                    float x, float *out)
{
    REQUIRE(table != NULL);
    REQUIRE(out != NULL);
    if (!table || !out) return INTERPOLATE_ERR_PARAM;
    if (!table->x || !table->y) return INTERPOLATE_ERR_PARAM;
    if (table->len < 2U) return INTERPOLATE_ERR_PARAM;

    /* NaN guard on query */
    if (is_nan(x)) return INTERPOLATE_ERR_PARAM;

    /* Verify sorted ascending */
    for (uint16_t i = 1U; i < table->len; i++) {
        if (table->x[i] <= table->x[i - 1U]) {
            return INTERPOLATE_ERR_PARAM;
        }
    }

    /* Clamp at boundaries */
    if (x <= table->x[0]) {
        *out = table->y[0];
        return INTERPOLATE_OK;
    }
    if (x >= table->x[table->len - 1U]) {
        *out = table->y[table->len - 1U];
        return INTERPOLATE_OK;
    }

    /* Binary search for the bracket [lo, hi] where x[lo] <= x < x[hi] */
    uint16_t lo = 0U;
    uint16_t hi = (uint16_t)(table->len - 1U);
    while ((uint16_t)(hi - lo) > 1U) {
        uint16_t mid = (uint16_t)(lo + (uint16_t)(hi - lo) / 2U);
        if (table->x[mid] <= x) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    /* Interpolate within bracket */
    float dx = table->x[hi] - table->x[lo];
    /* dx > 0 guaranteed by sorted check above */
    float t = (x - table->x[lo]) / dx;
    float result = table->y[lo] + t * (table->y[hi] - table->y[lo]);

    ENSURE(result == result);
    if (is_nan(result)) return INTERPOLATE_ERR_PARAM;

    *out = result;
    return INTERPOLATE_OK;
}

/* ── Bilinear interpolation ─────────────────────────────────────── */

interpolate_err_t interpolate_bilinear(float x0, float y0, float x1, float y1,
                                       float q00, float q10, float q01, float q11,
                                       float x, float y, float *out)
{
    REQUIRE(out != NULL);
    if (!out) return INTERPOLATE_ERR_PARAM;

    /* NaN guard on all inputs */
    if (is_nan(x0) || is_nan(y0) || is_nan(x1) || is_nan(y1) ||
        is_nan(q00) || is_nan(q10) || is_nan(q01) || is_nan(q11) ||
        is_nan(x) || is_nan(y)) {
        return INTERPOLATE_ERR_PARAM;
    }

    /* Division by zero guard */
    float dx = x1 - x0;
    float dy = y1 - y0;
    if (dx == 0.0f || dy == 0.0f) return INTERPOLATE_ERR_PARAM;

    float tx = (x - x0) / dx;
    float ty = (y - y0) / dy;

    /* Bilinear formula: interpolate along x at y0 and y1, then along y */
    float r0 = q00 + tx * (q10 - q00);   /* interpolate at y0 */
    float r1 = q01 + tx * (q11 - q01);   /* interpolate at y1 */
    float result = r0 + ty * (r1 - r0);  /* interpolate along y */

    ENSURE(result == result);
    if (is_nan(result)) return INTERPOLATE_ERR_PARAM;

    *out = result;
    return INTERPOLATE_OK;
}
