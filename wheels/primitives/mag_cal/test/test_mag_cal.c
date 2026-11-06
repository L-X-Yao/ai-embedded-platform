// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <float.h>
#include <string.h>
#include <math.h>
#include "mag_cal.h"
#define PI 3.14159265f

static void generate_sphere(mag_cal_t *cal, float cx, float cy, float cz,
                            float r, int n)
{
    for (int i = 0; i < n; i++) {
        float theta = PI * (float)i / (float)n;
        float phi = 2.0f * PI * (float)(i * 7 % n) / (float)n;
        float x = cx + r * sinf(theta) * cosf(phi);
        float y = cy + r * sinf(theta) * sinf(phi);
        float z = cz + r * cosf(theta);
        mag_cal_feed(cal, x, y, z);
    }
}

static void generate_ellipsoid(mag_cal_t *cal, float cx, float cy, float cz,
                               float rx, float ry, float rz, int n)
{
    for (int i = 0; i < n; i++) {
        float theta = PI * (float)i / (float)n;
        float phi = 2.0f * PI * (float)(i * 7 % n) / (float)n;
        float x = cx + rx * sinf(theta) * cosf(phi);
        float y = cy + ry * sinf(theta) * sinf(phi);
        float z = cz + rz * cosf(theta);
        mag_cal_feed(cal, x, y, z);
    }
}

/* ---------- tests ---------- */

TEST(test_init)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE);
    ASSERT_EQ(mag_cal_sample_count(&cal), 0);
    mag_cal_init(NULL);
    ASSERT_TRUE(1);
}

TEST(test_start)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_COLLECTING);
}

TEST(test_feed_before_start)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    ASSERT_TRUE(!mag_cal_feed(&cal, 1, 2, 3));
}

TEST(test_sphere_calibration)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0.1f, -0.2f, 0.05f, 0.3f, 500);

    mag_cal_state_t s = mag_cal_compute(&cal);
    ASSERT_EQ(s, MAG_CAL_DONE_OK);

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT_NEAR(r->offset_x, 0.1f, 0.02f);
    ASSERT_NEAR(r->offset_y, -0.2f, 0.02f);
    ASSERT_NEAR(r->offset_z, 0.05f, 0.02f);
    ASSERT_NEAR(r->scale_x, 1.0f, 0.1f);
    ASSERT_NEAR(r->scale_y, 1.0f, 0.1f);
    ASSERT_NEAR(r->scale_z, 1.0f, 0.1f);
}

TEST(test_ellipsoid_calibration)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_ellipsoid(&cal, 0.0f, 0.0f, 0.0f, 0.2f, 0.4f, 0.3f, 500);

    mag_cal_state_t s = mag_cal_compute(&cal);
    ASSERT_EQ(s, MAG_CAL_DONE_OK);

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT_TRUE(r->scale_x > r->scale_y);
    ASSERT_NEAR(r->offset_x, 0.0f, 0.02f);
    ASSERT_NEAR(r->offset_y, 0.0f, 0.02f);
}

TEST(test_apply)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0.5f, -0.3f, 0.1f, 0.4f, 500);
    mag_cal_compute(&cal);

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    float cx, cy, cz;
    mag_cal_apply(r, 0.5f, -0.3f, 0.1f, &cx, &cy, &cz);
    ASSERT_NEAR(cx, 0.0f, 0.02f);
    ASSERT_NEAR(cy, 0.0f, 0.02f);
    ASSERT_NEAR(cz, 0.0f, 0.02f);
}

TEST(test_insufficient_spread)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < 500; i++)
        mag_cal_feed(&cal, 0.1f, 0.2f, 0.3f);

    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST(test_zero_samples_fail)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST(test_null_safety)
{
    ASSERT_EQ(mag_cal_get_state(NULL), MAG_CAL_IDLE);
    ASSERT_TRUE(mag_cal_get_result(NULL) == NULL);
    ASSERT_TRUE(!mag_cal_feed(NULL, 0, 0, 0));
    ASSERT_EQ(mag_cal_sample_count(NULL), 0);
    mag_cal_start(NULL);
    float x, y, z;
    mag_cal_apply(NULL, 1, 2, 3, &x, &y, &z);
    ASSERT_TRUE(1);
}

TEST(test_reinit_clears)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    generate_sphere(&cal, 0, 0, 0, 1.0f, 100);
    ASSERT_EQ(mag_cal_sample_count(&cal), 100);

    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE);
    ASSERT_EQ(mag_cal_sample_count(&cal), 0);
}

TEST(test_buffer_full)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < MAG_CAL_MAX_SAMPLES; i++)
        ASSERT_TRUE(mag_cal_feed(&cal, (float)i, 0, 0));
    ASSERT_TRUE(!mag_cal_feed(&cal, 999, 0, 0));
    ASSERT_EQ(mag_cal_sample_count(&cal), MAG_CAL_MAX_SAMPLES);
}

TEST(test_single_sample)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    mag_cal_feed(&cal, 1.0f, 2.0f, 3.0f);
    ASSERT_EQ(mag_cal_sample_count(&cal), 1);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST(test_collinear_samples)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);
    for (int i = 0; i < 500; i++)
        mag_cal_feed(&cal, (float)i * 0.01f, 0.0f, 0.0f);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST(test_apply_null_outputs)
{
    mag_cal_result_t r = { .offset_x = 1, .offset_y = 2, .offset_z = 3,
                           .scale_x = 1, .scale_y = 1, .scale_z = 1 };
    float x = 999.0f, z = 999.0f;
    mag_cal_apply(&r, 1, 2, 3, &x, NULL, &z);
    ASSERT_NEAR(x, 999.0f, 0.01f);
    ASSERT_NEAR(z, 999.0f, 0.01f);
}

