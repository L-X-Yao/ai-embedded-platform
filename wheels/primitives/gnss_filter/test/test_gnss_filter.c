#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "gnss_filter.h"

/* ---------- helpers ---------- */

static const gnss_filter_cfg_t g_default_cfg = {
    .min_satellites  = 8,
    .max_hdop        = 1.5f,
    .max_vdop        = 3.0f,
    .stable_ms       = 10000,
    .static_alpha    = 0.20f,
    .strong_alpha    = 0.05f,
    .static_speed_cms = 100,
    .zupt_motion_mg  = 50,
};

static gnss_filter_input_t good_fix(void)
{
    gnss_filter_input_t in = {0};
    in.latitude    = 30.0f;
    in.longitude   = 120.0f;
    in.altitude    = 100.0f;
    in.speed_cms   = 0;
    in.satellites  = 12;
    in.fix_mode    = 2;
    in.fix_quality = 1;
    in.hdop        = 0.8f;
    in.vdop        = 1.2f;
    in.fix_valid   = true;
    return in;
}

/* ---------- tests ---------- */

TEST(test_init_null)
{
    gnss_filter_t flt;
    ASSERT_EQ(GNSS_FILTER_ERR_PARAM, gnss_filter_init(NULL, &g_default_cfg));
    ASSERT_EQ(GNSS_FILTER_ERR_PARAM, gnss_filter_init(&flt, NULL));
}

TEST(test_init_ok)
{
    gnss_filter_t flt;
    ASSERT_EQ(GNSS_FILTER_OK, gnss_filter_init(&flt, &g_default_cfg));
    ASSERT_FALSE(gnss_filter_is_ready(&flt));
}

TEST(test_update_null)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();
    ASSERT_EQ(GNSS_FILTER_ERR_PARAM, gnss_filter_update(NULL, &in, 0, 0));
    ASSERT_EQ(GNSS_FILTER_ERR_PARAM, gnss_filter_update(&flt, NULL, 0, 0));
}

TEST(test_not_ready_without_stable_time)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Feed good fix for less than stable_ms */
    for (uint32_t t = 0; t < 9000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_FALSE(gnss_filter_is_ready(&flt));
}

TEST(test_ready_after_stable_time)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Feed good fix past stable_ms threshold */
    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_TRUE(gnss_filter_is_ready(&flt));
    ASSERT_TRUE(fabsf(gnss_filter_lat(&flt) - 30.0f) < 0.01f);
    ASSERT_TRUE(fabsf(gnss_filter_lon(&flt) - 120.0f) < 0.01f);
}

TEST(test_quality_resets_on_bad_fix)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Accumulate 5s of quality */
    for (uint32_t t = 0; t < 5000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_FALSE(gnss_filter_is_ready(&flt));

    /* Bad fix resets accumulation */
    in.satellites = 3;
    gnss_filter_update(&flt, &in, 0, 5200);

    /* Continue with good fix — need full 10s again */
    in.satellites = 12;
    for (uint32_t t = 5400; t < 14000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_FALSE(gnss_filter_is_ready(&flt));

    for (uint32_t t = 14000; t <= 16000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_TRUE(gnss_filter_is_ready(&flt));
}

TEST(test_ema_stationary)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Get to ready state */
    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_TRUE(gnss_filter_is_ready(&flt));

    /* Inject drift: position jumps slightly (stationary, no IMU motion) */
    in.latitude = 30.001f;
    gnss_filter_update(&flt, &in, 10, 11200); /* motion=10 < 50 → ZUPT */

    /* With strong_alpha=0.05, filter barely moves */
    float lat = gnss_filter_lat(&flt);
    ASSERT_TRUE(lat < 30.0001f); /* barely moved from 30.0 */
}

TEST(test_ema_moving)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Get to ready state */
    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }

    /* Moving: speed > threshold AND IMU motion > zupt_motion_mg */
    in.speed_cms = 500;
    in.latitude = 30.01f;
    gnss_filter_update(&flt, &in, 200, 11200);

    /* When moving, filter seeds directly */
    float lat = gnss_filter_lat(&flt);
    ASSERT_TRUE(fabsf(lat - 30.01f) < 0.0001f);
}

