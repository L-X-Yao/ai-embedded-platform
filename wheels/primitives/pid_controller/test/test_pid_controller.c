// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <float.h>
#include <math.h>
#include "pid_controller.h"
static pid_controller_t pid;

static void setup_default(void)
{
    pid_controller_cfg_t cfg = {
        .kp = 1.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_min = -100.0f,
        .output_max = 100.0f,
        .integral_min = -50.0f,
        .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);
}

TEST(test_init)
{
    setup_default();
    ASSERT_NEAR(pid.integral, 0.0f, 1e-6f);
    ASSERT_NEAR(pid.prev_error, 0.0f, 1e-6f);
    ASSERT_TRUE(pid.first_run == 1);
}

TEST(test_null_safety)
{
    pid_controller_cfg_t cfg = {0};
    ASSERT_EQ(pid_controller_init(NULL, &cfg), PID_CONTROLLER_ERR_PARAM);
    ASSERT_EQ(pid_controller_init(&pid, NULL), PID_CONTROLLER_ERR_PARAM);
    ASSERT_EQ(pid_controller_reset(NULL), PID_CONTROLLER_ERR_PARAM);
    ASSERT_NEAR(pid_controller_update(NULL, 0, 0, 0.01f), 0.0f, 1e-6f);
    ASSERT_EQ(pid_controller_set_tunings(NULL, 0, 0, 0), PID_CONTROLLER_ERR_PARAM);
    ASSERT_EQ(pid_controller_set_output_limits(NULL, 0, 0), PID_CONTROLLER_ERR_PARAM);
}

TEST(test_p_only)
{
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 10.0f, 1e-4f);
}

TEST(test_p_only_negative_error)
{
    setup_default();
    float out = pid_controller_update(&pid, 0.0f, 10.0f, 0.01f);
    ASSERT_NEAR(out, -10.0f, 1e-4f);
}

TEST(test_output_clamping)
{
    setup_default();
    float out = pid_controller_update(&pid, 200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 100.0f, 1e-4f);

    pid_controller_reset(&pid);
    out = pid_controller_update(&pid, -200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, -100.0f, 1e-4f);
}

TEST(test_integral_accumulation)
{
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 1.0f, .kd = 0.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    float out = pid_controller_update(&pid, 10.0f, 0.0f, 1.0f);
    ASSERT_NEAR(out, 10.0f, 1e-4f);

    out = pid_controller_update(&pid, 10.0f, 0.0f, 1.0f);
    ASSERT_NEAR(out, 20.0f, 1e-4f);
}

TEST(test_integral_clamping)
{
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 1.0f, .kd = 0.0f,
        .output_min = -1000.0f, .output_max = 1000.0f,
        .integral_min = -5.0f, .integral_max = 5.0f,
    };
    pid_controller_init(&pid, &cfg);

    for (int i = 0; i < 100; i++)
    {
        pid_controller_update(&pid, 10.0f, 0.0f, 1.0f);
    }
    ASSERT_NEAR(pid.integral, 5.0f, 1e-4f);
}

TEST(test_derivative_first_step_zero)
{
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 0.0f, .kd = 1.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-4f);
}

TEST(test_derivative_step_change)
{
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 0.0f, .kd = 1.0f,
        .output_min = -10000.0f, .output_max = 10000.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    pid_controller_update(&pid, 0.0f, 0.0f, 0.01f);
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 1000.0f, 1e-2f);
}

TEST(test_pid_combined)
{
    pid_controller_cfg_t cfg = {
        .kp = 2.0f, .ki = 0.5f, .kd = 0.1f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.1f);
    float expected_p = 2.0f * 10.0f;
    float expected_i = 0.5f * (10.0f * 0.1f);
    float expected = expected_p + expected_i;
    ASSERT_NEAR(out, expected, 1e-3f);
}

TEST(test_reset)
{
    setup_default();
    pid_controller_update(&pid, 50.0f, 0.0f, 0.1f);
    pid_controller_reset(&pid);
    ASSERT_NEAR(pid.integral, 0.0f, 1e-6f);
    ASSERT_NEAR(pid.prev_error, 0.0f, 1e-6f);
    ASSERT_TRUE(pid.first_run == 1);
}

