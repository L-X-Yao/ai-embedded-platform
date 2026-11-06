// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <math.h>
#include <string.h>
#include "lookup_table.h"

#define TOL 1e-5f

/* ---- standard 5-point table for most tests ---- */
static const float xs[] = { 0.0f, 1.0f, 2.0f, 4.0f, 10.0f };
static const float ys[] = { 0.0f, 10.0f, 20.0f, 50.0f, 100.0f };
#define TABLE_LEN  (sizeof(xs) / sizeof(xs[0]))

TEST(test_exact_match)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    ASSERT_NEAR(lut_interpolate(&t, 0.0f),   0.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 1.0f),  10.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 2.0f),  20.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 4.0f),  50.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 10.0f), 100.0f, TOL);
}

TEST(test_interpolation_midpoint)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    /* midpoint of [0,1] segment: x=0.5 -> y=5.0 */
    ASSERT_NEAR(lut_interpolate(&t, 0.5f), 5.0f, TOL);
    /* midpoint of [1,2] segment: x=1.5 -> y=15.0 */
    ASSERT_NEAR(lut_interpolate(&t, 1.5f), 15.0f, TOL);
}

TEST(test_interpolation_non_uniform_spacing)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    /* segment [2,4]: slope = (50-20)/(4-2) = 15/unit */
    /* x=3.0 -> y = 20 + 15*1 = 35 */
    ASSERT_NEAR(lut_interpolate(&t, 3.0f), 35.0f, TOL);
    /* segment [4,10]: slope = (100-50)/(10-4) = 50/6 */
    /* x=7.0 -> y = 50 + (50/6)*3 = 75 */
    ASSERT_NEAR(lut_interpolate(&t, 7.0f), 75.0f, TOL);
}

TEST(test_clamp_below_range)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    ASSERT_NEAR(lut_interpolate(&t, -5.0f),   0.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, -100.0f), 0.0f, TOL);
}

TEST(test_clamp_above_range)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    ASSERT_NEAR(lut_interpolate(&t, 10.0f),  100.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 999.0f), 100.0f, TOL);
}

TEST(test_two_point_table)
{
    const float x2[] = { 1.0f, 3.0f };
    const float y2[] = { 10.0f, 30.0f };
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, x2, y2, 2), LOOKUP_TABLE_OK);
    /* exact endpoints */
    ASSERT_NEAR(lut_interpolate(&t, 1.0f), 10.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 3.0f), 30.0f, TOL);
    /* midpoint */
    ASSERT_NEAR(lut_interpolate(&t, 2.0f), 20.0f, TOL);
    /* clamp */
    ASSERT_NEAR(lut_interpolate(&t, 0.0f), 10.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 5.0f), 30.0f, TOL);
}

TEST(test_single_point)
{
    const float x1[] = { 5.0f };
    const float y1[] = { 42.0f };
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, x1, y1, 1), LOOKUP_TABLE_OK);
    /* any input returns the single y value (clamp both sides) */
    ASSERT_NEAR(lut_interpolate(&t, 5.0f),   42.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 0.0f),   42.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 100.0f), 42.0f, TOL);
}

TEST(test_null_params)
{
    lookup_table_t t;
    /* NULL table pointer */
    ASSERT_EQ(lut_init(NULL, xs, ys, TABLE_LEN), LOOKUP_TABLE_ERR_PARAM);
    /* NULL x array */
    ASSERT_EQ(lut_init(&t, NULL, ys, TABLE_LEN), LOOKUP_TABLE_ERR_PARAM);
    /* NULL y array */
    ASSERT_EQ(lut_init(&t, xs, NULL, TABLE_LEN), LOOKUP_TABLE_ERR_PARAM);
    /* zero count */
    ASSERT_EQ(lut_init(&t, xs, ys, 0), LOOKUP_TABLE_ERR_PARAM);
    /* interpolate on NULL table returns 0 */
    ASSERT_NEAR(lut_interpolate(NULL, 1.0f), 0.0f, TOL);
}

TEST(test_monotonic_check)
{
    lookup_table_t t;
    /* non-ascending: duplicate x */
    const float xdup[] = { 1.0f, 2.0f, 2.0f, 3.0f };
    const float ydup[] = { 0.0f, 1.0f, 2.0f, 3.0f };
    ASSERT_EQ(lut_init(&t, xdup, ydup, 4), LOOKUP_TABLE_ERR_NOT_SORTED);

    /* non-ascending: decreasing x */
    const float xdec[] = { 1.0f, 3.0f, 2.0f, 4.0f };
    const float ydec[] = { 0.0f, 1.0f, 2.0f, 3.0f };
    ASSERT_EQ(lut_init(&t, xdec, ydec, 4), LOOKUP_TABLE_ERR_NOT_SORTED);
}

