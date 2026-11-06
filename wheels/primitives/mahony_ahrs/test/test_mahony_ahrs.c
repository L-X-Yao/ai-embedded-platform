// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <float.h>
#include <string.h>
#include <math.h>
#include "mahony_ahrs.h"
TEST(test_default_cfg)
{
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    ASSERT_NEAR(cfg.kp, 2.0f, 0.01f);
    ASSERT_NEAR(cfg.ki, 0.005f, 0.001f);
    ASSERT_NEAR(cfg.sample_freq_hz, 200.0f, 0.1f);
}

TEST(test_init_quaternion_identity)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    ASSERT_NEAR(q0, 1.0f, 0.001f);
    ASSERT_NEAR(q1, 0.0f, 0.001f);
    ASSERT_NEAR(q2, 0.0f, 0.001f);
    ASSERT_NEAR(q3, 0.0f, 0.001f);
}

TEST(test_init_euler_zero)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 0.1f);
    ASSERT_NEAR(e.pitch_deg, 0.0f, 0.1f);
    ASSERT_NEAR(e.yaw_deg, 0.0f, 0.1f);
}

TEST(test_level_accel_converges)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 2000; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 1.0f);
    ASSERT_NEAR(e.pitch_deg, 0.0f, 1.0f);
}

TEST(test_tilted_accel)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    float ax = sinf(30.0f * 3.14159265f / 180.0f);
    float az = cosf(30.0f * 3.14159265f / 180.0f);

    for (int i = 0; i < 4000; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, ax, 0, az, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.pitch_deg, -30.0f, 3.0f);
}

TEST(test_gyro_integration)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.kp = 0.0f;
    cfg.ki = 0.0f;
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 200; i++) {
        mahony_ahrs_update(&f, 0, 0, 90.0f, 0, 0, 1.0f, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.yaw_deg, 90.0f, 5.0f);
}

TEST(test_null_safety)
{
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_euler_t e;
    float q;

    mahony_ahrs_init(NULL, &cfg);
    mahony_ahrs_init(NULL, NULL);
    mahony_ahrs_update(NULL, 0, 0, 0, 0, 0, 1, 0, 0, 0);
    mahony_ahrs_get_euler(NULL, &e);
    mahony_ahrs_get_euler(NULL, NULL);
    mahony_ahrs_get_quaternion(NULL, &q, &q, &q, &q);
    ASSERT_TRUE(1);

    mahony_ahrs_t f;
    mahony_ahrs_init(&f, &cfg);
    mahony_ahrs_get_quaternion(&f, NULL, NULL, NULL, NULL);
    ASSERT_TRUE(1);
}

TEST(test_reinit_resets)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_ahrs_update(&f, 10, 20, 30, 0, 0, 1, 0, 0, 0);
    }

    mahony_ahrs_init(&f, &cfg);
    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    ASSERT_NEAR(q0, 1.0f, 0.001f);
    ASSERT_NEAR(q1, 0.0f, 0.001f);
    ASSERT_NEAR(f.integral_fb_x, 0.0f, 0.001f);
}

TEST(test_quaternion_norm)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 5000; i++) {
        mahony_ahrs_update(&f, 5, -3, 15, 0.1f, -0.2f, 0.97f, 0.3f, -0.1f, 0.5f);
    }

    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    ASSERT_NEAR(norm, 1.0f, 0.001f);
}

TEST(test_zero_accel_skipped)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    float q0;
    mahony_ahrs_get_quaternion(&f, &q0, NULL, NULL, NULL);
    ASSERT_NEAR(q0, 1.0f, 0.001f);
}

/* Without a gravity reference only the feedback is lost: 90 deg/s about z
 * for 1 s at 200 Hz is still 90 degrees of yaw. */
TEST(test_zero_accel_still_integrates_gyro)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 200; i++) {
        mahony_ahrs_update(&f, 0, 0, 90.0f, 0, 0, 0, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.yaw_deg, 90.0f, 0.05f);
}

/* The reference filter keeps all feedback inside the accel-valid branch:
 * neither the integrators nor the magnetometer may move the estimate. */
