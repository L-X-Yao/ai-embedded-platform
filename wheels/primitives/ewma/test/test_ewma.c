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
    /* With constant input, integer EWMA converges to the input value.
     * Allow small rounding tolerance due to integer truncation. */
    int32_t val = ewma_get(&ctx);
    ASSERT_TRUE(val >= 990 && val <= 1000);
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
    /* Should converge close to 1000 (integer truncation causes slight offset) */
    int32_t val = ewma_get(&ctx);
    ASSERT_TRUE(val >= 930 && val <= 1000);
}

/* === Shift values affect tracking speed === */

TEST(test_shift1_fastest_tracking)
{
    ewma_t ctx;
    ewma_init(&ctx, 1);
    ewma_feed(&ctx, 0);       /* prime to 0 */
    int32_t val = ewma_feed(&ctx, 1000);
    /* shift=1: alpha=0.5, so value = 0 - (0>>1) + (1000>>1) = 500 */
    ASSERT_EQ(val, 500);
}

TEST(test_different_shifts_different_speeds)
{
    /* Smaller shift = faster tracking (larger alpha).
     * After one step from 0 to 1000:
     * shift=2: 0 + 1000>>2 = 250
     * shift=4: 0 + 1000>>4 = 62
     * shift=8: 0 + 1000>>8 = 3
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
    /* shift=16: 0 + 1000>>16 = 0 (integer truncation) */
    ASSERT_EQ(val, 0);
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
    /* Should converge close to 0 */
    int32_t val = ewma_get(&ctx);
    ASSERT_TRUE(val > -50 && val <= 0);
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
    int32_t val = ewma_get(&ctx);
    /* Should converge near big (with some integer truncation loss) */
    ASSERT_TRUE(val > big / 2);
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

    /* shift=1: value = value - (value>>1) + (sample>>1) = value/2 + sample/2 */
    int32_t v = ewma_feed(&ctx, 100);  /* prime: 100 */
    ASSERT_EQ(v, 100);

    v = ewma_feed(&ctx, 200);  /* 100 - 50 + 100 = 150 */
    ASSERT_EQ(v, 150);

    v = ewma_feed(&ctx, 200);  /* 150 - 75 + 100 = 175 */
    ASSERT_EQ(v, 175);
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
    RUN(test_step_response_converges);
    RUN(test_shift1_fastest_tracking);
    RUN(test_different_shifts_different_speeds);
    RUN(test_shift16_slowest_tracking);
    RUN(test_negative_values);
    RUN(test_reset_clears_state);
    RUN(test_large_values_no_overflow);
    RUN(test_get_before_feed);
    RUN(test_multi_feed_sequence);
TEST_MAIN_END()
