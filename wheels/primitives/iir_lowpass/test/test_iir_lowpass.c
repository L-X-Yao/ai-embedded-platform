// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "iir_lowpass.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int near(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= eps;
}

/* ------------------------------------------------------------------ */
/* init tests                                                          */
/* ------------------------------------------------------------------ */

TEST(test_init_ok)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.5f};
    ASSERT_EQ(iir_lowpass_init(&f, &cfg), IIR_LOWPASS_OK);
    ASSERT_TRUE(near(f.output, 0.0f, 1e-9f));
    ASSERT_EQ(f.primed, 0);
}

TEST(test_init_null)
{
    iir_lowpass_cfg_t cfg = {0.5f};
    ASSERT_EQ(iir_lowpass_init(NULL, &cfg), IIR_LOWPASS_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    iir_lowpass_t f;
    ASSERT_EQ(iir_lowpass_init(&f, NULL), IIR_LOWPASS_ERR_PARAM);
}

TEST(test_init_alpha_zero)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.0f};
    ASSERT_EQ(iir_lowpass_init(&f, &cfg), IIR_LOWPASS_ERR_PARAM);
}

TEST(test_init_alpha_negative)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {-0.5f};
    ASSERT_EQ(iir_lowpass_init(&f, &cfg), IIR_LOWPASS_ERR_PARAM);
}

TEST(test_init_alpha_over_one)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {1.5f};
    ASSERT_EQ(iir_lowpass_init(&f, &cfg), IIR_LOWPASS_ERR_PARAM);
}

TEST(test_init_alpha_nan)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {(float)NAN};
    ASSERT_EQ(iir_lowpass_init(&f, &cfg), IIR_LOWPASS_ERR_PARAM);
}

/* ------------------------------------------------------------------ */
/* feed / behaviour tests                                              */
/* ------------------------------------------------------------------ */

TEST(test_first_sample_passthrough)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.1f};
    iir_lowpass_init(&f, &cfg);
    float out = iir_lowpass_feed(&f, 42.0f);
    ASSERT_TRUE(near(out, 42.0f, 1e-6f));
    ASSERT_EQ(f.primed, 1);
}

TEST(test_smoothing)
{
    /* Step from 0 to 100. After a few samples output must move toward 100
     * but not overshoot. */
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.3f};
    iir_lowpass_init(&f, &cfg);
    iir_lowpass_feed(&f, 0.0f);  /* prime at 0 */

    float out = iir_lowpass_feed(&f, 100.0f);
    /* first real update: 0.3*100 + 0.7*0 = 30 */
    ASSERT_TRUE(near(out, 30.0f, 1e-5f));

    float prev = out;
    out = iir_lowpass_feed(&f, 100.0f);
    /* output should still be moving toward 100 */
    ASSERT_TRUE(out > prev);
    ASSERT_TRUE(out < 100.0f);
}

TEST(test_alpha_one_passthrough)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {1.0f};
    iir_lowpass_init(&f, &cfg);
    iir_lowpass_feed(&f, 0.0f);          /* prime */
    ASSERT_TRUE(near(iir_lowpass_feed(&f, 77.5f), 77.5f, 1e-6f));
    ASSERT_TRUE(near(iir_lowpass_feed(&f, -3.0f), -3.0f, 1e-6f));
}

TEST(test_alpha_small_slow)
{
    /* With alpha=0.01 a single step barely moves the output. */
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.01f};
    iir_lowpass_init(&f, &cfg);
    iir_lowpass_feed(&f, 0.0f);  /* prime at 0 */
    float out = iir_lowpass_feed(&f, 1000.0f);
    /* 0.01 * 1000 + 0.99 * 0 = 10 */
    ASSERT_TRUE(near(out, 10.0f, 1e-4f));
}

TEST(test_convergence)
{
    /* Feed the same value 100 times — output must converge within 1e-3. */
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.1f};
    iir_lowpass_init(&f, &cfg);
    int i;
    float out = 0.0f;
    for (i = 0; i < 100; i++)
    {
        out = iir_lowpass_feed(&f, 5.0f);
    }
    ASSERT_TRUE(near(out, 5.0f, 1e-3f));
}

