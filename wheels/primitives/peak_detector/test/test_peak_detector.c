// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "peak_detector.h"
#include <math.h>

/* ---------- init ---------- */

TEST(test_init_ok)
{
    peak_detector_t pd;
    ASSERT_EQ(peak_detector_init(&pd, 0.0f), PEAK_DETECTOR_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(peak_detector_init(NULL, 0.0f), PEAK_DETECTOR_ERR_PARAM);
}

TEST(test_init_decay_negative)
{
    peak_detector_t pd;
    ASSERT_EQ(peak_detector_init(&pd, -0.1f), PEAK_DETECTOR_ERR_PARAM);
}

TEST(test_init_decay_over_one)
{
    peak_detector_t pd;
    ASSERT_EQ(peak_detector_init(&pd, 1.1f), PEAK_DETECTOR_ERR_PARAM);
}

TEST(test_init_decay_nan)
{
    peak_detector_t pd;
    ASSERT_EQ(peak_detector_init(&pd, (float)NAN), PEAK_DETECTOR_ERR_PARAM);
}

/* ---------- unprimed state ---------- */

TEST(test_get_before_prime)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 0.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 0.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_range(&pd), 0.0f, 1e-6f);
}

/* ---------- first sample primes ---------- */

TEST(test_first_sample_primes)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, 42.0f);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 42.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 42.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_range(&pd), 0.0f, 1e-6f);
}

/* ---------- infinite hold (decay=0) ---------- */

TEST(test_peak_hold_rising)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, 1.0f);
    peak_detector_feed(&pd, 5.0f);
    peak_detector_feed(&pd, 3.0f);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 5.0f, 1e-6f);
}

TEST(test_valley_hold_falling)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, 10.0f);
    peak_detector_feed(&pd, 2.0f);
    peak_detector_feed(&pd, 7.0f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 2.0f, 1e-6f);
}

TEST(test_range_calculation)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, -3.0f);
    peak_detector_feed(&pd, 7.0f);
    ASSERT_NEAR(peak_detector_get_range(&pd), 10.0f, 1e-6f);
}

TEST(test_monotonic_increasing)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    for (int i = 0; i < 10; i++)
        peak_detector_feed(&pd, (float)i);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 9.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 0.0f, 1e-6f);
}

TEST(test_monotonic_decreasing)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    for (int i = 9; i >= 0; i--)
        peak_detector_feed(&pd, (float)i);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 9.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 0.0f, 1e-6f);
}

/* ---------- decay ---------- */

TEST(test_decay_peak_shrinks)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.1f);
    peak_detector_feed(&pd, 100.0f);
    peak_detector_feed(&pd, 50.0f);
    float p = peak_detector_get_peak(&pd);
    ASSERT_TRUE(p < 100.0f);
    ASSERT_TRUE(p > 50.0f);
}

TEST(test_decay_valley_rises)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.1f);
    peak_detector_feed(&pd, 0.0f);
    peak_detector_feed(&pd, 50.0f);
    float v = peak_detector_get_valley(&pd);
    ASSERT_TRUE(v > 0.0f);
    ASSERT_TRUE(v < 50.0f);
}

TEST(test_decay_one_tracks_signal)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 1.0f);
    peak_detector_feed(&pd, 100.0f);
    peak_detector_feed(&pd, 50.0f);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 50.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 50.0f, 1e-6f);
}

/* ---------- negative values ---------- */

TEST(test_negative_values)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, -5.0f);
    peak_detector_feed(&pd, -10.0f);
    peak_detector_feed(&pd, -3.0f);
    ASSERT_NEAR(peak_detector_get_peak(&pd), -3.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), -10.0f, 1e-6f);
}

/* ---------- NaN rejection ---------- */

TEST(test_feed_nan_rejected)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, 5.0f);
    ASSERT_EQ(peak_detector_feed(&pd, (float)NAN), PEAK_DETECTOR_ERR_PARAM);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 5.0f, 1e-6f);
}

TEST(test_feed_null_rejected)
{
    ASSERT_EQ(peak_detector_feed(NULL, 1.0f), PEAK_DETECTOR_ERR_PARAM);
}

/* ---------- reset ---------- */

TEST(test_reset)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);
    peak_detector_feed(&pd, 100.0f);
    peak_detector_feed(&pd, -50.0f);
    ASSERT_EQ(peak_detector_reset(&pd), PEAK_DETECTOR_OK);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 0.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 0.0f, 1e-6f);
}

TEST(test_reset_null)
{
    ASSERT_EQ(peak_detector_reset(NULL), PEAK_DETECTOR_ERR_PARAM);
}

/* ---------- get with null ---------- */

TEST(test_get_peak_null)
{
    ASSERT_NEAR(peak_detector_get_peak(NULL), 0.0f, 1e-6f);
}

TEST(test_get_valley_null)
{
    ASSERT_NEAR(peak_detector_get_valley(NULL), 0.0f, 1e-6f);
}

TEST(test_get_range_null)
{
    ASSERT_NEAR(peak_detector_get_range(NULL), 0.0f, 1e-6f);
}

/* ---------- infinity must not reach the held peak and valley ---------- */

TEST(test_feed_rejects_infinity)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.1f);

    ASSERT_EQ(peak_detector_feed(&pd, 1.0f), PEAK_DETECTOR_OK);
    ASSERT_EQ(peak_detector_feed(&pd, (float)INFINITY), PEAK_DETECTOR_ERR_PARAM);
    ASSERT_EQ(peak_detector_feed(&pd, (float)-INFINITY), PEAK_DETECTOR_ERR_PARAM);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 1.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 1.0f, 1e-6f);

    /* A held inf turned this step into inf - inf = NaN for good. */
    ASSERT_EQ(peak_detector_feed(&pd, 0.0f), PEAK_DETECTOR_OK);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 0.9f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 0.0f, 1e-6f);
}

TEST(test_infinity_does_not_prime)
{
    peak_detector_t pd;
    peak_detector_init(&pd, 0.0f);

    ASSERT_EQ(peak_detector_feed(&pd, (float)INFINITY), PEAK_DETECTOR_ERR_PARAM);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 0.0f, 1e-6f);

    ASSERT_EQ(peak_detector_feed(&pd, 5.0f), PEAK_DETECTOR_OK);
    ASSERT_NEAR(peak_detector_get_peak(&pd), 5.0f, 1e-6f);
    ASSERT_NEAR(peak_detector_get_valley(&pd), 5.0f, 1e-6f);
}

TEST_MAIN("peak_detector")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_decay_negative);
    RUN(test_init_decay_over_one);
    RUN(test_init_decay_nan);
    RUN(test_get_before_prime);
    RUN(test_first_sample_primes);
    RUN(test_peak_hold_rising);
    RUN(test_valley_hold_falling);
    RUN(test_range_calculation);
    RUN(test_monotonic_increasing);
    RUN(test_monotonic_decreasing);
    RUN(test_decay_peak_shrinks);
    RUN(test_decay_valley_rises);
    RUN(test_decay_one_tracks_signal);
    RUN(test_negative_values);
    RUN(test_feed_nan_rejected);
    RUN(test_feed_null_rejected);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_get_peak_null);
    RUN(test_get_valley_null);
    RUN(test_get_range_null);
    RUN(test_feed_rejects_infinity);
    RUN(test_infinity_does_not_prime);
TEST_MAIN_END()