TEST(test_zero_accel_applies_no_feedback)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.ki = 0.5f;
    mahony_ahrs_init(&f, &cfg);

    /* a tilted accel charges the integrators */
    for (int i = 0; i < 200; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0.5f, 0, 0.866f, 0, 0, 0);
    }
    ASSERT_TRUE(fabsf(f.integral_fb_y) > 0.001f);

    mahony_ahrs_t before = f;
    for (int i = 0; i < 200; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 0, 0.3f, 0.1f, 0.4f);
    }
    ASSERT_NEAR(f.q0, before.q0, 1e-5f);
    ASSERT_NEAR(f.q1, before.q1, 1e-5f);
    ASSERT_NEAR(f.q2, before.q2, 1e-5f);
    ASSERT_NEAR(f.q3, before.q3, 1e-5f);
    ASSERT_NEAR(f.integral_fb_x, before.integral_fb_x, 0.0f);
    ASSERT_NEAR(f.integral_fb_y, before.integral_fb_y, 0.0f);
    ASSERT_NEAR(f.integral_fb_z, before.integral_fb_z, 0.0f);
}

TEST(test_6axis_vs_9axis)
{
    mahony_ahrs_t f6, f9;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f6, &cfg);
    mahony_ahrs_init(&f9, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_ahrs_update(&f6, 0, 0, 0, 0, 0, 1, 0, 0, 0);
        mahony_ahrs_update(&f9, 0, 0, 0, 0, 0, 1, 0.3f, 0, 0.5f);
    }

    mahony_ahrs_euler_t e6, e9;
    mahony_ahrs_get_euler(&f6, &e6);
    mahony_ahrs_get_euler(&f9, &e9);
    ASSERT_NEAR(e6.roll_deg, 0.0f, 1.0f);
    ASSERT_NEAR(e6.pitch_deg, 0.0f, 1.0f);
    ASSERT_NEAR(e9.roll_deg, 0.0f, 1.0f);
    ASSERT_NEAR(e9.pitch_deg, 0.0f, 1.0f);
}

TEST(test_zero_freq_guard)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.sample_freq_hz = 0.0f;
    mahony_ahrs_init(&f, &cfg);
    mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_TRUE(!isnan(e.roll_deg));
    ASSERT_TRUE(!isnan(e.pitch_deg));
    ASSERT_TRUE(!isnan(e.yaw_deg));
}

TEST(test_negative_freq_guard)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.sample_freq_hz = -100.0f;
    mahony_ahrs_init(&f, &cfg);
    mahony_ahrs_update(&f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_TRUE(!isnan(e.roll_deg));
}

TEST(test_gimbal_lock_pitch90)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 4000; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 1.0f, 0, 0, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.pitch_deg, -90.0f, 3.0f);
    ASSERT_TRUE(!isnan(e.roll_deg));
    ASSERT_TRUE(!isnan(e.yaw_deg));
}

TEST(test_large_gyro_rate_stability)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.sample_freq_hz = 1000.0f;
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 1000; i++) {
        mahony_ahrs_update(&f, 2000, -1500, 1800, 0, 0, 1.0f, 0, 0, 0);
    }

    float q0, q1, q2, q3;
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    float norm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    ASSERT_NEAR(norm, 1.0f, 0.01f);
    ASSERT_TRUE(!isnan(q0));
}

TEST(test_near_inverted_accel)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.kp = 5.0f;
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 8000; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0.05f, 0, -1.0f, 0, 0, 0);
    }

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_TRUE(fabsf(e.roll_deg) > 90.0f || fabsf(e.pitch_deg) > 80.0f);
}

TEST(test_ki_corrects_bias)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    cfg.ki = 0.1f;
    mahony_ahrs_init(&f, &cfg);

    for (int i = 0; i < 5000; i++) {
        mahony_ahrs_update(&f, 5.0f, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }

    ASSERT_TRUE(fabsf(f.integral_fb_x) > 0.001f);

    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.roll_deg, 0.0f, 5.0f);
}