TEST(test_zupt_overrides_speed)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    /* Get to ready state */
    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }

    /* GNSS shows speed but IMU says stationary → EMA (not seed) */
    in.speed_cms = 200; /* above threshold */
    in.latitude = 30.01f;
    gnss_filter_update(&flt, &in, 10, 11200); /* IMU motion < 50 = stationary */

    float lat = gnss_filter_lat(&flt);
    /* Should EMA, not seed. strong_alpha=0.05 → barely moved */
    ASSERT_TRUE(lat < 30.001f);
}

TEST(test_reset)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();

    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_TRUE(gnss_filter_is_ready(&flt));

    gnss_filter_reset(&flt);
    ASSERT_FALSE(gnss_filter_is_ready(&flt));
    ASSERT_TRUE(fabsf(gnss_filter_lat(&flt)) < 0.0001f);
}

TEST(test_reset_null)
{
    ASSERT_EQ(GNSS_FILTER_ERR_PARAM, gnss_filter_reset(NULL));
}

TEST(test_getters_null)
{
    ASSERT_FALSE(gnss_filter_is_ready(NULL));
    ASSERT_TRUE(fabsf(gnss_filter_lat(NULL)) < 0.0001f);
    ASSERT_TRUE(fabsf(gnss_filter_lon(NULL)) < 0.0001f);
    ASSERT_TRUE(fabsf(gnss_filter_alt(NULL)) < 0.0001f);
}

TEST(test_hdop_too_high)
{
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_default_cfg);
    gnss_filter_input_t in = good_fix();
    in.hdop = 2.0f; /* above 1.5 max */

    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_FALSE(gnss_filter_is_ready(&flt));
}

TEST(test_datetime_to_unix_epoch)
{
    ASSERT_EQ(0LL, gnss_datetime_to_unix(1970, 1, 1, 0, 0, 0));
}

TEST(test_datetime_to_unix_known)
{
    /* 2024-01-01 00:00:00 UTC = 1704067200 */
    int64_t ts = gnss_datetime_to_unix(2024, 1, 1, 0, 0, 0);
    ASSERT_EQ(1704067200LL, ts);
}

TEST(test_datetime_to_unix_with_time)
{
    /* 2026-05-06 12:30:45 */
    int64_t ts = gnss_datetime_to_unix(2026, 5, 6, 12, 30, 45);
    /* 2026-01-01 = 1735689600 + Jan(31)+Feb(28)+Mar(31)+Apr(30)+5days = 125 days
     * 125*86400 + 12*3600 + 30*60 + 45 */
    int64_t jan1_2026 = gnss_datetime_to_unix(2026, 1, 1, 0, 0, 0);
    int64_t expected = jan1_2026 + (int64_t)(31+28+31+30+5)*86400 + 12*3600 + 30*60 + 45;
    ASSERT_EQ(expected, ts);
}

TEST(test_vdop_zero_ignored)
{
    gnss_filter_t flt;
    gnss_filter_cfg_t cfg = g_default_cfg;
    cfg.max_vdop = 3.0f;
    gnss_filter_init(&flt, &cfg);
    gnss_filter_input_t in = good_fix();
    in.vdop = 0.0f; /* zero means unavailable, should not fail */

    for (uint32_t t = 0; t <= 11000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    ASSERT_TRUE(gnss_filter_is_ready(&flt));
}

TEST_MAIN("gnss_filter")
    RUN(test_init_null);
    RUN(test_init_ok);
    RUN(test_update_null);
    RUN(test_not_ready_without_stable_time);
    RUN(test_ready_after_stable_time);
    RUN(test_quality_resets_on_bad_fix);
    RUN(test_ema_stationary);
    RUN(test_ema_moving);
    RUN(test_zupt_overrides_speed);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_getters_null);
    RUN(test_hdop_too_high);
    RUN(test_datetime_to_unix_epoch);
    RUN(test_datetime_to_unix_known);
    RUN(test_datetime_to_unix_with_time);
    RUN(test_vdop_zero_ignored);
TEST_MAIN_END()