/* Every comparison with NaN is false, so it must not pass the order check */
TEST(test_nan_in_x_rejected)
{
    lookup_table_t t;
    const float y[] = { 10.0f, 20.0f, 30.0f };

    /* the wrong pair 2 > 1 is hidden behind the NaN */
    const float hidden[] = { 2.0f, NAN, 1.0f };
    ASSERT_EQ(lut_init(&t, hidden, y, 3), LOOKUP_TABLE_ERR_NOT_SORTED);

    const float middle[] = { 0.0f, NAN, 2.0f };
    ASSERT_EQ(lut_init(&t, middle, y, 3), LOOKUP_TABLE_ERR_NOT_SORTED);

    const float first[] = { NAN, 1.0f, 2.0f };
    ASSERT_EQ(lut_init(&t, first, y, 3), LOOKUP_TABLE_ERR_NOT_SORTED);

    const float last[] = { 0.0f, 1.0f, NAN };
    ASSERT_EQ(lut_init(&t, last, y, 3), LOOKUP_TABLE_ERR_NOT_SORTED);

    /* no pair to compare, the loop never runs */
    const float single[] = { NAN };
    ASSERT_EQ(lut_init(&t, single, y, 1), LOOKUP_TABLE_ERR_NOT_SORTED);
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    lookup_table_t t;
    memset(&t, 0, sizeof(t));
    /* Should return 0.0f gracefully, not crash */
    ASSERT_NEAR(lut_interpolate(&t, 1.0f), 0.0f, TOL);
}

TEST(test_negative_x_values)
{
    const float xn[] = { -10.0f, -5.0f, 0.0f, 5.0f };
    const float yn[] = { 100.0f, 50.0f, 0.0f, -50.0f };
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xn, yn, 4), LOOKUP_TABLE_OK);
    /* midpoint of [-10,-5]: y = 100 + (50-100)/(-5-(-10)) * (-7.5-(-10)) = 75 */
    ASSERT_NEAR(lut_interpolate(&t, -7.5f), 75.0f, TOL);
    /* clamp below */
    ASSERT_NEAR(lut_interpolate(&t, -20.0f), 100.0f, TOL);
    /* clamp above */
    ASSERT_NEAR(lut_interpolate(&t, 10.0f), -50.0f, TOL);
}

TEST(test_large_table_binary_search)
{
    /* 11-point linear table: y = 2*x, x from 0..100 step 10 */
    const float xl[] = { 0.0f, 10.0f, 20.0f, 30.0f, 40.0f,
                         50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f };
    const float yl[] = { 0.0f, 20.0f, 40.0f, 60.0f, 80.0f,
                         100.0f, 120.0f, 140.0f, 160.0f, 180.0f, 200.0f };
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xl, yl, 11), LOOKUP_TABLE_OK);
    /* test several interior points across different segments */
    ASSERT_NEAR(lut_interpolate(&t, 15.0f),  30.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 55.0f), 110.0f, TOL);
    ASSERT_NEAR(lut_interpolate(&t, 85.0f), 170.0f, TOL);
}

TEST(test_quarter_point_interpolation)
{
    lookup_table_t t;
    ASSERT_EQ(lut_init(&t, xs, ys, TABLE_LEN), LOOKUP_TABLE_OK);
    /* x=0.25 in segment [0,1]: y = 0 + 10*0.25 = 2.5 */
    ASSERT_NEAR(lut_interpolate(&t, 0.25f), 2.5f, TOL);
    /* x=0.75 in segment [0,1]: y = 0 + 10*0.75 = 7.5 */
    ASSERT_NEAR(lut_interpolate(&t, 0.75f), 7.5f, TOL);
}

TEST_MAIN("lookup_table")
    RUN(test_exact_match);
    RUN(test_interpolation_midpoint);
    RUN(test_interpolation_non_uniform_spacing);
    RUN(test_clamp_below_range);
    RUN(test_clamp_above_range);
    RUN(test_two_point_table);
    RUN(test_single_point);
    RUN(test_null_params);
    RUN(test_monotonic_check);
    RUN(test_nan_in_x_rejected);
    RUN(test_zero_init_safety);
    RUN(test_negative_x_values);
    RUN(test_large_table_binary_search);
    RUN(test_quarter_point_interpolation);
TEST_MAIN_END()
