// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <stdio.h>
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

/* ── Mutation-hardening tests ─────────────────────────────────── */

/*
 * fp_from_int: verify exact bit layout — catches shift-by-wrong-amount mutations.
 * Q16.16 means val=1 must produce 0x00010000.
 */
TEST(test_from_int_exact_bits)
{
    ASSERT_EQ(fp_from_int(1),    (fp_t)0x00010000);
    ASSERT_EQ(fp_from_int(2),    (fp_t)0x00020000);
    ASSERT_EQ(fp_from_int(-1),   (fp_t)0xFFFF0000);
    ASSERT_EQ(fp_from_int(0),    (fp_t)0x00000000);
    ASSERT_EQ(fp_from_int(256),  (fp_t)0x01000000);
}

/*
 * fp_to_int: verify direction of truncation with fractional parts just below
 * the integer boundary — catches <= vs < mutations in the shift logic.
 */
TEST(test_to_int_exact_truncation)
{
    /* Positive fractions truncate DOWN (toward zero) */
    ASSERT_EQ(fp_to_int((fp_t)(FP_ONE + FP_ONE - 1)), 1);   /* 1.9999.. → 1 */
    ASSERT_EQ(fp_to_int((fp_t)(FP_ONE + 1)),           1);   /* 1.000015.. → 1 */
    /* Negative fractions truncate UP (toward zero) */
    ASSERT_EQ(fp_to_int((fp_t)(-(FP_ONE + FP_ONE - 1))), -1); /* -1.9999.. → -1 */
    ASSERT_EQ(fp_to_int((fp_t)(-(FP_ONE + 1))),           -1); /* -1.0000.. → -1 */
    /* Exact integer is not affected */
    ASSERT_EQ(fp_to_int((fp_t)(3 * FP_ONE)), 3);
    ASSERT_EQ(fp_to_int((fp_t)(-3 * FP_ONE)), -3);
}

/*
 * fp_to_int_rounded: verify rounding boundary at exactly 0.5 —
 * catches off-by-one in the FP_HALF addition.
 */
TEST(test_to_int_rounded_exact_half)
{
    /* Exactly 0.5 rounds to 1, exactly -0.5 rounds to -1 */
    ASSERT_EQ(fp_to_int_rounded(FP_HALF), 1);
    ASSERT_EQ(fp_to_int_rounded(-FP_HALF), -1);
    /* One below 0.5 rounds to 0 */
    ASSERT_EQ(fp_to_int_rounded(FP_HALF - 1), 0);
    ASSERT_EQ(fp_to_int_rounded(-(FP_HALF - 1)), 0);
    /* 2.5 rounds to 3 */
    ASSERT_EQ(fp_to_int_rounded(fp_from_int(2) + FP_HALF), 3);
    ASSERT_EQ(fp_to_int_rounded(-(fp_from_int(2) + FP_HALF)), -3);
}

/*
 * fp_add overflow clamping: verify exact saturation values —
 * catches > vs >= and + vs - mutations in the overflow guards.
 */
TEST(test_add_saturation_exact)
{
    /* FP_MAX + 1 must saturate to FP_MAX, not wrap */
    ASSERT_EQ(fp_add(FP_MAX, 1), FP_MAX);
    /* FP_MIN - 1 must saturate to FP_MIN */
    ASSERT_EQ(fp_add(FP_MIN, -1), FP_MIN);
    /* Values that do NOT overflow must be exact */
    ASSERT_EQ(fp_add(fp_from_int(1), fp_from_int(2)), fp_from_int(3));
    ASSERT_EQ(fp_add(fp_from_int(-1), fp_from_int(-2)), fp_from_int(-3));
}

/*
 * fp_sub saturation: symmetric partner to add saturation.
 */
TEST(test_sub_saturation_exact)
{
    ASSERT_EQ(fp_sub(FP_MIN, 1), FP_MIN);
    ASSERT_EQ(fp_sub(FP_MAX, -1), FP_MAX);
    ASSERT_EQ(fp_sub(fp_from_int(5), fp_from_int(3)), fp_from_int(2));
    ASSERT_EQ(fp_sub(fp_from_int(3), fp_from_int(5)), fp_from_int(-2));
}

