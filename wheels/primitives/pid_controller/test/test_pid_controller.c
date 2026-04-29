#include <stdio.h>
#include <math.h>
#include "../pid_controller.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a) - (b)) < (tol), msg)

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

static void test_init(void)
{
    printf("%s\n", __func__);
    setup_default();
    ASSERT_NEAR(pid.integral, 0.0f, 1e-6f, "integral zero after init");
    ASSERT_NEAR(pid.prev_error, 0.0f, 1e-6f, "prev_error zero after init");
    ASSERT(pid.first_run == 1, "first_run set");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    pid_controller_cfg_t cfg = {0};
    ASSERT_EQ(pid_controller_init(NULL, &cfg), PID_CONTROLLER_ERR_PARAM, "init null ctx");
    ASSERT_EQ(pid_controller_init(&pid, NULL), PID_CONTROLLER_ERR_PARAM, "init null cfg");
    ASSERT_EQ(pid_controller_reset(NULL), PID_CONTROLLER_ERR_PARAM, "reset null");
    ASSERT_NEAR(pid_controller_update(NULL, 0, 0, 0.01f), 0.0f, 1e-6f, "update null");
    ASSERT_EQ(pid_controller_set_tunings(NULL, 0, 0, 0), PID_CONTROLLER_ERR_PARAM, "tunings null");
    ASSERT_EQ(pid_controller_set_output_limits(NULL, 0, 0), PID_CONTROLLER_ERR_PARAM, "limits null");
}

static void test_p_only(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 10.0f, 1e-4f, "P-only: error=10 kp=1 => output=10");
}

static void test_p_only_negative_error(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 0.0f, 10.0f, 0.01f);
    ASSERT_NEAR(out, -10.0f, 1e-4f, "P-only: negative error");
}

static void test_output_clamping(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 100.0f, 1e-4f, "output clamped to max");

    pid_controller_reset(&pid);
    out = pid_controller_update(&pid, -200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, -100.0f, 1e-4f, "output clamped to min");
}

static void test_integral_accumulation(void)
{
    printf("%s\n", __func__);
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 1.0f, .kd = 0.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    float out = pid_controller_update(&pid, 10.0f, 0.0f, 1.0f);
    ASSERT_NEAR(out, 10.0f, 1e-4f, "I: first step integral=10");

    out = pid_controller_update(&pid, 10.0f, 0.0f, 1.0f);
    ASSERT_NEAR(out, 20.0f, 1e-4f, "I: second step integral=20");
}

static void test_integral_clamping(void)
{
    printf("%s\n", __func__);
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
    ASSERT_NEAR(pid.integral, 5.0f, 1e-4f, "integral clamped at max");
}

static void test_derivative_first_step_zero(void)
{
    printf("%s\n", __func__);
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 0.0f, .kd = 1.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-4f, "D: first step derivative is zero");
}

static void test_derivative_step_change(void)
{
    printf("%s\n", __func__);
    pid_controller_cfg_t cfg = {
        .kp = 0.0f, .ki = 0.0f, .kd = 1.0f,
        .output_min = -10000.0f, .output_max = 10000.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    pid_controller_update(&pid, 0.0f, 0.0f, 0.01f);
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 1000.0f, 1e-2f, "D: step change 10/0.01=1000");
}

static void test_pid_combined(void)
{
    printf("%s\n", __func__);
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
    ASSERT_NEAR(out, expected, 1e-3f, "PID combined first step");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup_default();
    pid_controller_update(&pid, 50.0f, 0.0f, 0.1f);
    pid_controller_reset(&pid);
    ASSERT_NEAR(pid.integral, 0.0f, 1e-6f, "integral cleared on reset");
    ASSERT_NEAR(pid.prev_error, 0.0f, 1e-6f, "prev_error cleared on reset");
    ASSERT(pid.first_run == 1, "first_run reset");
}

static void test_set_tunings(void)
{
    printf("%s\n", __func__);
    setup_default();
    pid_controller_set_tunings(&pid, 3.0f, 0.5f, 0.2f);
    ASSERT_NEAR(pid.cfg.kp, 3.0f, 1e-6f, "kp updated");
    ASSERT_NEAR(pid.cfg.ki, 0.5f, 1e-6f, "ki updated");
    ASSERT_NEAR(pid.cfg.kd, 0.2f, 1e-6f, "kd updated");
}

static void test_set_output_limits(void)
{
    printf("%s\n", __func__);
    setup_default();
    pid_controller_set_output_limits(&pid, -50.0f, 50.0f);
    float out = pid_controller_update(&pid, 200.0f, 0.0f, 0.01f);
    ASSERT_NEAR(out, 50.0f, 1e-4f, "new output max applied");
}

static void test_zero_dt_returns_zero(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, 0.0f);
    ASSERT_NEAR(out, 0.0f, 1e-6f, "dt=0 returns 0");
}

static void test_negative_dt_returns_zero(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 10.0f, 0.0f, -0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-6f, "dt<0 returns 0");
}

static void test_convergence_simulation(void)
{
    printf("%s\n", __func__);
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
    ASSERT_NEAR(measured, setpoint, 2.0f, "converges toward setpoint");
}

static void test_zero_error_zero_output(void)
{
    printf("%s\n", __func__);
    setup_default();
    float out = pid_controller_update(&pid, 5.0f, 5.0f, 0.01f);
    ASSERT_NEAR(out, 0.0f, 1e-6f, "zero error => zero output");
}

static void test_integral_negative(void)
{
    printf("%s\n", __func__);
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
    ASSERT_NEAR(pid.integral, -50.0f, 1e-4f, "negative integral clamped at min");
}

int main(void)
{
    printf("test_pid_controller\n");
    test_init();
    test_null_safety();
    test_p_only();
    test_p_only_negative_error();
    test_output_clamping();
    test_integral_accumulation();
    test_integral_clamping();
    test_derivative_first_step_zero();
    test_derivative_step_change();
    test_pid_combined();
    test_reset();
    test_set_tunings();
    test_set_output_limits();
    test_zero_dt_returns_zero();
    test_negative_dt_returns_zero();
    test_convergence_simulation();
    test_zero_error_zero_output();
    test_integral_negative();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
