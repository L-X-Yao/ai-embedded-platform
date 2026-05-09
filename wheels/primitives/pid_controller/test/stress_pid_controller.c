// SPDX-License-Identifier: MIT
/*
 * stress_pid_controller.c — long-running stress tests for pid_controller
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_pid_controller.c ../pid_controller.c -o /tmp/stress_pid_controller -lm
 *
 * Scenarios:
 *   1. Integral windup clamping — sustained error for 100K steps, verify bounded
 *   2. Output saturation under step response — verify output stays in limits
 *   3. Oscillation stability — setpoint flip-flop for 100K steps, output bounded
 *   4. Accumulation drift — tiny dt over 1M steps, verify no float drift to NaN/Inf
 */

#include "test_harness.h"
#include "pid_controller.h"
#include <math.h>

/* ---- tests ---------------------------------------------------------- */

TEST(integral_windup_clamping)
{
    pid_controller_t pid;
    pid_controller_cfg_t cfg = {
        .kp = 1.0f, .ki = 10.0f, .kd = 0.0f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -50.0f, .integral_max = 50.0f,
    };
    pid_controller_init(&pid, &cfg);

    /* Sustained max error for 100K steps — integral must clamp at 50 */
    float output = 0.0f;
    for (int i = 0; i < 100000; i++) {
        output = pid_controller_update(&pid, 1000.0f, 0.0f, 0.01f);
    }
    /* Output = kp*error + ki*integral_clamped + kd*deriv
     * integral should be clamped at 50, so ki*integral = 10*50 = 500, but output_max=100 */
    ASSERT_TRUE(output <= 100.0f);
    ASSERT_TRUE(output >= -100.0f);
    /* integral itself must be clamped */
    ASSERT_TRUE(pid.integral <= 50.0f);
    ASSERT_TRUE(pid.integral >= -50.0f);
}

TEST(output_saturation_step_response)
{
    pid_controller_t pid;
    pid_controller_cfg_t cfg = {
        .kp = 5.0f, .ki = 2.0f, .kd = 1.0f,
        .output_min = -10.0f, .output_max = 10.0f,
        .integral_min = -100.0f, .integral_max = 100.0f,
    };
    pid_controller_init(&pid, &cfg);

    int ok = 1;
    float measured = 0.0f;
    for (int i = 0; i < 100000; i++) {
        float output = pid_controller_update(&pid, 100.0f, measured, 0.01f);
        if (!(output >= -10.0f && output <= 10.0f)) { ok = 0; break; }
        /* Simulate plant: measured moves toward setpoint slowly */
        measured += output * 0.001f;
    }
    ASSERT_TRUE(ok);
}

TEST(oscillation_stability)
{
    pid_controller_t pid;
    pid_controller_cfg_t cfg = {
        .kp = 2.0f, .ki = 0.5f, .kd = 0.1f,
        .output_min = -50.0f, .output_max = 50.0f,
        .integral_min = -200.0f, .integral_max = 200.0f,
    };
    pid_controller_init(&pid, &cfg);

    int ok = 1;
    float measured = 0.0f;
    for (int i = 0; i < 100000; i++) {
        /* Flip setpoint every 100 steps */
        float setpoint = (i / 100) % 2 == 0 ? 10.0f : -10.0f;
        float output = pid_controller_update(&pid, setpoint, measured, 0.01f);

        /* Output must never exceed limits */
        if (!(output >= -50.0f && output <= 50.0f)) { ok = 0; break; }

        /* Check for NaN/Inf */
        if (isnan(output) || isinf(output)) { ok = 0; break; }

        measured += output * 0.01f;
        /* clamp measured to prevent runaway in sim */
        if (measured > 1000.0f) measured = 1000.0f;
        if (measured < -1000.0f) measured = -1000.0f;
    }
    ASSERT_TRUE(ok);
    /* integral must still be finite and clamped */
    ASSERT_TRUE(!isnan(pid.integral));
    ASSERT_TRUE(!isinf(pid.integral));
}

TEST(tiny_dt_no_drift_to_nan)
{
    pid_controller_t pid;
    pid_controller_cfg_t cfg = {
        .kp = 1.0f, .ki = 1.0f, .kd = 1.0f,
        .output_min = -1000.0f, .output_max = 1000.0f,
        .integral_min = -1000.0f, .integral_max = 1000.0f,
    };
    pid_controller_init(&pid, &cfg);

    int ok = 1;
    for (int i = 0; i < 1000000; i++) {
        float setpoint = (float)(i % 100);
        float measured = (float)((i + 17) % 100);
        float output = pid_controller_update(&pid, setpoint, measured, 0.0001f);
        if (isnan(output) || isinf(output)) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_TRUE(!isnan(pid.integral));
    ASSERT_TRUE(!isinf(pid.integral));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress pid_controller")
    RUN(integral_windup_clamping);
    RUN(output_saturation_step_response);
    RUN(oscillation_stability);
    RUN(tiny_dt_no_drift_to_nan);
TEST_MAIN_END()
