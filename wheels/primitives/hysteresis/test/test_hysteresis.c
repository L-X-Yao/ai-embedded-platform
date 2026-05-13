// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hysteresis.h"
#include <math.h>

/* === Parameter validation === */

TEST(test_init_null)
{
    ASSERT_EQ(hysteresis_init(NULL, 0.0f, 1.0f), HYSTERESIS_ERR_PARAM);
}

TEST(test_init_low_ge_high)
{
    hysteresis_t ctx;
    ASSERT_EQ(hysteresis_init(&ctx, 5.0f, 5.0f), HYSTERESIS_ERR_PARAM);
    ASSERT_EQ(hysteresis_init(&ctx, 10.0f, 5.0f), HYSTERESIS_ERR_PARAM);
}

TEST(test_init_nan_thresholds)
{
    hysteresis_t ctx;
    float nan_val = NAN;
    ASSERT_EQ(hysteresis_init(&ctx, nan_val, 10.0f), HYSTERESIS_ERR_PARAM);
    ASSERT_EQ(hysteresis_init(&ctx, 0.0f, nan_val), HYSTERESIS_ERR_PARAM);
    ASSERT_EQ(hysteresis_init(&ctx, nan_val, nan_val), HYSTERESIS_ERR_PARAM);
}

TEST(test_init_valid)
{
    hysteresis_t ctx;
    ASSERT_EQ(hysteresis_init(&ctx, 10.0f, 20.0f), HYSTERESIS_OK);
}

TEST(test_feed_null)
{
    ASSERT_FALSE(hysteresis_feed(NULL, 5.0f));
}

TEST(test_get_state_null)
{
    ASSERT_FALSE(hysteresis_get_state(NULL));
}

TEST(test_reset_null)
{
    ASSERT_EQ(hysteresis_reset(NULL), HYSTERESIS_ERR_PARAM);
}

TEST(test_set_thresholds_null)
{
    ASSERT_EQ(hysteresis_set_thresholds(NULL, 1.0f, 2.0f),
              HYSTERESIS_ERR_PARAM);
}

TEST(test_set_thresholds_invalid)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);
    ASSERT_EQ(hysteresis_set_thresholds(&ctx, 20.0f, 10.0f),
              HYSTERESIS_ERR_PARAM);
    ASSERT_EQ(hysteresis_set_thresholds(&ctx, 10.0f, 10.0f),
              HYSTERESIS_ERR_PARAM);
}

/* === Basic hysteresis cycle === */

TEST(test_basic_hysteresis_cycle)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Start below low threshold — should be LOW */
    bool state = hysteresis_feed(&ctx, 5.0f);
    ASSERT_FALSE(state);

    /* Still in dead band — stays LOW */
    state = hysteresis_feed(&ctx, 15.0f);
    ASSERT_FALSE(state);

    /* Cross high threshold — switches to HIGH */
    state = hysteresis_feed(&ctx, 20.0f);
    ASSERT_TRUE(state);

    /* Back into dead band — stays HIGH */
    state = hysteresis_feed(&ctx, 15.0f);
    ASSERT_TRUE(state);

    /* Cross low threshold — switches to LOW */
    state = hysteresis_feed(&ctx, 10.0f);
    ASSERT_FALSE(state);
}

/* === Dead band holds state === */

TEST(test_dead_band_no_change)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Prime to LOW */
    hysteresis_feed(&ctx, 5.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* Values inside dead band should not change state */
    hysteresis_feed(&ctx, 11.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));
    hysteresis_feed(&ctx, 15.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));
    hysteresis_feed(&ctx, 19.9f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));
}

/* === Exactly at thresholds === */

TEST(test_exact_at_thresholds)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Start LOW */
    hysteresis_feed(&ctx, 5.0f);

    /* Exactly at high threshold: >= thresh_high switches to HIGH */
    bool state = hysteresis_feed(&ctx, 20.0f);
    ASSERT_TRUE(state);

    /* Exactly at low threshold: <= thresh_low switches to LOW */
    state = hysteresis_feed(&ctx, 10.0f);
    ASSERT_FALSE(state);
}

/* === Dynamic threshold update === */