TEST(test_compute_before_start)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST(test_abort_resets_state_and_count)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    mag_cal_feed(&cal, 1.0f, 0.0f, 0.0f);
    mag_cal_feed(&cal, 2.0f, 0.0f, 0.0f);
    mag_cal_feed(&cal, 3.0f, 0.0f, 0.0f);
    ASSERT_EQ(mag_cal_sample_count(&cal), 3);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_COLLECTING);

    mag_cal_abort(&cal);
    ASSERT_EQ(mag_cal_get_state(&cal), MAG_CAL_IDLE);
    ASSERT_EQ(mag_cal_sample_count(&cal), 0);
}

TEST(test_abort_null_is_safe)
{
    mag_cal_abort(NULL);
    ASSERT_TRUE(1);
}

TEST(test_feed_rejects_non_finite)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    ASSERT_FALSE(mag_cal_feed(&cal, INFINITY, 0.0f, 0.0f));
    ASSERT_FALSE(mag_cal_feed(&cal, 0.0f, -INFINITY, 0.0f));
    ASSERT_FALSE(mag_cal_feed(&cal, 0.0f, 0.0f, INFINITY));
    ASSERT_FALSE(mag_cal_feed(&cal, NAN, 0.0f, 0.0f));
    ASSERT_EQ(mag_cal_sample_count(&cal), 0);
}

/* The refused sample leaves the calibration the good ones give. */
TEST(test_infinite_sample_does_not_spoil_result)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    mag_cal_feed(&cal, 1.0f, 1.0f, 1.0f);
    mag_cal_feed(&cal, -1.0f, -1.0f, -1.0f);
    mag_cal_feed(&cal, INFINITY, 0.0f, 0.0f);

    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_OK);
    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT_NEAR(r->offset_x, 0.0f, 1e-6f);
    ASSERT_NEAR(r->scale_x, 1.0f, 1e-6f);
    ASSERT_NEAR(r->scale_y, 1.0f, 1e-6f);
    ASSERT_NEAR(r->scale_z, 1.0f, 1e-6f);
}

/* Feed two samples, then expect DONE_FAIL and the old result untouched. */
static void expect_fail(float a, float b)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    ASSERT_TRUE(mag_cal_feed(&cal, a, a, a));
    ASSERT_TRUE(mag_cal_feed(&cal, b, b, b));
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);

    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    ASSERT_NEAR(r->offset_x, 0.0f, 1e-6f);
    ASSERT_NEAR(r->scale_x, 1.0f, 1e-6f);
}

/* Finite samples can still overflow the arithmetic in compute. */
TEST(test_compute_fails_on_overflow)
{
    expect_fail(FLT_MAX, -FLT_MAX);    /* max - min overflows the radius */
    expect_fail(FLT_MAX, 3.0e38f);     /* max + min overflows the offset */
    expect_fail(1.5e38f, -1.5e38f);    /* the mean radius overflows */
}

/* Every radius and the mean are finite, but 5e37 / 0.01 is not. */
TEST(test_compute_fails_on_scale_overflow)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    mag_cal_feed(&cal, 1.5e38f, 0.01f, 0.01f);
    mag_cal_feed(&cal, -1.5e38f, -0.01f, -0.01f);
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

/* samples[] is public: what feed would refuse can still be in there. */
TEST(test_compute_fails_on_bad_stored_sample)
{
    mag_cal_t cal;
    mag_cal_init(&cal);
    mag_cal_start(&cal);

    mag_cal_feed(&cal, 1.0f, 1.0f, 1.0f);
    mag_cal_feed(&cal, -1.0f, -1.0f, -1.0f);
    cal.samples[0][0] = NAN;
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);

    mag_cal_start(&cal);
    mag_cal_feed(&cal, 1.0f, 1.0f, 1.0f);
    mag_cal_feed(&cal, -1.0f, -1.0f, -1.0f);
    cal.samples[1][2] = INFINITY;
    ASSERT_EQ(mag_cal_compute(&cal), MAG_CAL_DONE_FAIL);
}

TEST_MAIN("mag_cal")
    RUN(test_feed_rejects_non_finite);
    RUN(test_infinite_sample_does_not_spoil_result);
    RUN(test_compute_fails_on_overflow);
    RUN(test_compute_fails_on_scale_overflow);
    RUN(test_compute_fails_on_bad_stored_sample);
    RUN(test_init);
    RUN(test_start);
    RUN(test_feed_before_start);
    RUN(test_sphere_calibration);
    RUN(test_ellipsoid_calibration);
    RUN(test_apply);
    RUN(test_insufficient_spread);
    RUN(test_zero_samples_fail);
    RUN(test_null_safety);
    RUN(test_reinit_clears);
    RUN(test_buffer_full);
    RUN(test_single_sample);
    RUN(test_collinear_samples);
    RUN(test_apply_null_outputs);
    RUN(test_compute_before_start);
    RUN(test_abort_resets_state_and_count);
    RUN(test_abort_null_is_safe);
TEST_MAIN_END()