/* Level and still: the estimate must settle at identity. */
static void level_samples(mahony_ahrs_t *f, int n)
{
    for (int i = 0; i < n; i++) {
        mahony_ahrs_update(f, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    }
}

/* One bad gyro or accel value drops the sample and leaves the state alone. */
TEST(test_non_finite_gyro_accel_dropped)
{
    const float bad[] = { NAN, INFINITY, -INFINITY };

    for (int b = 0; b < 3; b++) {
        for (int axis = 0; axis < 6; axis++) {
            mahony_ahrs_t f;
            mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
            mahony_ahrs_init(&f, &cfg);
            level_samples(&f, 10);

            mahony_ahrs_t before = f;
            float in[6] = { 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 1.0f };
            in[axis] = bad[b];
            mahony_ahrs_update(&f, in[0], in[1], in[2], in[3], in[4], in[5],
                               0, 0, 0);
            ASSERT_MEMEQ_N(&f, &before, sizeof(f));

            level_samples(&f, 1000);
            float q0;
            mahony_ahrs_get_quaternion(&f, &q0, NULL, NULL, NULL);
            ASSERT_NEAR(q0, 1.0f, 0.001f);
        }
    }
}

/* A bad magnetometer value means "not available", the same as zeros. */
TEST(test_non_finite_mag_treated_as_absent)
{
    const float bad[] = { NAN, INFINITY, -INFINITY };

    for (int b = 0; b < 3; b++) {
        for (int axis = 0; axis < 3; axis++) {
            mahony_ahrs_t f, ref;
            mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
            mahony_ahrs_init(&f, &cfg);
            mahony_ahrs_init(&ref, &cfg);

            float m[3] = { 0.3f, 0.1f, 0.4f };
            m[axis] = bad[b];
            mahony_ahrs_update(&f, 1.0f, 2.0f, 3.0f, 0.1f, 0, 1.0f,
                               m[0], m[1], m[2]);
            mahony_ahrs_update(&ref, 1.0f, 2.0f, 3.0f, 0.1f, 0, 1.0f, 0, 0, 0);
            ASSERT_MEMEQ_N(&f, &ref, sizeof(f));
        }
    }
}

/* NaN already in the state must not stay: the integrators feed every step. */
TEST(test_nan_in_state_recovers)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    float q0;

    mahony_ahrs_init(&f, &cfg);
    f.q2 = NAN;
    level_samples(&f, 1);
    mahony_ahrs_get_quaternion(&f, &q0, NULL, NULL, NULL);
    ASSERT_NEAR(q0, 1.0f, 0.001f);

    mahony_ahrs_init(&f, &cfg);
    f.integral_fb_x = NAN;
    level_samples(&f, 1);
    ASSERT_NEAR(f.integral_fb_x, 0.0f, 0.001f);

    /* tilted accel: a NaN integrator would reset the estimate every step */
    for (int i = 0; i < 2000; i++) {
        mahony_ahrs_update(&f, 0, 0, 0, 0.5f, 0, 0.866f, 0, 0, 0);
    }
    mahony_ahrs_euler_t e;
    mahony_ahrs_get_euler(&f, &e);
    ASSERT_NEAR(e.pitch_deg, -30.0f, 2.0f);
}

/* A finite rate so large that the norm overflows must not zero the estimate. */
TEST(test_overflowing_norm_keeps_unit_quaternion)
{
    mahony_ahrs_t f;
    mahony_ahrs_cfg_t cfg = mahony_ahrs_default_cfg();
    float q0, q1, q2, q3;

    mahony_ahrs_init(&f, &cfg);
    mahony_ahrs_update(&f, FLT_MAX, 0, 0, 0, 0, 1.0f, 0, 0, 0);
    mahony_ahrs_get_quaternion(&f, &q0, &q1, &q2, &q3);
    ASSERT_NEAR(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3, 1.0f, 0.001f);
}

TEST_MAIN("mahony_ahrs")
    RUN(test_non_finite_gyro_accel_dropped);
    RUN(test_non_finite_mag_treated_as_absent);
    RUN(test_nan_in_state_recovers);
    RUN(test_overflowing_norm_keeps_unit_quaternion);
    RUN(test_default_cfg);
    RUN(test_init_quaternion_identity);
    RUN(test_init_euler_zero);
    RUN(test_level_accel_converges);
    RUN(test_tilted_accel);
    RUN(test_gyro_integration);
    RUN(test_null_safety);
    RUN(test_reinit_resets);
    RUN(test_quaternion_norm);
    RUN(test_zero_accel_skipped);
    RUN(test_zero_accel_still_integrates_gyro);
    RUN(test_zero_accel_applies_no_feedback);
    RUN(test_6axis_vs_9axis);
    RUN(test_zero_freq_guard);
    RUN(test_negative_freq_guard);
    RUN(test_gimbal_lock_pitch90);
    RUN(test_large_gyro_rate_stability);
    RUN(test_near_inverted_accel);
    RUN(test_ki_corrects_bias);
TEST_MAIN_END()

