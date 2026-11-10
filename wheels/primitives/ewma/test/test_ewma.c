// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ewma.h"
#include <limits.h>

/* === Parameter validation === */

TEST(test_init_null)
{
    ASSERT_EQ(ewma_init(NULL, 4), EWMA_ERR_PARAM);
}

TEST(test_init_shift_zero)
{
    ewma_t ctx;
    ASSERT_EQ(ewma_init(&ctx, 0), EWMA_ERR_PARAM);
}

TEST(test_init_shift_too_large)
{
    ewma_t ctx;
    ASSERT_EQ(ewma_init(&ctx, 17), EWMA_ERR_PARAM);
    ASSERT_EQ(ewma_init(&ctx, 255), EWMA_ERR_PARAM);
}

TEST(test_init_valid_shift_range)
{
    ewma_t ctx;
    ASSERT_EQ(ewma_init(&ctx, 1), EWMA_OK);
    ASSERT_EQ(ewma_init(&ctx, 16), EWMA_OK);
}

TEST(test_feed_null)
{
    ASSERT_EQ(ewma_feed(NULL, 100), 0);
}

TEST(test_get_null)
{
    ASSERT_EQ(ewma_get(NULL), 0);
}

TEST(test_reset_null)
{
    ASSERT_EQ(ewma_reset(NULL), EWMA_ERR_PARAM);
}

/* === Priming === */

TEST(test_first_sample_primes)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    int32_t val = ewma_feed(&ctx, 1000);
    ASSERT_EQ(val, 1000);
    ASSERT_EQ(ewma_get(&ctx), 1000);
}

TEST(test_first_sample_negative)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    int32_t val = ewma_feed(&ctx, -500);
    ASSERT_EQ(val, -500);
    ASSERT_EQ(ewma_get(&ctx), -500);
}

/* === Constant input convergence === */

TEST(test_constant_input_converges)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    for (int i = 0; i < 200; i++)
    {
        ewma_feed(&ctx, 1000);
    }
    /* The first sample primes to the input and the input never changes. */
    ASSERT_EQ(ewma_get(&ctx), 1000);
}

/* Prime with `first`, then feed `target` for long enough to settle.
 * The error shrinks by 1 - 1/2^shift per feed down to one count, and the
 * last count takes at most 2^shift feeds: ln(2^32) + 1 < 32 time constants
 * covers the whole int32 span. */
static int32_t settle(uint8_t shift, int32_t first, int32_t target)
{
    ewma_t ctx;
    ewma_init(&ctx, shift);
    ewma_feed(&ctx, first);

    int32_t out = first;
    for (int32_t i = 0; i < (32 << shift); i++)
    {
        out = ewma_feed(&ctx, target);
    }
    return out;
}

TEST(test_reaches_a_constant_input_rising)
{
    ASSERT_EQ(settle(8, 0, 255), 255);        /* below 2^shift */
    ASSERT_EQ(settle(8, 0, 4095), 4095);
    ASSERT_EQ(settle(12, 1000, 4000), 4000);
    ASSERT_EQ(settle(16, 1000, 60000), 60000);
    ASSERT_EQ(settle(4, 0, 1000), 1000);
    ASSERT_EQ(settle(16, 0, 1), 1);           /* a single count */
}

TEST(test_reaches_a_constant_input_falling)
{
    ASSERT_EQ(settle(8, 4095, 0), 0);
    ASSERT_EQ(settle(8, 255, 0), 0);
    ASSERT_EQ(settle(16, 60000, 1000), 1000);
    ASSERT_EQ(settle(4, 1000, -1000), -1000);
    ASSERT_EQ(settle(8, 0, -255), -255);
    ASSERT_EQ(settle(16, 0, -1), -1);
}

TEST(test_reaches_the_ends_of_the_range)
{
    ASSERT_EQ(settle(16, INT32_MIN, INT32_MAX), INT32_MAX);
    ASSERT_EQ(settle(16, INT32_MAX, INT32_MIN), INT32_MIN);
    ASSERT_EQ(settle(1, INT32_MIN, INT32_MAX), INT32_MAX);
    ASSERT_EQ(settle(1, INT32_MAX, INT32_MIN), INT32_MIN);
}

TEST(test_stays_within_a_count_of_the_real_ewma)
{
    /* The definition: y += (x - y) / 2^shift, in real arithmetic. */
    static const int32_t in[] = { 1000, 1100, 900, 900, 5000, -300, -300,
                                  7, 7, 7, 4095, 0, 1, 2, 3, -4096, 12 };
    ewma_t ctx;
    ewma_init(&ctx, 3);

    double y = (double)in[0];
    ASSERT_EQ(ewma_feed(&ctx, in[0]), in[0]);

    for (unsigned i = 1; i < sizeof(in) / sizeof(in[0]); i++)
    {
        y += ((double)in[i] - y) / 8.0;
        double err = (double)ewma_feed(&ctx, in[i]) - y;
        ASSERT_TRUE(err > -1.0 && err < 1.0);
    }
}