TEST(test_set_tunings)
{
    setup_default();
    pid_controller_set_tunings(&pid, 3.0f, 0.5f, 0.2f);
    ASSERT_NEAR(pid.cfg.kp, 3.0f, 1e-6f);
    ASSERT_NEAR(pid.cfg.ki, 0.5f, 1e-6f);
    ASSERT_NEAR(pid.cfg.kd, 0.2f, 1e-6f);
}

TEST(test_set_output_limits)
{
    setup_default();
    pid_controller_set_output_limits(&pid, -50.0f, 50.0f);
    float out = pid_controller_update(&pid, 200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 50.0f, 1e-4f);
}

TEST(test_zero_dt_returns_zero)
{
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.0f);
    ASSERT_NEAR(out, 0.0f, 1e-6f);
}

TEST(test_negative_dt_returns_zero)
{
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, -0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-6f);
}

TEST(test_convergence_simulation)
{
    pid_controller_cfg_t cfg = {
        .kp = 1.0f, .ki = 0.1f, .kd = 0.05f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -100.0f, .integral_max = 100.0f,
    };
    pid_controller_init(&pid, &cfg);

    float measured = 0.0f;
    float setpoint = 50.0f;
    float dt = 0.01f;

    for (int i = 0; i < 5000; i++)
    {
        float out = pid_controller_update(&pid, setpoint, measured, dt);
        measured += out * 0.1f * dt;
    }
    ASSERT_NEAR(measured, setpoint, 2.0f);
}

TEST(test_zero_error_zero_output)
{
    setup_default();
    float out = pid_controller_update(&pid, 5.0f, 5.0f, 0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-6f);
}

TEST(test_integral_negative)
{
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 1.0f, .kd = 0.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    for (int i = 0; i < 100; i++)
    {
        pid_controller_update(&pid, -10.0f, 0.0f, 1.0f);
    }
    ASSERT_NEAR(pid.integral, -50.0f, 1e-4f);
}

/* PI as in the audit: no D term, so 0 * inf in it is the trap. */
static const pid_controller_cfg_t pi_cfg = {
    .kp = 1.0f,
    .ki = 0.5f,
    .kd = 0.0f,
    .output_min = -10.0f,
    .output_max = 10.0f,
    .integral_min = -50.0f,
    .integral_max = 50.0f,
};

/* A refused input returns the documented 0.0f and leaves the state alone. */
TEST(test_non_finite_inputs_refused)
{
    const float bad[] = { NAN, INFINITY, -INFINITY };

    for (int b = 0; b < 3; b++) {
        pid_controller_init(&pid, &pi_cfg);
        pid_controller_update(&pid, 5.0f, 0.0f, 0.1f);
        float integral = pid.integral;
        float prev_error = pid.prev_error;

        ASSERT_NEAR(pid_controller_update(&pid, bad[b], 0.0f, 0.1f), 0.0f, 0.0f);
        ASSERT_NEAR(pid_controller_update(&pid, 5.0f, bad[b], 0.1f), 0.0f, 0.0f);
        ASSERT_NEAR(pid_controller_update(&pid, 5.0f, 0.0f, bad[b]), 0.0f, 0.0f);
        ASSERT_NEAR(pid.integral, integral, 0.0f);
        ASSERT_NEAR(pid.prev_error, prev_error, 0.0f);
    }
}

/* Later outputs are the same as if the bad sample had never come. */
TEST(test_infinite_measured_leaves_no_trace)
{
    pid_controller_t with, without;
    pid_controller_init(&with, &pi_cfg);
    pid_controller_init(&without, &pi_cfg);

    pid_controller_update(&with, 1.0f, 0.0f, 0.01f);
    pid_controller_update(&without, 1.0f, 0.0f, 0.01f);

    pid_controller_update(&with, 1.0f, INFINITY, 0.01f);

    for (int i = 0; i < 3; i++) {
        float a = pid_controller_update(&with, 1.0f, 2.5f, 0.01f);
        float b = pid_controller_update(&without, 1.0f, 2.5f, 0.01f);
        ASSERT_NEAR(a, b, 0.0f);
    }
}

