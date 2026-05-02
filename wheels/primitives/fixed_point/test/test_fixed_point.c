// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "fixed_point.h"

/* ── Conversion tests ─────────────────────────────────────────── */

TEST(test_from_int_to_int_roundtrip)
{
    ASSERT_EQ(fp_to_int(fp_from_int(0)),    0);
    ASSERT_EQ(fp_to_int(fp_from_int(1)),    1);
    ASSERT_EQ(fp_to_int(fp_from_int(-1)),  -1);
    ASSERT_EQ(fp_to_int(fp_from_int(100)),  100);
    ASSERT_EQ(fp_to_int(fp_from_int(-100)), -100);
    ASSERT_EQ(fp_to_int(fp_from_int(32767)), 32767);
    ASSERT_EQ(fp_to_int(fp_from_int(-32768)), -32768);
}

TEST(test_from_float_to_float_roundtrip)
{
    ASSERT_NEAR(fp_to_float(fp_from_float(1.5f)),   1.5f,  0.001f);
    ASSERT_NEAR(fp_to_float(fp_from_float(-3.25f)), -3.25f, 0.001f);
    ASSERT_NEAR(fp_to_float(fp_from_float(0.0f)),   0.0f,  0.001f);
    ASSERT_NEAR(fp_to_float(fp_from_float(100.75f)), 100.75f, 0.001f);
}

TEST(test_to_int_truncation)
{
    /* Positive: truncate toward zero */
    ASSERT_EQ(fp_to_int(fp_from_float(1.9f)),  1);
    ASSERT_EQ(fp_to_int(fp_from_float(1.1f)),  1);
    /* Negative: truncate toward zero */
    ASSERT_EQ(fp_to_int(fp_from_float(-1.9f)), -1);
    ASSERT_EQ(fp_to_int(fp_from_float(-1.1f)), -1);
}

TEST(test_to_int_rounded)
{
    ASSERT_EQ(fp_to_int_rounded(fp_from_float(1.4f)),  1);
    ASSERT_EQ(fp_to_int_rounded(fp_from_float(1.5f)),  2);
    ASSERT_EQ(fp_to_int_rounded(fp_from_float(1.6f)),  2);
    ASSERT_EQ(fp_to_int_rounded(fp_from_float(-1.5f)), -2);
    ASSERT_EQ(fp_to_int_rounded(fp_from_float(-1.4f)), -1);
}

/* ── Arithmetic tests ─────────────────────────────────────────── */

TEST(test_add)
{
    /* 1.5 + 2.25 = 3.75 */
    fp_t a = fp_from_float(1.5f);
    fp_t b = fp_from_float(2.25f);
    ASSERT_NEAR(fp_to_float(fp_add(a, b)), 3.75f, 0.001f);

    /* 0 + 0 = 0 */
    ASSERT_EQ(fp_add(fp_from_int(0), fp_from_int(0)), fp_from_int(0));

    /* negative + negative */
    ASSERT_NEAR(fp_to_float(fp_add(fp_from_float(-1.5f), fp_from_float(-2.5f))),
                -4.0f, 0.001f);
}

TEST(test_sub)
{
    /* 5.0 - 3.25 = 1.75 */
    fp_t a = fp_from_float(5.0f);
    fp_t b = fp_from_float(3.25f);
    ASSERT_NEAR(fp_to_float(fp_sub(a, b)), 1.75f, 0.001f);

    /* a - a = 0 */
    fp_t c = fp_from_float(42.42f);
    ASSERT_EQ(fp_sub(c, c), 0);
}

TEST(test_mul)
{
    /* 2.5 * 3.0 = 7.5 */
    fp_t a = fp_from_float(2.5f);
    fp_t b = fp_from_float(3.0f);
    ASSERT_NEAR(fp_to_float(fp_mul(a, b)), 7.5f, 0.001f);

    /* 0 * anything = 0 */
    ASSERT_EQ(fp_mul(fp_from_int(0), fp_from_int(42)), 0);

    /* 1 * x = x */
    fp_t x = fp_from_float(123.456f);
    ASSERT_NEAR(fp_to_float(fp_mul(FP_ONE, x)), 123.456f, 0.01f);
}

TEST(test_mul_large_values_overflow_safe)
{
    /* 200.0 * 100.0 = 20000.0 — would overflow int32 without int64 intermediate */
    fp_t a = fp_from_float(200.0f);
    fp_t b = fp_from_float(100.0f);
    ASSERT_NEAR(fp_to_float(fp_mul(a, b)), 20000.0f, 1.0f);

    /* -500.0 * 50.0 = -25000.0 */
    fp_t c = fp_from_float(-500.0f);
    fp_t d = fp_from_float(50.0f);
    ASSERT_NEAR(fp_to_float(fp_mul(c, d)), -25000.0f, 1.0f);
}

TEST(test_div)
{
    /* 7.5 / 2.5 = 3.0 */
    fp_t a = fp_from_float(7.5f);
    fp_t b = fp_from_float(2.5f);
    ASSERT_NEAR(fp_to_float(fp_div(a, b)), 3.0f, 0.001f);

    /* 1.0 / 3.0 ~ 0.333 */
    ASSERT_NEAR(fp_to_float(fp_div(FP_ONE, fp_from_int(3))),
                0.333f, 0.001f);
}

TEST(test_div_by_zero)
{
    /* Positive / 0 → FP_MAX */
    ASSERT_EQ(fp_div(fp_from_int(5), 0), FP_MAX);
    /* Negative / 0 → FP_MIN */
    ASSERT_EQ(fp_div(fp_from_int(-5), 0), FP_MIN);
    /* Zero / 0 → FP_MAX (a >= 0 path) */
    ASSERT_EQ(fp_div(fp_from_int(0), 0), FP_MAX);
}