TEST(test_noise_keeps_its_mean)
{
    /* Bounded noise 2072..2088 around 2080 at shift 6, coming up from 0. */
    ewma_t ctx;
    ewma_init(&ctx, 6);
    ewma_feed(&ctx, 0);

    uint32_t lcg = 12345u;
    int64_t in_sum = 0;
    int64_t out_sum = 0;
    const int32_t skip = 32 << 6;   /* let the step from 0 finish */
    const int32_t n = 200000;

    for (int32_t i = 0; i < skip + n; i++)
    {
        lcg = lcg * 1664525u + 1013904223u;
        int32_t sample = 2072 + (int32_t)((lcg >> 16) % 17u);
        int32_t out = ewma_feed(&ctx, sample);
        if (i >= skip)
        {
            in_sum  += sample;
            out_sum += out;
        }
    }

    /* The two means agree to under one count. */
    int64_t diff = out_sum - in_sum;
    ASSERT_TRUE(diff > -(int64_t)n && diff < (int64_t)n);
}

TEST(test_get_returns_the_last_output)
{
    ewma_t ctx;
    ewma_init(&ctx, 5);
    ewma_feed(&ctx, -77);

    for (int32_t i = 0; i < 500; i++)
    {
        int32_t out = ewma_feed(&ctx, i * 13 - 900);
        ASSERT_EQ(ewma_get(&ctx), out);
    }
}

/* === Step response === */

TEST(test_step_response_converges)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);

    /* Prime with zeros */
    for (int i = 0; i < 100; i++)
    {
        ewma_feed(&ctx, 0);
    }
    ASSERT_EQ(ewma_get(&ctx), 0);

    /* Step to 1000 */
    for (int i = 0; i < 200; i++)
    {
        ewma_feed(&ctx, 1000);
    }
    /* 1000 * (15/16)^200 is far below one count: the step is complete. */
    ASSERT_EQ(ewma_get(&ctx), 1000);
}

/* === Shift values affect tracking speed === */

TEST(test_shift1_fastest_tracking)
{
    ewma_t ctx;
    ewma_init(&ctx, 1);
    ewma_feed(&ctx, 0);       /* prime to 0 */
    int32_t val = ewma_feed(&ctx, 1000);
    /* shift=1: alpha=0.5, so 0 + (1000 - 0) / 2 = 500 */
    ASSERT_EQ(val, 500);
}

TEST(test_different_shifts_different_speeds)
{
    /* Smaller shift = faster tracking (larger alpha).
     * After one step from 0 to 1000:
     * shift=2: 1000/4   = 250
     * shift=4: 1000/16  = 62.5
     * shift=8: 1000/256 = 3.9
     */
    ewma_t fast, medium, slow;
    ewma_init(&fast, 2);
    ewma_init(&medium, 4);
    ewma_init(&slow, 8);

    ewma_feed(&fast, 0);
    ewma_feed(&medium, 0);
    ewma_feed(&slow, 0);

    int32_t vf = ewma_feed(&fast, 1000);
    int32_t vm = ewma_feed(&medium, 1000);
    int32_t vs = ewma_feed(&slow, 1000);

    ASSERT_TRUE(vf > vm);
    ASSERT_TRUE(vm > vs);
}

TEST(test_shift16_slowest_tracking)
{
    ewma_t ctx;
    ewma_init(&ctx, 16);
    ewma_feed(&ctx, 0);       /* prime to 0 */
    int32_t val = ewma_feed(&ctx, 1000);
    /* shift=16: one step is 1000/65536 = 0.015 of a count, so the output
     * does not show it yet... */
    ASSERT_EQ(val, 0);

    /* ...but the step is kept: 45426 feeds is ln(2) * 2^16, half way. */
    for (int32_t i = 1; i < 45426; i++)
    {
        val = ewma_feed(&ctx, 1000);
    }
    ASSERT_TRUE(val >= 499 && val <= 501);
}

/* === Negative values === */

TEST(test_negative_values)
{
    ewma_t ctx;
    ewma_init(&ctx, 2);
    ewma_feed(&ctx, -1000);
    ASSERT_EQ(ewma_get(&ctx), -1000);

    /* Feed towards zero */
    for (int i = 0; i < 100; i++)
    {
        ewma_feed(&ctx, 0);
    }
    /* 1000 * (3/4)^100 is far below one count. */
    ASSERT_EQ(ewma_get(&ctx), 0);
}

/* === Reset === */