TEST(test_reset)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.5f};
    iir_lowpass_init(&f, &cfg);
    iir_lowpass_feed(&f, 100.0f);
    iir_lowpass_feed(&f, 200.0f);
    ASSERT_EQ(f.primed, 1);

    iir_lowpass_reset(&f);
    ASSERT_EQ(f.primed, 0);
    ASSERT_TRUE(near(f.output, 0.0f, 1e-9f));
    /* After reset, next feed should be passthrough again */
    float out = iir_lowpass_feed(&f, 7.0f);
    ASSERT_TRUE(near(out, 7.0f, 1e-6f));
}

TEST(test_set_alpha)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.5f};
    iir_lowpass_init(&f, &cfg);
    ASSERT_EQ(iir_lowpass_set_alpha(&f, 0.9f), IIR_LOWPASS_OK);
    ASSERT_TRUE(near(f.alpha, 0.9f, 1e-9f));
    /* Verify new alpha is used on next feed */
    iir_lowpass_feed(&f, 0.0f);  /* prime */
    float out = iir_lowpass_feed(&f, 10.0f);
    /* 0.9 * 10 + 0.1 * 0 = 9 */
    ASSERT_TRUE(near(out, 9.0f, 1e-5f));
}

TEST(test_set_alpha_invalid)
{
    iir_lowpass_t f;
    iir_lowpass_cfg_t cfg = {0.5f};
    iir_lowpass_init(&f, &cfg);
    ASSERT_EQ(iir_lowpass_set_alpha(&f, 0.0f),  IIR_LOWPASS_ERR_PARAM);
    ASSERT_EQ(iir_lowpass_set_alpha(&f, -1.0f), IIR_LOWPASS_ERR_PARAM);
    ASSERT_EQ(iir_lowpass_set_alpha(&f, 1.5f),  IIR_LOWPASS_ERR_PARAM);
    /* alpha unchanged after failed set */
    ASSERT_TRUE(near(f.alpha, 0.5f, 1e-9f));
}

TEST(test_get_null)
{
    ASSERT_TRUE(near(iir_lowpass_get(NULL), 0.0f, 1e-9f));
}

TEST(test_feed_null)
{
    ASSERT_TRUE(near(iir_lowpass_feed(NULL, 1.0f), 0.0f, 1e-9f));
}

TEST(test_alpha_from_rc)
{
    /* dt=0.01s, rc=0.1s → alpha = 0.01/0.11 ≈ 0.09090... */
    float alpha = iir_lowpass_alpha_from_rc(0.01f, 0.1f);
    ASSERT_TRUE(near(alpha, 0.09090909f, 1e-5f));
}

TEST(test_alpha_from_rc_invalid)
{
    ASSERT_TRUE(near(iir_lowpass_alpha_from_rc(0.0f, 0.1f),  0.0f, 1e-9f));
    ASSERT_TRUE(near(iir_lowpass_alpha_from_rc(0.01f, 0.0f), 0.0f, 1e-9f));
    ASSERT_TRUE(near(iir_lowpass_alpha_from_rc(-1.0f, 0.1f), 0.0f, 1e-9f));
    ASSERT_TRUE(near(iir_lowpass_alpha_from_rc(0.01f, -1.0f),0.0f, 1e-9f));
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

TEST_MAIN("iir_lowpass")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_null_cfg);
    RUN(test_init_alpha_zero);
    RUN(test_init_alpha_negative);
    RUN(test_init_alpha_over_one);
    RUN(test_init_alpha_nan);
    RUN(test_first_sample_passthrough);
    RUN(test_smoothing);
    RUN(test_alpha_one_passthrough);
    RUN(test_alpha_small_slow);
    RUN(test_convergence);
    RUN(test_reset);
    RUN(test_set_alpha);
    RUN(test_set_alpha_invalid);
    RUN(test_get_null);
    RUN(test_feed_null);
    RUN(test_alpha_from_rc);
    RUN(test_alpha_from_rc_invalid);
TEST_MAIN_END()
