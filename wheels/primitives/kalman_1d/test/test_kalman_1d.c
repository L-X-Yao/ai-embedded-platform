// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "kalman_1d.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

TEST(test_init_ok)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_OK);
    /* After init: x and p are zeroed (p==0 signals not-yet-bootstrapped) */
    ASSERT_NEAR(kf.x, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.p, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.q, 0.1f, 1e-9f);
    ASSERT_NEAR(kf.r, 1.0f, 1e-9f);
}

TEST(test_init_null)
{
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    ASSERT_EQ(kalman_1d_init(NULL, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    kalman_1d_t kf;
    ASSERT_EQ(kalman_1d_init(&kf, NULL), KALMAN_1D_ERR_PARAM);
}

TEST(test_init_q_zero)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.0f, 1.0f};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_init_r_zero)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 0.0f};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_init_q_negative)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {-0.1f, 1.0f};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_init_q_nan)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {(float)NAN, 1.0f};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_bootstrap)
{
    /* First update must set x exactly to the measurement and k = 1.0 */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    float z = 42.5f;
    float out = kalman_1d_update(&kf, z);
    ASSERT_NEAR(out, z, 1e-6f);
    ASSERT_NEAR(kf.k, 1.0f, 1e-6f);
    /* After bootstrap p must be positive (non-zero) */
    ASSERT_TRUE(kf.p > 0.0f);
}

TEST(test_convergence)
{
    /* Feed the same constant 50 times; estimate should converge to it */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.01f, 0.5f};
    kalman_1d_init(&kf, &cfg);

    float true_val = 37.0f;
    float out = 0.0f;
    for (int i = 0; i < 50; i++)
    {
        out = kalman_1d_update(&kf, true_val);
    }
    ASSERT_NEAR(out, true_val, 1e-3f);
}

TEST(test_noise_rejection)
{
    /*
     * Deterministic pseudo-noise added to a constant true value.
     * After filtering the estimate should be closer to true_val than
     * the average absolute deviation of the raw (noisy) measurements.
     */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.01f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    float noise[] = {0.5f, -0.3f, 0.7f, -0.6f, 0.4f,
                     -0.2f, 0.8f, -0.5f, 0.3f, -0.7f};
    float true_val = 10.0f;

    float out = 0.0f;
    for (int i = 0; i < 50; i++)
    {
        float z = true_val + noise[i % 10];
        out = kalman_1d_update(&kf, z);
    }

    /* Filter error must be smaller than typical noise magnitude (0.5) */
    float filter_err = out - true_val;
    if (filter_err < 0.0f) filter_err = -filter_err;
    ASSERT_TRUE(filter_err < 0.5f);
}

TEST(test_step_response)
{
    /*
     * Feed 0 for 20 samples, then 100 for 30 samples.
     * Output must track the step: first phase near 0, second phase
     * significantly above 50 (well past the midpoint).
     */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 0.5f};
    kalman_1d_init(&kf, &cfg);

    float out = 0.0f;
    for (int i = 0; i < 20; i++)
    {
        out = kalman_1d_update(&kf, 0.0f);
    }
    ASSERT_NEAR(out, 0.0f, 0.5f);

    for (int i = 0; i < 30; i++)
    {
        out = kalman_1d_update(&kf, 100.0f);
    }
    /* Should have tracked well into the new level */
    ASSERT_TRUE(out > 50.0f);
}

TEST(test_gain_decreases)
{
    /*
     * With constant measurements the filter gains confidence in its
     * estimate and K should decrease (fall well below the bootstrap 1.0).
     */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.001f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    float val = 5.0f;
    for (int i = 0; i < 100; i++)
    {
        kalman_1d_update(&kf, val);
    }
    ASSERT_TRUE(kf.k < 0.5f);
}