TEST(test_reset_clears_state)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    ewma_feed(&ctx, 5000);
    ewma_feed(&ctx, 6000);

    ASSERT_EQ(ewma_reset(&ctx), EWMA_OK);
    ASSERT_EQ(ewma_get(&ctx), 0);

    /* After reset, next feed should prime again */
    int32_t val = ewma_feed(&ctx, 42);
    ASSERT_EQ(val, 42);
}

/* === Large values (overflow safety) === */

TEST(test_large_values_no_overflow)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);

    /* Use values well within int32 range to avoid overflow.
     * INT32_MAX/4 = ~536870911 */
    int32_t big = INT32_MAX / 4;
    for (int i = 0; i < 100; i++)
    {
        ewma_feed(&ctx, big);
    }
    ASSERT_EQ(ewma_get(&ctx), big);
}

/* === Get before any feed === */

TEST(test_get_before_feed)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    ASSERT_EQ(ewma_get(&ctx), 0);
}

/* === Multi-feed arithmetic verification === */

TEST(test_multi_feed_sequence)
{
    ewma_t ctx;
    ewma_init(&ctx, 1);

    /* shift=1: y = y/2 + sample/2 */
    int32_t v = ewma_feed(&ctx, 100);  /* prime: 100 */
    ASSERT_EQ(v, 100);

    v = ewma_feed(&ctx, 200);  /* 50 + 100 = 150 */
    ASSERT_EQ(v, 150);

    v = ewma_feed(&ctx, 200);  /* 75 + 100 = 175 */
    ASSERT_EQ(v, 175);
}

/* === INT32_MIN / INT32_MAX extremes === */

TEST(test_int32_max_constant)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    int32_t v = ewma_feed(&ctx, INT32_MAX);
    ASSERT_EQ(v, INT32_MAX);  /* prime to exact value */

    /* Feeding the same value must not overflow or move the output. */
    for (int i = 0; i < 50; i++)
    {
        v = ewma_feed(&ctx, INT32_MAX);
    }
    ASSERT_EQ(v, INT32_MAX);
}

TEST(test_int32_min_constant)
{
    ewma_t ctx;
    ewma_init(&ctx, 4);
    int32_t v = ewma_feed(&ctx, INT32_MIN);
    ASSERT_EQ(v, INT32_MIN);  /* prime to exact value */

    /* Feeding the same value must not overflow or move the output. */
    for (int i = 0; i < 50; i++)
    {
        v = ewma_feed(&ctx, INT32_MIN);
    }
    ASSERT_EQ(v, INT32_MIN);
}

TEST(test_negative_to_positive_step)
{
    ewma_t ctx;
    ewma_init(&ctx, 2);

    /* Prime and settle at -1000 */
    for (int i = 0; i < 100; i++)
    {
        ewma_feed(&ctx, -1000);
    }
    int32_t v = ewma_get(&ctx);
    ASSERT_TRUE(v < 0);

    /* Step to +1000 */
    for (int i = 0; i < 200; i++)
    {
        ewma_feed(&ctx, 1000);
    }
    v = ewma_get(&ctx);
    /* 2000 * (3/4)^200 is far below one count. */
    ASSERT_EQ(v, 1000);
}

TEST(test_alternating_samples)
{
    ewma_t ctx;
    ewma_init(&ctx, 2);
    ewma_feed(&ctx, 0);  /* prime to 0 */

    /* Feed alternating +100 / -100 — average should stay near 0 */
    for (int i = 0; i < 200; i++)
    {
        ewma_feed(&ctx, (i % 2 == 0) ? 100 : -100);
    }
    int32_t v = ewma_get(&ctx);
    ASSERT_TRUE(v > -50 && v < 50);
}

TEST_MAIN("ewma")
    RUN(test_init_null);
    RUN(test_init_shift_zero);
    RUN(test_init_shift_too_large);
    RUN(test_init_valid_shift_range);
    RUN(test_feed_null);
    RUN(test_get_null);
    RUN(test_reset_null);
    RUN(test_first_sample_primes);
    RUN(test_first_sample_negative);
    RUN(test_constant_input_converges);
    RUN(test_reaches_a_constant_input_rising);
    RUN(test_reaches_a_constant_input_falling);
    RUN(test_reaches_the_ends_of_the_range);
    RUN(test_stays_within_a_count_of_the_real_ewma);
    RUN(test_noise_keeps_its_mean);
    RUN(test_get_returns_the_last_output);
    RUN(test_step_response_converges);
    RUN(test_shift1_fastest_tracking);
    RUN(test_different_shifts_different_speeds);
    RUN(test_shift16_slowest_tracking);
    RUN(test_negative_values);
    RUN(test_reset_clears_state);
    RUN(test_large_values_no_overflow);
    RUN(test_get_before_feed);
    RUN(test_multi_feed_sequence);
    RUN(test_int32_max_constant);
    RUN(test_int32_min_constant);
    RUN(test_negative_to_positive_step);
    RUN(test_alternating_samples);
TEST_MAIN_END()
