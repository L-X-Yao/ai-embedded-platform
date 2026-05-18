// SPDX-License-Identifier: MIT
/*
 * prop_lookup_table.c — property-based tests for lookup_table
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_lookup_table.c ../lookup_table.c -o prop_lookup_table
 */

#include "test_harness.h"
#include "property_test.h"
#include "../lookup_table.h"
#include <math.h>

/* Property: boundary values are exact — querying at table x returns table y */
PROPERTY(boundary_exact, 10000) {
    /* Build a small monotonic table */
    size_t count = (size_t)ARBI_RANGE(seed, 2, 8);
    float x[8], y[8];

    float xv = (float)ARBI_RANGE(seed, -100, 0);
    for (size_t i = 0; i < count; i++) {
        x[i] = xv;
        y[i] = (float)ARBI_RANGE(seed, -1000, 1000);
        xv += 1.0f + (float)ARBI_RANGE(seed, 1, 10);
    }

    lookup_table_t lut;
    lookup_table_err_t err = lut_init(&lut, x, y, count);
    PROP_ASSERT_EQ(err, LOOKUP_TABLE_OK);

    /* Query at each exact x point */
    size_t idx = ARBI_SIZE(seed, count - 1);
    float result = lut_interpolate(&lut, x[idx]);
    float diff = result - y[idx];
    if (diff < 0) diff = -diff;
    PROP_ASSERT(diff < 0.01f);
}

/* Property: interpolation is monotonic for monotonic y */
PROPERTY(monotonic_interpolation, 5000) {
    /* Build a strictly increasing table */
    size_t count = (size_t)ARBI_RANGE(seed, 3, 8);
    float x[8], y[8];

    float xv = 0.0f, yv = 0.0f;
    for (size_t i = 0; i < count; i++) {
        x[i] = xv;
        y[i] = yv;
        xv += 1.0f + (float)ARBI_RANGE(seed, 1, 5);
        yv += 1.0f + (float)ARBI_RANGE(seed, 1, 10);
    }

    lookup_table_t lut;
    lut_init(&lut, x, y, count);

    /* Sample two points where x1 < x2 — result should be y1 <= y2 */
    float range = x[count - 1] - x[0];
    float x1_off = (float)ARBI_RANGE(seed, 0, 1000) / 1000.0f * range;
    float x2_off = (float)ARBI_RANGE(seed, 0, 1000) / 1000.0f * range;
    float x1 = x[0] + (x1_off < x2_off ? x1_off : x2_off);
    float x2 = x[0] + (x1_off < x2_off ? x2_off : x1_off);

    float y1 = lut_interpolate(&lut, x1);
    float y2 = lut_interpolate(&lut, x2);
    PROP_ASSERT(y2 >= y1 - 0.001f);
}

/* Property: output clamped to [y[0], y[n-1]] for monotonic-y out-of-range inputs */
PROPERTY(clamp_out_of_range, 10000) {
    size_t count = (size_t)ARBI_RANGE(seed, 2, 8);
    float x[8], y[8];

    float xv = 0.0f;
    for (size_t i = 0; i < count; i++) {
        x[i] = xv;
        y[i] = (float)ARBI_RANGE(seed, -500, 500);
        xv += 1.0f + (float)ARBI_RANGE(seed, 1, 10);
    }

    lookup_table_t lut;
    lut_init(&lut, x, y, count);

    /* Below range: should return y[0] */
    float below = x[0] - (float)ARBI_RANGE(seed, 1, 1000);
    float r_below = lut_interpolate(&lut, below);
    float diff = r_below - y[0];
    if (diff < 0) diff = -diff;
    PROP_ASSERT(diff < 0.01f);

    /* Above range: should return y[count-1] */
    float above = x[count - 1] + (float)ARBI_RANGE(seed, 1, 1000);
    float r_above = lut_interpolate(&lut, above);
    diff = r_above - y[count - 1];
    if (diff < 0) diff = -diff;
    PROP_ASSERT(diff < 0.01f);
}

/* Property: interpolation result bounded by neighboring y values */
PROPERTY(result_between_neighbors, 5000) {
    /* 3-point table for simplicity */
    float x[3] = {0.0f, 10.0f, 20.0f};
    float y[3];
    y[0] = (float)ARBI_RANGE(seed, -100, 100);
    y[1] = (float)ARBI_RANGE(seed, -100, 100);
    y[2] = (float)ARBI_RANGE(seed, -100, 100);

    lookup_table_t lut;
    lut_init(&lut, x, y, 3);

    /* Query in first segment [0, 10] */
    float qx = (float)ARBI_RANGE(seed, 0, 10);
    float result = lut_interpolate(&lut, qx);

    float lo = y[0] < y[1] ? y[0] : y[1];
    float hi = y[0] > y[1] ? y[0] : y[1];
    PROP_ASSERT(result >= lo - 0.01f);
    PROP_ASSERT(result <= hi + 0.01f);
}

TEST_MAIN("lookup_table properties") {
    RUN(boundary_exact);
    RUN(monotonic_interpolation);
    RUN(clamp_out_of_range);
    RUN(result_between_neighbors);
}
TEST_MAIN_END()
