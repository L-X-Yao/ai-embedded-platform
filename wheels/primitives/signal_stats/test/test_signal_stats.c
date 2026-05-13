#include "test_harness.h"
#include "signal_stats.h"
#include <math.h>

TEST(test_init_ok)
{
    signal_stats_t s;
    ASSERT_EQ(signal_stats_init(&s), SIGNAL_STATS_OK);
    ASSERT_EQ(signal_stats_count(&s), 0u);
}

TEST(test_init_null)
{
    ASSERT_EQ(signal_stats_init(NULL), SIGNAL_STATS_ERR_PARAM);
}

TEST(test_empty_getters)
{
    signal_stats_t s;
    signal_stats_init(&s);
    ASSERT_NEAR(signal_stats_mean(&s), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_variance(&s), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_min(&s), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_max(&s), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_range(&s), 0.0f, 1e-6f);
}

TEST(test_single_sample)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 42.0f);
    ASSERT_EQ(signal_stats_count(&s), 1u);
    ASSERT_NEAR(signal_stats_mean(&s), 42.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_variance(&s), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_min(&s), 42.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_max(&s), 42.0f, 1e-6f);
}

TEST(test_known_mean)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 2.0f);
    signal_stats_feed(&s, 4.0f);
    signal_stats_feed(&s, 6.0f);
    ASSERT_NEAR(signal_stats_mean(&s), 4.0f, 1e-5f);
}

TEST(test_known_variance)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 2.0f);
    signal_stats_feed(&s, 4.0f);
    signal_stats_feed(&s, 4.0f);
    signal_stats_feed(&s, 4.0f);
    signal_stats_feed(&s, 5.0f);
    signal_stats_feed(&s, 5.0f);
    signal_stats_feed(&s, 7.0f);
    signal_stats_feed(&s, 9.0f);
    /* Population variance = 4.0 */
    ASSERT_NEAR(signal_stats_variance(&s), 4.0f, 0.01f);
}

TEST(test_min_max)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 5.0f);
    signal_stats_feed(&s, -3.0f);
    signal_stats_feed(&s, 10.0f);
    signal_stats_feed(&s, 1.0f);
    ASSERT_NEAR(signal_stats_min(&s), -3.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_max(&s), 10.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_range(&s), 13.0f, 1e-6f);
}

TEST(test_negative_values)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, -10.0f);
    signal_stats_feed(&s, -20.0f);
    ASSERT_NEAR(signal_stats_mean(&s), -15.0f, 1e-5f);
    ASSERT_NEAR(signal_stats_min(&s), -20.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_max(&s), -10.0f, 1e-6f);
}

TEST(test_constant_signal)
{
    signal_stats_t s;
    signal_stats_init(&s);
    for (int i = 0; i < 100; i++)
        signal_stats_feed(&s, 7.0f);
    ASSERT_NEAR(signal_stats_mean(&s), 7.0f, 1e-5f);
    ASSERT_NEAR(signal_stats_variance(&s), 0.0f, 1e-5f);
}

TEST(test_nan_rejected)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 1.0f);
    ASSERT_EQ(signal_stats_feed(&s, (float)NAN), SIGNAL_STATS_ERR_PARAM);
    ASSERT_EQ(signal_stats_count(&s), 1u);
}

TEST(test_feed_null)
{
    ASSERT_EQ(signal_stats_feed(NULL, 1.0f), SIGNAL_STATS_ERR_PARAM);
}

TEST(test_reset)
{
    signal_stats_t s;
    signal_stats_init(&s);
    signal_stats_feed(&s, 100.0f);
    signal_stats_reset(&s);
    ASSERT_EQ(signal_stats_count(&s), 0u);
    ASSERT_NEAR(signal_stats_mean(&s), 0.0f, 1e-6f);
}

TEST(test_reset_null)
{
    ASSERT_EQ(signal_stats_reset(NULL), SIGNAL_STATS_ERR_PARAM);
}

TEST(test_null_getters)
{
    ASSERT_NEAR(signal_stats_mean(NULL), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_variance(NULL), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_min(NULL), 0.0f, 1e-6f);
    ASSERT_NEAR(signal_stats_max(NULL), 0.0f, 1e-6f);
    ASSERT_EQ(signal_stats_count(NULL), 0u);
    ASSERT_NEAR(signal_stats_range(NULL), 0.0f, 1e-6f);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)SIGNAL_STATS_OK, 0);
    ASSERT_EQ((int)SIGNAL_STATS_ERR_PARAM, -1);
}

TEST_MAIN("signal_stats")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_empty_getters);
    RUN(test_single_sample);
    RUN(test_known_mean);
    RUN(test_known_variance);
    RUN(test_min_max);
    RUN(test_negative_values);
    RUN(test_constant_signal);
    RUN(test_nan_rejected);
    RUN(test_feed_null);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_null_getters);
    RUN(test_error_codes);
TEST_MAIN_END()
