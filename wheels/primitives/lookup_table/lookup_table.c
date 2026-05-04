// SPDX-License-Identifier: MIT
/*
 * lookup_table — interpolated lookup table for sensor linearization
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "lookup_table.h"
#include "contract.h"

#include <stddef.h>

lookup_table_err_t lut_init(lookup_table_t *table,
                            const float *x, const float *y,
                            size_t count)
{
    if (!table || !x || !y || count == 0) {
        return LOOKUP_TABLE_ERR_PARAM;
    }

    /* Verify strict ascending order */
    for (size_t i = 1; i < count; i++) {
        if (x[i] <= x[i - 1]) {
            return LOOKUP_TABLE_ERR_NOT_SORTED;
        }
    }

    table->x     = x;
    table->y     = y;
    table->count = count;
    ENSURE(table->count > 0);
    return LOOKUP_TABLE_OK;
}

float lut_interpolate(const lookup_table_t *table, float x)
{
    if (!table || !table->x || !table->y || table->count == 0) {
        return 0.0f;
    }

    /* reject NaN — would fall through clamp and corrupt interpolation */
    if (x != x) {
        return 0.0f;
    }

    size_t n = table->count;

    /* Clamp below range */
    if (x <= table->x[0]) {
        return table->y[0];
    }

    /* Clamp above range */
    if (x >= table->x[n - 1]) {
        return table->y[n - 1];
    }

    /* Binary search for the interval [x[lo], x[lo+1]] containing x */
    size_t lo = 0;
    size_t hi = n - 1;
    while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;
        if (x < table->x[mid]) {
            hi = mid;
        } else {
            lo = mid;
        }
    }

    /* Linear interpolation */
    float x0 = table->x[lo];
    float x1 = table->x[hi];
    float y0 = table->y[lo];
    float y1 = table->y[hi];
    float t  = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}
