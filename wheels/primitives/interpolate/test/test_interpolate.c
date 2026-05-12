// SPDX-License-Identifier: MIT
#include "test_harness.h"
/**
 * @file    test_interpolate.c
 * @brief   C unit tests for interpolate wheel.
 *
 * Returns 0 on all-pass, non-zero on first failure (pytest checks exit code).
 * Each TEST() prints PASS/FAIL line for human + machine readability.
 */

#include "interpolate.h"
#include <math.h>

/* ── Linear basic ───────────────────────────────────────────────── */

TEST(test_linear_basic)
{
    float out;
    ASSERT_EQ(interpolate_linear(0.0f, 0.0f, 10.0f, 100.0f, 5.0f, &out),
              INTERPOLATE_OK);
    ASSERT_NEAR(out, 50.0f, 0.001f);
}

TEST(test_linear_at_endpoints)
{
    float out;
    /* At x0 */
    ASSERT_EQ(interpolate_linear(1.0f, 10.0f, 3.0f, 30.0f, 1.0f, &out),
              INTERPOLATE_OK);
    ASSERT_NEAR(out, 10.0f, 0.001f);

    /* At x1 */
    ASSERT_EQ(interpolate_linear(1.0f, 10.0f, 3.0f, 30.0f, 3.0f, &out),
              INTERPOLATE_OK);
    ASSERT_NEAR(out, 30.0f, 0.001f);
}

TEST(test_linear_extrapolation)
{
    /* Linear extrapolation beyond endpoints should still work */
    float out;
    ASSERT_EQ(interpolate_linear(0.0f, 0.0f, 10.0f, 10.0f, 20.0f, &out),
              INTERPOLATE_OK);
    ASSERT_NEAR(out, 20.0f, 0.001f);
}

/* ── Table interpolation ────────────────────────────────────────── */

TEST(test_table_2_point)
{
    float xs[] = { 0.0f, 10.0f };
    float ys[] = { 0.0f, 100.0f };
    interpolate_table_t table = { xs, ys, 2 };

    float out;
    ASSERT_EQ(interpolate_table(&table, 5.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 50.0f, 0.001f);
}

TEST(test_table_multi_point)
{
    /* NTC thermistor-like calibration curve (5 points) */
    float xs[] = { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f };
    float ys[] = { 27.28f, 10.0f, 3.603f, 1.299f, 0.5f };
    interpolate_table_t table = { xs, ys, 5 };

    float out;
    /* At exact point */
    ASSERT_EQ(interpolate_table(&table, 25.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 10.0f, 0.001f);

    /* Between first two points */
    ASSERT_EQ(interpolate_table(&table, 12.5f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 18.64f, 0.01f);
}

TEST(test_table_clamp_boundaries)
{
    float xs[] = { 10.0f, 20.0f, 30.0f };
    float ys[] = { 100.0f, 200.0f, 300.0f };
    interpolate_table_t table = { xs, ys, 3 };

    float out;
    /* Below minimum: clamp to first y */
    ASSERT_EQ(interpolate_table(&table, 0.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 100.0f, 0.001f);

    /* Above maximum: clamp to last y */
    ASSERT_EQ(interpolate_table(&table, 50.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 300.0f, 0.001f);
}

/* ── Bilinear interpolation ─────────────────────────────────────── */

TEST(test_bilinear_corners)
{
    /* At each corner, should return that corner's value */
    float out;
    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   0.0f, 0.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 1.0f, 0.001f);  /* q00 */

    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   1.0f, 0.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 2.0f, 0.001f);  /* q10 */

    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   0.0f, 1.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 3.0f, 0.001f);  /* q01 */

    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   1.0f, 1.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 4.0f, 0.001f);  /* q11 */
}

TEST(test_bilinear_center)
{
    float out;
    /* Center of unit square with corners 0,0,0,4 -> average = 1.0 */
    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   0.0f, 0.0f, 0.0f, 4.0f,
                                   0.5f, 0.5f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 1.0f, 0.001f);

    /* Uniform value: all corners = 5.0 -> center = 5.0 */
    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   5.0f, 5.0f, 5.0f, 5.0f,
                                   0.5f, 0.5f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 5.0f, 0.001f);
}

/* ── Division by zero rejection ─────────────────────────────────── */

TEST(test_division_by_zero)
{
    float out;
    /* Linear: x0 == x1 */
    ASSERT_EQ(interpolate_linear(5.0f, 10.0f, 5.0f, 20.0f, 5.0f, &out),
              INTERPOLATE_ERR_PARAM);

    /* Bilinear: x0 == x1 */
    ASSERT_EQ(interpolate_bilinear(3.0f, 0.0f, 3.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   3.0f, 0.5f, &out), INTERPOLATE_ERR_PARAM);

    /* Bilinear: y0 == y1 */
    ASSERT_EQ(interpolate_bilinear(0.0f, 3.0f, 1.0f, 3.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   0.5f, 3.0f, &out), INTERPOLATE_ERR_PARAM);
}