TEST(test_high_q_fast_response)
{
    /*
     * Large Q (high process noise) → filter trusts measurements more →
     * faster response.  Both filters are primed on 0 for 20 samples so
     * their covariances have settled; then a step to 100 is applied for
     * 10 samples.  The high-Q filter must track further toward 100.
     */
    kalman_1d_t kf_fast, kf_slow;
    kalman_1d_cfg_t cfg_fast = {1.0f,   1.0f};
    kalman_1d_cfg_t cfg_slow = {0.001f, 1.0f};
    kalman_1d_init(&kf_fast, &cfg_fast);
    kalman_1d_init(&kf_slow, &cfg_slow);

    /* Prime on steady 0 to settle covariances */
    for (int i = 0; i < 20; i++)
    {
        kalman_1d_update(&kf_fast, 0.0f);
        kalman_1d_update(&kf_slow, 0.0f);
    }

    float out_fast = 0.0f, out_slow = 0.0f;
    for (int i = 0; i < 10; i++)
    {
        out_fast = kalman_1d_update(&kf_fast, 100.0f);
        out_slow = kalman_1d_update(&kf_slow, 100.0f);
    }
    ASSERT_TRUE(out_fast > out_slow);
}

TEST(test_high_r_slow_response)
{
    /*
     * Large R (high measurement noise) → filter trusts measurements less →
     * more smoothing / slower tracking.  Both filters are primed on 0 for
     * 20 samples to settle their covariances; then a step to 100 is applied
     * for 10 samples.  The low-R filter must track further toward 100.
     */
    kalman_1d_t kf_smooth, kf_track;
    kalman_1d_cfg_t cfg_smooth = {0.1f, 100.0f};
    kalman_1d_cfg_t cfg_track  = {0.1f,   0.1f};
    kalman_1d_init(&kf_smooth, &cfg_smooth);
    kalman_1d_init(&kf_track,  &cfg_track);

    /* Prime on steady 0 to settle covariances */
    for (int i = 0; i < 20; i++)
    {
        kalman_1d_update(&kf_smooth, 0.0f);
        kalman_1d_update(&kf_track,  0.0f);
    }

    float out_smooth = 0.0f, out_track = 0.0f;
    for (int i = 0; i < 10; i++)
    {
        out_smooth = kalman_1d_update(&kf_smooth, 100.0f);
        out_track  = kalman_1d_update(&kf_track,  100.0f);
    }
    ASSERT_TRUE(out_track > out_smooth);
}

TEST(test_reset)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    kalman_1d_update(&kf, 50.0f);
    kalman_1d_update(&kf, 55.0f);

    kalman_1d_reset(&kf);
    ASSERT_NEAR(kf.x, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.p, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.k, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.q, 0.0f, 1e-9f);
    ASSERT_NEAR(kf.r, 0.0f, 1e-9f);
}

TEST(test_set_noise)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    ASSERT_EQ(kalman_1d_set_noise(&kf, 0.5f, 2.0f), KALMAN_1D_OK);
    ASSERT_NEAR(kf.q, 0.5f, 1e-9f);
    ASSERT_NEAR(kf.r, 2.0f, 1e-9f);
}

TEST(test_set_noise_invalid)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    /* q == 0 */
    ASSERT_EQ(kalman_1d_set_noise(&kf, 0.0f, 1.0f), KALMAN_1D_ERR_PARAM);
    /* r negative */
    ASSERT_EQ(kalman_1d_set_noise(&kf, 0.1f, -1.0f), KALMAN_1D_ERR_PARAM);
    /* null handle */
    ASSERT_EQ(kalman_1d_set_noise(NULL, 0.1f, 1.0f), KALMAN_1D_ERR_PARAM);
}

TEST(test_get_null)
{
    ASSERT_NEAR(kalman_1d_get(NULL), 0.0f, 1e-9f);
}

TEST(test_get_gain_null)
{
    ASSERT_NEAR(kalman_1d_get_gain(NULL), 0.0f, 1e-9f);
}

TEST(test_update_null)
{
    ASSERT_NEAR(kalman_1d_update(NULL, 42.0f), 0.0f, 1e-9f);
}

TEST(test_init_r_nan)
{
    /* r NaN must be rejected the same way q NaN is */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, (float)NAN};
    ASSERT_EQ(kalman_1d_init(&kf, &cfg), KALMAN_1D_ERR_PARAM);
}