TEST(test_infinite_dt_does_not_spoil_integral)
{
    pid_controller_init(&pid, &pi_cfg);

    pid_controller_update(&pid, 1.0f, 1.0f, INFINITY);
    ASSERT_NEAR(pid.integral, 0.0f, 0.0f);
    /* error 1 for 1 s: 1 * 1 + 0.5 * 1 */
    ASSERT_NEAR(pid_controller_update(&pid, 1.0f, 0.0f, 1.0f), 1.5f, 1e-5f);
}

TEST(test_set_tunings_refuses_non_finite)
{
    const float bad[] = { NAN, INFINITY, -INFINITY };

    for (int b = 0; b < 3; b++) {
        pid_controller_init(&pid, &pi_cfg);
        ASSERT_EQ(pid_controller_set_tunings(&pid, bad[b], 0, 0), PID_CONTROLLER_ERR_PARAM);
        ASSERT_EQ(pid_controller_set_tunings(&pid, 0, bad[b], 0), PID_CONTROLLER_ERR_PARAM);
        ASSERT_EQ(pid_controller_set_tunings(&pid, 0, 0, bad[b]), PID_CONTROLLER_ERR_PARAM);
        ASSERT_NEAR(pid.cfg.kp, 1.0f, 0.0f);
        ASSERT_NEAR(pid.cfg.ki, 0.5f, 0.0f);
        ASSERT_NEAR(pid.cfg.kd, 0.0f, 0.0f);
    }
}

TEST(test_set_output_limits_refuses_nan)
{
    pid_controller_init(&pid, &pi_cfg);

    ASSERT_EQ(pid_controller_set_output_limits(&pid, NAN, NAN), PID_CONTROLLER_ERR_PARAM);
    ASSERT_EQ(pid_controller_set_output_limits(&pid, NAN, 1.0f), PID_CONTROLLER_ERR_PARAM);
    ASSERT_EQ(pid_controller_set_output_limits(&pid, 1.0f, NAN), PID_CONTROLLER_ERR_PARAM);

    /* the old +-10 still holds */
    ASSERT_NEAR(pid_controller_update(&pid, 1e6f, 0.0f, 0.01f), 10.0f, 0.0f);
}

TEST(test_init_refuses_nan)
{
    pid_controller_t c;

    for (int i = 0; i < 7; i++) {
        pid_controller_cfg_t cfg = pi_cfg;
        float *fields[7] = {
            &cfg.kp, &cfg.ki, &cfg.kd, &cfg.output_min, &cfg.output_max,
            &cfg.integral_min, &cfg.integral_max,
        };
        *fields[i] = NAN;
        ASSERT_EQ(pid_controller_init(&c, &cfg), PID_CONTROLLER_ERR_PARAM);
    }
}

TEST(test_init_refuses_infinite_gain)
{
    pid_controller_t c;
    pid_controller_cfg_t cfg = pi_cfg;

    cfg.kp = INFINITY;
    ASSERT_EQ(pid_controller_init(&c, &cfg), PID_CONTROLLER_ERR_PARAM);
    cfg = pi_cfg;
    cfg.ki = -INFINITY;
    ASSERT_EQ(pid_controller_init(&c, &cfg), PID_CONTROLLER_ERR_PARAM);
    cfg = pi_cfg;
    cfg.kd = INFINITY;
    ASSERT_EQ(pid_controller_init(&c, &cfg), PID_CONTROLLER_ERR_PARAM);
}

/* Infinite limits mean "no limit": that stays valid. */
TEST(test_infinite_limits_still_accepted)
{
    pid_controller_cfg_t cfg = pi_cfg;
    cfg.ki = 0.0f;
    cfg.output_min = -INFINITY;
    cfg.output_max = INFINITY;
    cfg.integral_min = -INFINITY;
    cfg.integral_max = INFINITY;

    ASSERT_EQ(pid_controller_init(&pid, &cfg), PID_CONTROLLER_OK);
    ASSERT_NEAR(pid_controller_update(&pid, 1e6f, 0.0f, 0.01f), 1e6f, 1.0f);
    ASSERT_EQ(pid_controller_set_output_limits(&pid, -INFINITY, INFINITY), PID_CONTROLLER_OK);
}