/*
 * fp_mul exact values: integer × integer must be precise —
 * catches shift-by-wrong-amount mutations in the result >> FRAC_BITS step.
 */
TEST(test_mul_exact_integer_results)
{
    ASSERT_EQ(fp_mul(fp_from_int(3), fp_from_int(4)), fp_from_int(12));
    ASSERT_EQ(fp_mul(fp_from_int(-3), fp_from_int(4)), fp_from_int(-12));
    ASSERT_EQ(fp_mul(fp_from_int(-3), fp_from_int(-4)), fp_from_int(12));
    ASSERT_EQ(fp_mul(fp_from_int(1), fp_from_int(1)), fp_from_int(1));
    /* 0.5 * 0.5 = 0.25 */
    ASSERT_EQ(fp_mul(FP_HALF, FP_HALF), FP_HALF / 2);
}

/*
 * fp_mul saturation: large × large must clamp.
 */
TEST(test_mul_saturation)
{
    ASSERT_EQ(fp_mul(FP_MAX, fp_from_int(2)), FP_MAX);
    ASSERT_EQ(fp_mul(FP_MIN, fp_from_int(2)), FP_MIN);
}

/*
 * fp_div exact: verify numerator scaling — catches *= vs /=, >> vs << mutations.
 */
TEST(test_div_exact_integer_results)
{
    ASSERT_EQ(fp_div(fp_from_int(12), fp_from_int(4)), fp_from_int(3));
    ASSERT_EQ(fp_div(fp_from_int(-12), fp_from_int(4)), fp_from_int(-3));
    ASSERT_EQ(fp_div(fp_from_int(12), fp_from_int(-4)), fp_from_int(-3));
    ASSERT_EQ(fp_div(fp_from_int(0), fp_from_int(7)), fp_from_int(0));
    /* 1 / 2 = 0.5 exactly */
    ASSERT_EQ(fp_div(fp_from_int(1), fp_from_int(2)), FP_HALF);
    /* 1 / 4 = 0.25 exactly */
    ASSERT_EQ(fp_div(fp_from_int(1), fp_from_int(4)), FP_HALF / 2);
}

/*
 * fp_abs boundary: INT32_MIN must return FP_MAX, not hang or return wrong value —
 * catches val == FP_MIN guard condition mutations.
 */
TEST(test_abs_boundaries)
{
    ASSERT_EQ(fp_abs(FP_MIN), FP_MAX);
    ASSERT_EQ(fp_abs(FP_MAX), FP_MAX);
    ASSERT_EQ(fp_abs(fp_from_int(0)), fp_from_int(0));
    ASSERT_EQ(fp_abs(fp_from_int(-7)), fp_from_int(7));
    ASSERT_EQ(fp_abs(fp_from_int(7)), fp_from_int(7));
    /* value just above FP_MIN must negate correctly */
    ASSERT_EQ(fp_abs(FP_MIN + 1), -(FP_MIN + 1));
}

/*
 * fp_min/fp_max: both arguments equal must return that value —
 * catches < vs <= mutations.
 */
TEST(test_min_max_equal)
{
    fp_t v = fp_from_float(5.5f);
    ASSERT_EQ(fp_min(v, v), v);
    ASSERT_EQ(fp_max(v, v), v);
    /* adjacent values */
    fp_t a = fp_from_int(3);
    fp_t b = fp_from_int(3) + 1; /* 3 + 1 LSB */
    ASSERT_EQ(fp_min(a, b), a);
    ASSERT_EQ(fp_min(b, a), a);
    ASSERT_EQ(fp_max(a, b), b);
    ASSERT_EQ(fp_max(b, a), b);
}

/*
 * fp_clamp boundaries: value exactly at lo or hi must not be clamped —
 * catches < vs <= mutations in the boundary checks.
 */
TEST(test_clamp_at_boundaries)
{
    fp_t lo = fp_from_int(2);
    fp_t hi = fp_from_int(8);

    /* Exactly at lo: must return lo unchanged */
    ASSERT_EQ(fp_clamp(lo, lo, hi), lo);
    /* Exactly at hi: must return hi unchanged */
    ASSERT_EQ(fp_clamp(hi, lo, hi), hi);
    /* One below lo: must clamp to lo */
    ASSERT_EQ(fp_clamp(lo - 1, lo, hi), lo);
    /* One above hi: must clamp to hi */
    ASSERT_EQ(fp_clamp(hi + 1, lo, hi), hi);
}