TEST(test_set_noise_nan)
{
    /* set_noise uses !(q > 0.0f) which rejects NaN via NaN-safe guard */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    ASSERT_EQ(kalman_1d_set_noise(&kf, (float)NAN, 1.0f), KALMAN_1D_ERR_PARAM);
    ASSERT_EQ(kalman_1d_set_noise(&kf, 0.1f, (float)NAN), KALMAN_1D_ERR_PARAM);
}

TEST(test_update_nan)
{
    /* NaN measurement must not corrupt state; current estimate returned */
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.1f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    float known = 10.0f;
    kalman_1d_update(&kf, known);

    float before = kalman_1d_get(&kf);
    float result = kalman_1d_update(&kf, (float)NAN);

    /* returned value equals current estimate (NaN not injected) */
    ASSERT_NEAR(result, before, 1e-6f);
    /* internal state must not be NaN */
    ASSERT_TRUE(kalman_1d_get(&kf) == kalman_1d_get(&kf));
}

/* q=0.01 r=1 fed 1, 2, 3, by hand:
 *   after 2: p_pred = 1.01, k = 1.01/2.01, x = 1.5024876, p = 0.5024876
 *   after 3: p_pred = 0.5124876, k = 0.3388374, x = 2.0099006
 * A bad sample between 2 and 3 must not change any of that. */
static void check_sample_is_skipped(float bad)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.01f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    kalman_1d_update(&kf, 1.0f);
    ASSERT_NEAR(kalman_1d_update(&kf, 2.0f), 1.5024876f, 1e-5f);
    ASSERT_NEAR(kalman_1d_update(&kf, bad), 1.5024876f, 1e-5f);
    ASSERT_NEAR(kalman_1d_update(&kf, 3.0f), 2.0099006f, 1e-4f);
}

TEST(test_update_infinity_is_skipped)
{
    check_sample_is_skipped((float)INFINITY);
    check_sample_is_skipped(-(float)INFINITY);
}

TEST(test_update_infinity_as_first_sample)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.01f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    ASSERT_NEAR(kalman_1d_update(&kf, (float)INFINITY), 0.0f, 1e-6f);
    /* Not bootstrapped yet, so the next real sample is taken as it is. */
    ASSERT_NEAR(kalman_1d_update(&kf, 5.0f), 5.0f, 1e-6f);
}

TEST(test_update_overflow_is_skipped)
{
    kalman_1d_t kf;
    kalman_1d_cfg_t cfg = {0.01f, 1.0f};
    kalman_1d_init(&kf, &cfg);

    kalman_1d_update(&kf, 3.0e38f);

    /* z - x = -6e38 does not fit in a float, though z and x both do. */
    ASSERT_TRUE(kalman_1d_update(&kf, -3.0e38f) == 3.0e38f);

    /* A sample the filter can use still moves it. */
    float next = kalman_1d_update(&kf, 2.0e38f);
    ASSERT_TRUE(next < 3.0e38f && next > 2.0e38f);
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

TEST_MAIN("kalman_1d")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_null_cfg);
    RUN(test_init_q_zero);
    RUN(test_init_r_zero);
    RUN(test_init_q_negative);
    RUN(test_init_q_nan);
    RUN(test_init_r_nan);
    RUN(test_bootstrap);
    RUN(test_convergence);
    RUN(test_noise_rejection);
    RUN(test_step_response);
    RUN(test_gain_decreases);
    RUN(test_high_q_fast_response);
    RUN(test_high_r_slow_response);
    RUN(test_reset);
    RUN(test_set_noise);
    RUN(test_set_noise_invalid);
    RUN(test_set_noise_nan);
    RUN(test_get_null);
    RUN(test_get_gain_null);
    RUN(test_update_null);
    RUN(test_update_nan);
    RUN(test_update_infinity_is_skipped);
    RUN(test_update_infinity_as_first_sample);
    RUN(test_update_overflow_is_skipped);
TEST_MAIN_END()
