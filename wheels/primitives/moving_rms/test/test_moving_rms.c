// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "moving_rms.h"
#include <math.h>
#include <string.h>

static float win[8];
static moving_rms_t rms;

static void setup(void) { moving_rms_init(&rms, win, 8); }

TEST(test_init_ok)
{
    float buf[4];
    moving_rms_t r;
    ASSERT_EQ(moving_rms_init(&r, buf, 4), MOVING_RMS_OK);
}

TEST(test_init_null)
{
    float buf[4];
    ASSERT_EQ(moving_rms_init(NULL, buf, 4), MOVING_RMS_ERR_PARAM);
    moving_rms_t r;
    ASSERT_EQ(moving_rms_init(&r, NULL, 4), MOVING_RMS_ERR_PARAM);
    ASSERT_EQ(moving_rms_init(&r, buf, 0), MOVING_RMS_ERR_PARAM);
}

TEST(test_get_empty)
{
    setup();
    ASSERT_NEAR(moving_rms_get(&rms), 0.0f, 1e-6f);
}

TEST(test_dc_signal)
{
    setup();
    for (int i = 0; i < 8; i++)
        moving_rms_feed(&rms, 5.0f);
    ASSERT_NEAR(moving_rms_get(&rms), 5.0f, 0.01f);
}

TEST(test_negative_dc)
{
    setup();
    for (int i = 0; i < 8; i++)
        moving_rms_feed(&rms, -3.0f);
    ASSERT_NEAR(moving_rms_get(&rms), 3.0f, 0.01f);
}

TEST(test_sine_rms)
{
    float buf[100];
    moving_rms_t r;
    moving_rms_init(&r, buf, 100);
    for (int i = 0; i < 200; i++)
        moving_rms_feed(&r, sinf((float)i * 0.0628318f));
    /* RMS of sine = 1/sqrt(2) ≈ 0.7071 */
    ASSERT_NEAR(moving_rms_get(&r), 0.7071f, 0.02f);
}

TEST(test_single_sample)
{
    setup();
    float v = moving_rms_feed(&rms, 4.0f);
    ASSERT_NEAR(v, 4.0f, 1e-5f);
}

TEST(test_window_slides)
{
    float buf[2];
    moving_rms_t r;
    moving_rms_init(&r, buf, 2);
    moving_rms_feed(&r, 3.0f);
    moving_rms_feed(&r, 4.0f);
    /* RMS of [3,4] = sqrt((9+16)/2) = sqrt(12.5) */
    ASSERT_NEAR(moving_rms_get(&r), sqrtf(12.5f), 0.01f);
    moving_rms_feed(&r, 0.0f);
    /* RMS of [4,0] = sqrt(16/2) = sqrt(8) */
    ASSERT_NEAR(moving_rms_get(&r), sqrtf(8.0f), 0.01f);
}

TEST(test_nan_rejected)
{
    setup();
    moving_rms_feed(&rms, 5.0f);
    float prev = moving_rms_get(&rms);
    moving_rms_feed(&rms, (float)NAN);
    ASSERT_NEAR(moving_rms_get(&rms), prev, 1e-6f);
}

TEST(test_reset)
{
    setup();
    moving_rms_feed(&rms, 10.0f);
    moving_rms_reset(&rms);
    ASSERT_NEAR(moving_rms_get(&rms), 0.0f, 1e-6f);
}

TEST(test_reset_null)
{
    ASSERT_EQ(moving_rms_reset(NULL), MOVING_RMS_ERR_PARAM);
}

TEST(test_get_null)
{
    ASSERT_NEAR(moving_rms_get(NULL), 0.0f, 1e-6f);
}

TEST(test_feed_null)
{
    ASSERT_NEAR(moving_rms_feed(NULL, 1.0f), 0.0f, 1e-6f);
}

/* A failed init leaves the context as the caller had it, zeroed here */
TEST(test_feed_after_failed_init)
{
    moving_rms_t r;
    memset(&r, 0, sizeof(r));
    ASSERT_EQ(moving_rms_init(&r, NULL, 8), MOVING_RMS_ERR_PARAM);

    ASSERT_NEAR(moving_rms_feed(&r, 1.0f), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_rms_get(&r), 0.0f, 1e-6f);
    ASSERT_EQ(r.count, 0);
}

/* cap 0 would divide by zero when head moves */
TEST(test_feed_zero_capacity)
{
    float buf[1] = { 0.0f };
    moving_rms_t r;
    memset(&r, 0, sizeof(r));
    r.buf = buf;

    ASSERT_NEAR(moving_rms_feed(&r, 1.0f), 0.0f, 1e-6f);
    ASSERT_EQ(r.count, 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)MOVING_RMS_OK, 0);
    ASSERT_EQ((int)MOVING_RMS_ERR_PARAM, -1);
}