/*
 * fp_lerp: exact values at t=0 and t=FP_ONE, plus midpoint at t=FP_HALF.
 * Catches sign errors in diff computation and shift-by-wrong-amount.
 */
TEST(test_lerp_exact_values)
{
    fp_t a = fp_from_int(10);
    fp_t b = fp_from_int(20);

    ASSERT_EQ(fp_lerp(a, b, 0), a);
    ASSERT_EQ(fp_lerp(a, b, FP_ONE), b);
    /* t=FP_HALF → exactly 15.0 */
    ASSERT_EQ(fp_lerp(a, b, FP_HALF), fp_from_int(15));

    /* Reversed: b < a */
    fp_t c = fp_from_int(20);
    fp_t d = fp_from_int(10);
    ASSERT_EQ(fp_lerp(c, d, 0), c);
    ASSERT_EQ(fp_lerp(c, d, FP_ONE), d);
    ASSERT_EQ(fp_lerp(c, d, FP_HALF), fp_from_int(15));
}

/*
 * fp_comparison: verify strict inequality (not >=, not <=) —
 * catches > vs >= and < vs <= mutations.
 */
TEST(test_comparison_strict)
{
    fp_t a = fp_from_int(5);
    fp_t b = fp_from_int(5);

    /* Equal values: neither gt nor lt */
    ASSERT_FALSE(fp_gt(a, b));
    ASSERT_FALSE(fp_lt(a, b));
    ASSERT_TRUE(fp_eq(a, b));

    /* Adjacent by 1 LSB */
    fp_t c = a + 1;
    ASSERT_TRUE(fp_gt(c, a));
    ASSERT_FALSE(fp_gt(a, c));
    ASSERT_TRUE(fp_lt(a, c));
    ASSERT_FALSE(fp_lt(c, a));
    ASSERT_FALSE(fp_eq(a, c));
}

/*
 * fp_to_string: verify exact return value (char count) and string content —
 * catches off-by-one in pos tracking and decimal place logic.
 */
TEST(test_to_string_exact_return_value)
{
    char buf[32];
    int len;

    /* "0" → 1 char */
    len = fp_to_string(fp_from_int(0), buf, sizeof(buf), 0);
    ASSERT_EQ(len, 1);

    /* "1" → 1 char */
    len = fp_to_string(fp_from_int(1), buf, sizeof(buf), 0);
    ASSERT_EQ(len, 1);

    /* "42" → 2 chars */
    len = fp_to_string(fp_from_int(42), buf, sizeof(buf), 0);
    ASSERT_EQ(len, 2);

    /* "-1" → 2 chars */
    len = fp_to_string(fp_from_int(-1), buf, sizeof(buf), 0);
    ASSERT_EQ(len, 2);

    /* "1.5" with 1 decimal → 3 chars */
    len = fp_to_string(fp_from_float(1.5f), buf, sizeof(buf), 1);
    ASSERT_EQ(len, 3);
    ASSERT_TRUE(buf[0] == '1' && buf[1] == '.' && buf[2] == '5');

    /* "0.50" with 2 decimals → 4 chars */
    len = fp_to_string(fp_from_float(0.5f), buf, sizeof(buf), 2);
    ASSERT_EQ(len, 4);
    ASSERT_TRUE(buf[0] == '0' && buf[1] == '.' && buf[2] == '5' && buf[3] == '0');
}

/*
 * fp_to_string decimal places clamped at 5: verify 6+ decimal_places are
 * silently capped (not a buffer overrun), and 5 is accepted.
 */
TEST(test_to_string_decimal_places_clamped)
{
    char buf[32];
    int len;

    /* 5 decimal places is the max */
    len = fp_to_string(fp_from_float(1.0f), buf, sizeof(buf), 5);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0], '1');
    ASSERT_EQ(buf[1], '.');
    ASSERT_EQ(len, 7); /* "1.00000" = 7 chars */

    /* 6 decimal places is capped to 5 → same result */
    int len2 = fp_to_string(fp_from_float(1.0f), buf, sizeof(buf), 6);
    ASSERT_EQ(len2, 7);
}

