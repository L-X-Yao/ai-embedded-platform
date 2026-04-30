#include "test_harness.h"
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

TEST_MAIN("pid_controller")
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

