// SPDX-License-Identifier: MIT
/*
 * stress_moving_average — long-running accumulation / overflow tests
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -I.. -I../../../../tooling/test \
 *       stress_moving_average.c ../moving_average.c -lm -o /tmp/stress_moving_average
 *
 * Scenarios:
 *   1. SMA: 1,000,000 constant samples — verify average drift < 0.1 %
 *   2. SMA: 1,000,000 alternating +MAX/-MAX — verify no Inf/NaN
 *   3. EMA: 1,000,000 constant samples — verify no NaN/Inf
 *   4. EMA: 1,000,000 alternating large/small — verify stability
 */

#include "moving_average.h"
#include "test_harness.h"
#include <math.h>
#include <float.h>

#define ITERATIONS  1000000
#define SMA_WINDOW  64

/* ---- helpers -------------------------------------------------------- */

static int is_finite_f(float v)
{
    return (v == v) && (v - v == 0.0f);          /* not NaN, not Inf */
}

/* ---- SMA tests ------------------------------------------------------ */

TEST(sma_constant_no_drift)
{
    float buf[SMA_WINDOW];
    moving_average_sma_t sma;
    moving_average_sma_init(&sma, buf, SMA_WINDOW);

    const float constant = 42.0f;

    for (int i = 0; i < ITERATIONS; i++)
    {
        moving_average_sma_feed(&sma, constant);
    }

    float avg = moving_average_sma_get(&sma);
    float error = fabsf(avg - constant) / constant;

    ASSERT_TRUE(error < 0.001f);                  /* < 0.1 % drift */
}

TEST(sma_ramp_no_drift)
{
    /*
     * Feed values 0, 1, 2, ..., N-1.  Once the window is full the
     * true average of the last SMA_WINDOW values is:
     *   expected = i - (SMA_WINDOW - 1) / 2.0
     * Check at the end.
     */
    float buf[SMA_WINDOW];
    moving_average_sma_t sma;
    moving_average_sma_init(&sma, buf, SMA_WINDOW);

    float result = 0.0f;
    for (int i = 0; i < ITERATIONS; i++)
    {
        result = moving_average_sma_feed(&sma, (float)i);
    }

    float expected = (float)(ITERATIONS - 1) - (float)(SMA_WINDOW - 1) / 2.0f;
    float error    = fabsf(result - expected) / expected;

    ASSERT_TRUE(is_finite_f(result));
    ASSERT_TRUE(error < 0.001f);                  /* < 0.1 % */
}

TEST(sma_alternating_large_values)
{
    /* +/- large values — must not produce NaN or Inf */
    float buf[SMA_WINDOW];
    moving_average_sma_t sma;
    moving_average_sma_init(&sma, buf, SMA_WINDOW);

    const float big = 1.0e30f;
    int ok = 1;

    for (int i = 0; i < ITERATIONS; i++)
    {
        float sample = (i & 1) ? big : -big;
        float avg = moving_average_sma_feed(&sma, sample);
        if (!is_finite_f(avg))
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
}

/* ---- EMA tests ------------------------------------------------------ */

TEST(ema_constant_no_nan)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.1f);

    const float constant = 100.0f;

    for (int i = 0; i < ITERATIONS; i++)
    {
        moving_average_ema_feed(&ema, constant);
    }

    float val = moving_average_ema_get(&ema);
    ASSERT_TRUE(is_finite_f(val));
    ASSERT_NEAR(val, constant, 0.01f);
}

TEST(ema_alternating_large_no_nan)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.05f);

    int ok = 1;
    for (int i = 0; i < ITERATIONS; i++)
    {
        float sample = (i & 1) ? 1.0e30f : -1.0e30f;
        float val = moving_average_ema_feed(&ema, sample);
        if (!is_finite_f(val))
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
}

TEST(ema_small_alpha_stability)
{
    /* Very small alpha (slow filter) — 1M samples of constant should converge */
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.001f);

    const float target = 55.5f;

    for (int i = 0; i < ITERATIONS; i++)
    {
        moving_average_ema_feed(&ema, target);
    }

    float val = moving_average_ema_get(&ema);
    ASSERT_TRUE(is_finite_f(val));
    ASSERT_NEAR(val, target, 0.01f);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress_moving_average")
    RUN(sma_constant_no_drift);
    RUN(sma_ramp_no_drift);
    RUN(sma_alternating_large_values);
    RUN(ema_constant_no_nan);
    RUN(ema_alternating_large_no_nan);
    RUN(ema_small_alpha_stability);
TEST_MAIN_END()