/*
 * fp_to_string: NUL termination is always written (regression guard).
 */
TEST(test_to_string_nul_termination)
{
    char buf[16];
    buf[0] = (char)0xFF; /* poison */
    int len = fp_to_string(fp_from_int(7), buf, sizeof(buf), 2);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[len], '\0');
}

/*
 * fp_from_float NaN: must return 0 without UB.
 */
TEST(test_from_float_nan)
{
    float nan_val = 0.0f / 0.0f; /* produce NaN */
    fp_t result = fp_from_float(nan_val);
    ASSERT_EQ(result, 0);
}

/*
 * fp_to_float: verify the division by FP_ONE is exact for known values.
 * Catches * vs / mutations.
 */
TEST(test_to_float_exact)
{
    ASSERT_NEAR(fp_to_float(FP_ONE), 1.0f, 0.000001f);
    ASSERT_NEAR(fp_to_float(FP_HALF), 0.5f, 0.000001f);
    ASSERT_NEAR(fp_to_float(fp_from_int(0)), 0.0f, 0.000001f);
    ASSERT_NEAR(fp_to_float(fp_from_int(-1)), -1.0f, 0.000001f);
    ASSERT_NEAR(fp_to_float(fp_from_int(100)), 100.0f, 0.001f);
}

/*
 * fp_to_string, 5 places: frac_raw * 100000 passes 2^32 at frac_raw 42950.
 * The expected digits are exact binary fractions worked out by hand.
 */
TEST(test_to_string_five_places_large_fraction)
{
    char buf[32];

    ASSERT_EQ(fp_to_string((fp_t)0x0000C000, buf, sizeof(buf), 5), 7);
    ASSERT_STR_EQ(buf, "0.75000");
    ASSERT_EQ(fp_to_string((fp_t)0x0003C000, buf, sizeof(buf), 5), 7);
    ASSERT_STR_EQ(buf, "3.75000");
    ASSERT_EQ(fp_to_string(-(fp_t)0x000CE000, buf, sizeof(buf), 5), 9);
    ASSERT_STR_EQ(buf, "-12.87500");
    /* last fraction that fitted, then the first that did not */
    ASSERT_EQ(fp_to_string((fp_t)42949, buf, sizeof(buf), 5), 7);
    ASSERT_STR_EQ(buf, "0.65535");
    ASSERT_EQ(fp_to_string((fp_t)42950, buf, sizeof(buf), 5), 7);
    ASSERT_STR_EQ(buf, "0.65536");
}

/* Every fraction at 5 places against round-to-nearest in double, which is
 * exact here: frac * 100000 needs 33 bits and / 65536 only moves the point. */
TEST(test_to_string_five_places_exhaustive)
{
    char buf[32];
    char want[32];

    for (uint32_t frac = 0; frac <= 0xFFFFu; frac++) {
        double scaled = (double)frac * 100000.0 / 65536.0;
        (void)snprintf(want, sizeof(want), "0.%05u", (unsigned)(scaled + 0.5));
        ASSERT_EQ(fp_to_string((fp_t)frac, buf, sizeof(buf), 5), 7);
        ASSERT_STR_EQ(buf, want);
    }
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
    RUN(test_from_int_exact_bits);
    RUN(test_to_int_exact_truncation);
    RUN(test_to_int_rounded_exact_half);
    RUN(test_add_saturation_exact);
    RUN(test_sub_saturation_exact);
    RUN(test_mul_exact_integer_results);
    RUN(test_mul_saturation);
    RUN(test_div_exact_integer_results);
    RUN(test_abs_boundaries);
    RUN(test_min_max_equal);
    RUN(test_clamp_at_boundaries);
    RUN(test_lerp_exact_values);
    RUN(test_comparison_strict);
    RUN(test_to_string_exact_return_value);
    RUN(test_to_string_decimal_places_clamped);
    RUN(test_to_string_nul_termination);
    RUN(test_from_float_nan);
    RUN(test_to_float_exact);
    RUN(test_to_string_five_places_large_fraction);
    RUN(test_to_string_five_places_exhaustive);
TEST_MAIN_END()