/* Finite inputs can still make NaN: error - prev_error overflows, and kd = 0
 * turns that into 0 * inf.  The output falls back to 0 inside the limits. */
TEST(test_nan_from_finite_inputs_stays_in_limits)
{
    pid_controller_init(&pid, &pi_cfg);
    pid_controller_update(&pid, 3e38f, 0.0f, 1.0f);
    ASSERT_NEAR(pid_controller_update(&pid, -3e38f, 0.0f, 1.0f), 0.0f, 0.0f);

    /* 0 is outside these limits, so the nearest limit stands in for it */
    pid_controller_cfg_t cfg = pi_cfg;
    cfg.output_min = 10.0f;
    cfg.output_max = 20.0f;
    pid_controller_init(&pid, &cfg);
    pid_controller_update(&pid, 3e38f, 0.0f, 1.0f);
    ASSERT_NEAR(pid_controller_update(&pid, -3e38f, 0.0f, 1.0f), 10.0f, 0.0f);
}

/* The difference of two finite inputs can overflow: same as a bad input. */
TEST(test_overflowing_error_refused)
{
    pid_controller_init(&pid, &pi_cfg);
    ASSERT_NEAR(pid_controller_update(&pid, FLT_MAX, -FLT_MAX, 0.1f), 0.0f, 0.0f);
    ASSERT_NEAR(pid.integral, 0.0f, 0.0f);
    ASSERT_TRUE(pid.first_run);
}

/* With no integral limit an overflowing step is dropped: an infinite integral
 * would hold the output at a limit for good. */
TEST(test_integral_stays_finite_without_limits)
{
    pid_controller_cfg_t cfg = pi_cfg;
    cfg.integral_min = -INFINITY;
    cfg.integral_max = INFINITY;
    pid_controller_init(&pid, &cfg);

    pid_controller_update(&pid, 1.0f, 0.0f, 1.0f);
    pid_controller_update(&pid, 1e30f, 0.0f, 1e30f);
    ASSERT_NEAR(pid.integral, 1.0f, 0.0f);
}

/* NaN written into the state must not stay there. */
TEST(test_nan_in_state_recovers)
{
    pid_controller_init(&pid, &pi_cfg);
    pid.integral = NAN;
    pid_controller_update(&pid, 1.0f, 0.0f, 1.0f);
    ASSERT_NEAR(pid.integral, 0.0f, 0.0f);

    /* error 1 for 1 s more: 1 * 1 + 0.5 * 1 */
    ASSERT_NEAR(pid_controller_update(&pid, 1.0f, 0.0f, 1.0f), 1.5f, 1e-5f);
}

TEST_MAIN("pid_controller")
    RUN(test_non_finite_inputs_refused);
    RUN(test_infinite_measured_leaves_no_trace);
    RUN(test_infinite_dt_does_not_spoil_integral);
    RUN(test_set_tunings_refuses_non_finite);
    RUN(test_set_output_limits_refuses_nan);
    RUN(test_init_refuses_nan);
    RUN(test_init_refuses_infinite_gain);
    RUN(test_infinite_limits_still_accepted);
    RUN(test_nan_from_finite_inputs_stays_in_limits);
    RUN(test_overflowing_error_refused);
    RUN(test_integral_stays_finite_without_limits);
    RUN(test_nan_in_state_recovers);
    RUN(test_init);
    RUN(test_null_safety);
    RUN(test_p_only);
    RUN(test_p_only_negative_error);
    RUN(test_output_clamping);
    RUN(test_integral_accumulation);
    RUN(test_integral_clamping);
    RUN(test_derivative_first_step_zero);
    RUN(test_derivative_step_change);
    RUN(test_pid_combined);
    RUN(test_reset);
    RUN(test_set_tunings);
    RUN(test_set_output_limits);
    RUN(test_zero_dt_returns_zero);
    RUN(test_negative_dt_returns_zero);
    RUN(test_convergence_simulation);
    RUN(test_zero_error_zero_output);
    RUN(test_integral_negative);
TEST_MAIN_END()