/* --- input that would leave the running sum NaN for good --- */

TEST(test_feed_rejects_infinity)
{
    setup();
    ASSERT_NEAR(moving_rms_feed(&rms, 1.0f), 1.0f, 1e-6f);
    ASSERT_NEAR(moving_rms_feed(&rms, (float)INFINITY), 1.0f, 1e-6f);
    ASSERT_NEAR(moving_rms_feed(&rms, (float)-INFINITY), 1.0f, 1e-6f);
    ASSERT_EQ(rms.count, 1);

    /* (1 + 9) / 2 */
    ASSERT_NEAR(moving_rms_feed(&rms, 3.0f), sqrtf(5.0f), 1e-5f);
}

TEST(test_feed_rejects_sample_whose_square_overflows)
{
    setup();
    /* 2e19 is finite, its square 4e38 is not: FLT_MAX is about 3.4e38. */
    ASSERT_NEAR(moving_rms_feed(&rms, 2.0f), 2.0f, 1e-6f);
    ASSERT_NEAR(moving_rms_feed(&rms, 2e19f), 2.0f, 1e-6f);
    ASSERT_EQ(rms.count, 1);
    ASSERT_NEAR(moving_rms_feed(&rms, 2.0f), 2.0f, 1e-6f);
}

TEST(test_feed_rejects_sample_overflowing_sum)
{
    setup();
    /* Each square is about 2.25e38, finite; together they are not. */
    float first = moving_rms_feed(&rms, 1.5e19f);
    ASSERT_TRUE(moving_rms_feed(&rms, 1.5e19f) == first);
    ASSERT_EQ(rms.count, 1);
}

/* 32767^2 is so large that float loses the +-5 squares beside it. Two windows
 * later only +-5 is left and the RMS must say so, wherever the sample fell. */
TEST(test_large_sample_leaves_no_residue)
{
    float buf[64];
    moving_rms_t r;

    for (int pos = 0; pos < 64; pos++) {
        moving_rms_init(&r, buf, 64);
        for (int i = 0; i < pos; i++) {
            moving_rms_feed(&r, 5.0f);
        }
        moving_rms_feed(&r, 32767.0f);

        float v = 0.0f;
        for (int i = 0; i < 2 * 64; i++) {
            v = moving_rms_feed(&r, (i & 1) ? 5.0f : -5.0f);
        }
        ASSERT_NEAR(v, 5.0f, 1e-3f);
    }
}

TEST(test_large_sample_leaves_no_residue_small_window)
{
    float buf[8];
    moving_rms_t r;
    moving_rms_init(&r, buf, 8);

    moving_rms_feed(&r, 1e6f);
    float v = 0.0f;
    for (int i = 0; i < 2 * 8; i++) {
        v = moving_rms_feed(&r, 1.0f);
    }
    ASSERT_NEAR(v, 1.0f, 1e-5f);
}

/* While the large sample is in the window it must still count in full. */
TEST(test_large_sample_counts_while_in_window)
{
    float buf[64];
    moving_rms_t r;
    /* sqrt((32767^2 + 63 * 5^2) / 64) */
    const float want = (float)sqrt((32767.0 * 32767.0 + 63.0 * 25.0) / 64.0);

    for (int pos = 0; pos < 64; pos++) {
        moving_rms_init(&r, buf, 64);
        for (int i = 0; i < pos; i++) {
            moving_rms_feed(&r, 5.0f);
        }
        moving_rms_feed(&r, 32767.0f);

        float v = 0.0f;
        for (int i = 0; i < 63; i++) {
            v = moving_rms_feed(&r, 5.0f);
        }
        ASSERT_NEAR(v, want, 0.5f);
    }
}

TEST_MAIN("moving_rms")
    RUN(test_large_sample_leaves_no_residue);
    RUN(test_large_sample_leaves_no_residue_small_window);
    RUN(test_large_sample_counts_while_in_window);
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_get_empty);
    RUN(test_dc_signal);
    RUN(test_negative_dc);
    RUN(test_sine_rms);
    RUN(test_single_sample);
    RUN(test_window_slides);
    RUN(test_nan_rejected);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_get_null);
    RUN(test_feed_null);
    RUN(test_feed_after_failed_init);
    RUN(test_feed_zero_capacity);
    RUN(test_error_codes);
    RUN(test_feed_rejects_infinity);
    RUN(test_feed_rejects_sample_whose_square_overflows);
    RUN(test_feed_rejects_sample_overflowing_sum);
TEST_MAIN_END()