TEST(test_dynamic_threshold_update)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Prime to HIGH */
    hysteresis_feed(&ctx, 25.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    /* Widen the band upward */
    ASSERT_EQ(hysteresis_set_thresholds(&ctx, 10.0f, 30.0f), HYSTERESIS_OK);

    /* 25 is now in dead band with high=30, so stays HIGH */
    hysteresis_feed(&ctx, 25.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    /* 30 reaches new high threshold */
    hysteresis_feed(&ctx, 30.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));
}

/* === Reset clears state === */

TEST(test_reset_clears_state)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    hysteresis_feed(&ctx, 25.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    ASSERT_EQ(hysteresis_reset(&ctx), HYSTERESIS_OK);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* After reset, next feed re-primes */
    hysteresis_feed(&ctx, 5.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));
}

/* === Negative thresholds === */

TEST(test_negative_thresholds)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, -20.0f, -10.0f);

    /* Start below low threshold */
    hysteresis_feed(&ctx, -25.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* Cross high threshold */
    bool state = hysteresis_feed(&ctx, -10.0f);
    ASSERT_TRUE(state);

    /* Back below low threshold */
    state = hysteresis_feed(&ctx, -20.0f);
    ASSERT_FALSE(state);
}

/* === Very narrow band === */

TEST(test_narrow_band)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 9.9f, 10.1f);

    hysteresis_feed(&ctx, 9.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* 10.0 is in dead band — stays LOW */
    hysteresis_feed(&ctx, 10.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* Cross high threshold */
    hysteresis_feed(&ctx, 10.1f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    /* Back to 10.0 — in dead band, stays HIGH */
    hysteresis_feed(&ctx, 10.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    /* Cross low threshold */
    hysteresis_feed(&ctx, 9.9f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));
}

/* === Very wide band === */

TEST(test_wide_band)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, -100.0f, 100.0f);

    hysteresis_feed(&ctx, -200.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* 0 is in dead band */
    hysteresis_feed(&ctx, 0.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* Cross high */
    hysteresis_feed(&ctx, 100.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));
}

/* === NaN input guard === */

TEST(test_nan_input_no_change)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Prime to LOW */
    hysteresis_feed(&ctx, 5.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    /* NaN input should not change state */
    float nan_val = NAN;
    bool state = hysteresis_feed(&ctx, nan_val);
    ASSERT_FALSE(state);

    /* Now set to HIGH */
    hysteresis_feed(&ctx, 25.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));

    /* NaN should keep HIGH */
    state = hysteresis_feed(&ctx, nan_val);
    ASSERT_TRUE(state);
}

/* === Get state without feed === */

TEST(test_get_state_before_feed)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);
    /* Before priming, state is false */
    ASSERT_FALSE(hysteresis_get_state(&ctx));
}

/* === First feed primes based on midpoint === */

TEST(test_first_feed_priming)
{
    hysteresis_t ctx;
    hysteresis_init(&ctx, 10.0f, 20.0f);

    /* Midpoint = 15. Value 5 < 15, so primes to LOW. */
    hysteresis_feed(&ctx, 5.0f);
    ASSERT_FALSE(hysteresis_get_state(&ctx));

    hysteresis_reset(&ctx);

    /* Value 25 >= 15, so primes to HIGH. But also >= thresh_high,
     * so standard logic confirms HIGH. */
    hysteresis_feed(&ctx, 25.0f);
    ASSERT_TRUE(hysteresis_get_state(&ctx));
}

TEST_MAIN("hysteresis")
    RUN(test_init_null);
    RUN(test_init_low_ge_high);
    RUN(test_init_nan_thresholds);
    RUN(test_init_valid);
    RUN(test_feed_null);
    RUN(test_get_state_null);
    RUN(test_reset_null);
    RUN(test_set_thresholds_null);
    RUN(test_set_thresholds_invalid);
    RUN(test_basic_hysteresis_cycle);
    RUN(test_dead_band_no_change);
    RUN(test_exact_at_thresholds);
    RUN(test_dynamic_threshold_update);
    RUN(test_reset_clears_state);
    RUN(test_negative_thresholds);
    RUN(test_narrow_band);
    RUN(test_wide_band);
    RUN(test_nan_input_no_change);
    RUN(test_get_state_before_feed);
    RUN(test_first_feed_priming);
TEST_MAIN_END()