/* ── NaN input rejection ────────────────────────────────────────── */

TEST(test_nan_rejection)
{
    float out;
    float nan_val = 0.0f / 0.0f;

    /* Linear: NaN x */
    ASSERT_EQ(interpolate_linear(0.0f, 0.0f, 1.0f, 1.0f, nan_val, &out),
              INTERPOLATE_ERR_PARAM);

    /* Table: NaN x */
    float xs[] = { 0.0f, 1.0f };
    float ys[] = { 0.0f, 1.0f };
    interpolate_table_t table = { xs, ys, 2 };
    ASSERT_EQ(interpolate_table(&table, nan_val, &out), INTERPOLATE_ERR_PARAM);

    /* Bilinear: NaN x */
    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   nan_val, 0.5f, &out), INTERPOLATE_ERR_PARAM);
}

/* ── NULL pointer rejection ─────────────────────────────────────── */

TEST(test_null_rejection)
{
    float out;
    float xs[] = { 0.0f, 1.0f };
    float ys[] = { 0.0f, 1.0f };
    interpolate_table_t table = { xs, ys, 2 };

    /* Linear: NULL out */
    ASSERT_EQ(interpolate_linear(0.0f, 0.0f, 1.0f, 1.0f, 0.5f, NULL),
              INTERPOLATE_ERR_PARAM);

    /* Table: NULL out */
    ASSERT_EQ(interpolate_table(&table, 0.5f, NULL), INTERPOLATE_ERR_PARAM);

    /* Table: NULL table */
    ASSERT_EQ(interpolate_table(NULL, 0.5f, &out), INTERPOLATE_ERR_PARAM);

    /* Table: NULL x array */
    interpolate_table_t bad_table = { NULL, ys, 2 };
    ASSERT_EQ(interpolate_table(&bad_table, 0.5f, &out), INTERPOLATE_ERR_PARAM);

    /* Table: NULL y array */
    interpolate_table_t bad_table2 = { xs, NULL, 2 };
    ASSERT_EQ(interpolate_table(&bad_table2, 0.5f, &out), INTERPOLATE_ERR_PARAM);

    /* Bilinear: NULL out */
    ASSERT_EQ(interpolate_bilinear(0.0f, 0.0f, 1.0f, 1.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f,
                                   0.5f, 0.5f, NULL), INTERPOLATE_ERR_PARAM);
}

/* ── Table: too few points / unsorted ───────────────────────────── */

TEST(test_table_invalid_params)
{
    float out;
    float xs[] = { 1.0f };
    float ys[] = { 1.0f };
    interpolate_table_t table1 = { xs, ys, 1 };
    ASSERT_EQ(interpolate_table(&table1, 0.5f, &out), INTERPOLATE_ERR_PARAM);

    /* Unsorted x values */
    float xs_bad[] = { 3.0f, 1.0f, 2.0f };
    float ys_bad[] = { 30.0f, 10.0f, 20.0f };
    interpolate_table_t table_bad = { xs_bad, ys_bad, 3 };
    ASSERT_EQ(interpolate_table(&table_bad, 2.0f, &out), INTERPOLATE_ERR_PARAM);
}

/* ── Table: at exact data points ────────────────────────────────── */

TEST(test_table_exact_points)
{
    float xs[] = { 0.0f, 10.0f, 20.0f, 30.0f };
    float ys[] = { 0.0f, 5.0f, 15.0f, 35.0f };
    interpolate_table_t table = { xs, ys, 4 };

    float out;
    /* At exact points should return exact y */
    ASSERT_EQ(interpolate_table(&table, 0.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 0.0f, 0.001f);

    ASSERT_EQ(interpolate_table(&table, 10.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 5.0f, 0.001f);

    ASSERT_EQ(interpolate_table(&table, 20.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 15.0f, 0.001f);

    ASSERT_EQ(interpolate_table(&table, 30.0f, &out), INTERPOLATE_OK);
    ASSERT_NEAR(out, 35.0f, 0.001f);
}

TEST_MAIN("interpolate")
    RUN(test_linear_basic);
    RUN(test_linear_at_endpoints);
    RUN(test_linear_extrapolation);
    RUN(test_table_2_point);
    RUN(test_table_multi_point);
    RUN(test_table_clamp_boundaries);
    RUN(test_bilinear_corners);
    RUN(test_bilinear_center);
    RUN(test_division_by_zero);
    RUN(test_nan_rejection);
    RUN(test_null_rejection);
    RUN(test_table_invalid_params);
    RUN(test_table_exact_points);
TEST_MAIN_END()