/* ── Comparison tests ─────────────────────────────────────────── */

TEST(test_comparison)
{
    fp_t a = fp_from_float(1.5f);
    fp_t b = fp_from_float(2.5f);
    fp_t c = fp_from_float(1.5f);

    ASSERT_TRUE(fp_gt(b, a));
    ASSERT_FALSE(fp_gt(a, b));
    ASSERT_FALSE(fp_gt(a, c));

    ASSERT_TRUE(fp_lt(a, b));
    ASSERT_FALSE(fp_lt(b, a));
    ASSERT_FALSE(fp_lt(a, c));

    ASSERT_TRUE(fp_eq(a, c));
    ASSERT_FALSE(fp_eq(a, b));
}

/* ── Math function tests ──────────────────────────────────────── */

TEST(test_abs)
{
    ASSERT_EQ(fp_abs(fp_from_int(5)),  fp_from_int(5));
    ASSERT_EQ(fp_abs(fp_from_int(-5)), fp_from_int(5));
    ASSERT_EQ(fp_abs(fp_from_int(0)),  fp_from_int(0));
    /* INT32_MIN edge case: returns FP_MAX */
    ASSERT_EQ(fp_abs(FP_MIN), FP_MAX);
}

TEST(test_min_max_clamp)
{
    fp_t a = fp_from_int(3);
    fp_t b = fp_from_int(7);

    ASSERT_EQ(fp_min(a, b), a);
    ASSERT_EQ(fp_min(b, a), a);
    ASSERT_EQ(fp_max(a, b), b);
    ASSERT_EQ(fp_max(b, a), b);

    /* Clamp within range → unchanged */
    ASSERT_EQ(fp_clamp(fp_from_int(5), a, b), fp_from_int(5));
    /* Clamp below → lo */
    ASSERT_EQ(fp_clamp(fp_from_int(1), a, b), a);
    /* Clamp above → hi */
    ASSERT_EQ(fp_clamp(fp_from_int(10), a, b), b);
}

TEST(test_lerp)
{
    fp_t a = fp_from_int(0);
    fp_t b = fp_from_int(10);

    /* t=0 → a */
    ASSERT_EQ(fp_lerp(a, b, 0), a);
    /* t=FP_ONE → b */
    ASSERT_EQ(fp_lerp(a, b, FP_ONE), b);
    /* t=FP_HALF → midpoint */
    ASSERT_NEAR(fp_to_float(fp_lerp(a, b, FP_HALF)), 5.0f, 0.001f);

    /* Non-zero base: lerp(2, 8, 0.25) = 3.5 */
    fp_t c = fp_from_int(2);
    fp_t d = fp_from_int(8);
    fp_t t = fp_from_float(0.25f);
    ASSERT_NEAR(fp_to_float(fp_lerp(c, d, t)), 3.5f, 0.01f);
}

/* ── String formatting tests ──────────────────────────────────── */

TEST(test_to_string_basic)
{
    char buf[32];
    int len;

    /* "1.50" */
    len = fp_to_string(fp_from_float(1.5f), buf, sizeof(buf), 2);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strcmp(buf, "1.50") == 0);

    /* "-3.25" */
    len = fp_to_string(fp_from_float(-3.25f), buf, sizeof(buf), 2);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strcmp(buf, "-3.25") == 0);

    /* "0.00" */
    len = fp_to_string(fp_from_int(0), buf, sizeof(buf), 2);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strcmp(buf, "0.00") == 0);

    /* Integer with no decimal places */
    len = fp_to_string(fp_from_int(42), buf, sizeof(buf), 0);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strcmp(buf, "42") == 0);
}

TEST(test_to_string_buffer_too_small)
{
    char buf[4];
    /* "-3.25" needs 6 chars (5 + NUL) — buf is only 4 */
    int len = fp_to_string(fp_from_float(-3.25f), buf, sizeof(buf), 2);
    ASSERT_EQ(len, -1);
}

TEST(test_to_string_null_buf)
{
    int len = fp_to_string(fp_from_int(1), (char *)0, 0, 2);
    ASSERT_EQ(len, -1);
}

TEST(test_fp_constants)
{
    /* FP_ONE should convert back to 1.0 */
    ASSERT_NEAR(fp_to_float(FP_ONE), 1.0f, 0.0001f);
    /* FP_HALF should convert back to 0.5 */
    ASSERT_NEAR(fp_to_float(FP_HALF), 0.5f, 0.0001f);
    /* FP_PI should be close to pi */
    ASSERT_NEAR(fp_to_float(FP_PI), 3.14159f, 0.001f);
}

TEST_MAIN("fixed_point")
    RUN(test_from_int_to_int_roundtrip);
    RUN(test_from_float_to_float_roundtrip);
    RUN(test_to_int_truncation);
    RUN(test_to_int_rounded);
    RUN(test_add);
    RUN(test_sub);
    RUN(test_mul);
    RUN(test_mul_large_values_overflow_safe);
    RUN(test_div);
    RUN(test_div_by_zero);
    RUN(test_comparison);
    RUN(test_abs);
    RUN(test_min_max_clamp);
    RUN(test_lerp);
    RUN(test_to_string_basic);
    RUN(test_to_string_buffer_too_small);
    RUN(test_to_string_null_buf);
    RUN(test_fp_constants);
TEST_MAIN_END()
