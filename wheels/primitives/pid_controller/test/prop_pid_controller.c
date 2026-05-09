/*
 * prop_pid_controller.c — property-based tests for pid_controller
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_pid_controller.c ../pid_controller.c -o prop_pid_controller -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../pid_controller.h"
#include <math.h>

/* Property: output is always within [output_min, output_max] */
PROPERTY(output_bounded, 10000) {
    float kp = (float)(ARBI_RANGE(seed, 1, 100)) / 10.0f;
    float ki = (float)(ARBI_RANGE(seed, 0, 50))  / 100.0f;
    float kd = (float)(ARBI_RANGE(seed, 0, 50))  / 100.0f;

    float out_min = -100.0f;
    float out_max = 100.0f;

    pid_controller_cfg_t cfg = {
        .kp = kp, .ki = ki, .kd = kd,
        .output_min = out_min, .output_max = out_max,
        .integral_min = -1000.0f, .integral_max = 1000.0f
    };

    pid_controller_t ctx;
    pid_controller_init(&ctx, &cfg);

    float setpoint = (float)(ARBI_RANGE(seed, -500, 500));
    float measured = (float)(ARBI_RANGE(seed, -500, 500));
    float dt = (float)(ARBI_RANGE(seed, 1, 100)) / 100.0f;

    float output = pid_controller_update(&ctx, setpoint, measured, dt);

    PROP_ASSERT(output >= out_min - 0.001f);
    PROP_ASSERT(output <= out_max + 0.001f);
}

/* Property: P-only controller — output proportional to error */
PROPERTY(p_only_proportional, 10000) {
    float kp = (float)(ARBI_RANGE(seed, 1, 100)) / 10.0f;

    pid_controller_cfg_t cfg = {
        .kp = kp, .ki = 0.0f, .kd = 0.0f,
        .output_min = -10000.0f, .output_max = 10000.0f,
        .integral_min = -1000.0f, .integral_max = 1000.0f
    };

    pid_controller_t ctx;
    pid_controller_init(&ctx, &cfg);

    float setpoint = (float)(ARBI_RANGE(seed, -100, 100));
    float measured = (float)(ARBI_RANGE(seed, -100, 100));

    float output = pid_controller_update(&ctx, setpoint, measured, 0.01f);
    float expected = kp * (setpoint - measured);

    /* Clamp expected to limits */
    if (expected > cfg.output_max) expected = cfg.output_max;
    if (expected < cfg.output_min) expected = cfg.output_min;

    PROP_ASSERT(fabsf(output - expected) < 0.01f);
}

/* Property: zero error produces near-zero output (P-only, fresh state) */
PROPERTY(zero_error_zero_output, 5000) {
    float kp = (float)(ARBI_RANGE(seed, 1, 100)) / 10.0f;

    pid_controller_cfg_t cfg = {
        .kp = kp, .ki = 0.0f, .kd = 0.0f,
        .output_min = -1000.0f, .output_max = 1000.0f,
        .integral_min = -1000.0f, .integral_max = 1000.0f
    };

    pid_controller_t ctx;
    pid_controller_init(&ctx, &cfg);

    float val = (float)(ARBI_RANGE(seed, -100, 100));
    float output = pid_controller_update(&ctx, val, val, 0.01f);

    PROP_ASSERT(fabsf(output) < 0.01f);
}

/* Property: reset clears internal state */
PROPERTY(reset_clears_state, 5000) {
    pid_controller_cfg_t cfg = {
        .kp = 1.0f, .ki = 0.5f, .kd = 0.1f,
        .output_min = -100.0f, .output_max = 100.0f,
        .integral_min = -100.0f, .integral_max = 100.0f
    };

    pid_controller_t ctx;
    pid_controller_init(&ctx, &cfg);

    /* Run some iterations to build integral */
    for (int i = 0; i < 10; i++) {
        pid_controller_update(&ctx, 50.0f, 0.0f, 0.01f);
    }

    pid_controller_reset(&ctx);

    /* After reset, P-only response should dominate — same as fresh */
    float out1 = pid_controller_update(&ctx, 10.0f, 0.0f, 0.01f);

    pid_controller_t fresh;
    pid_controller_init(&fresh, &cfg);
    float out2 = pid_controller_update(&fresh, 10.0f, 0.0f, 0.01f);

    PROP_ASSERT(fabsf(out1 - out2) < 0.01f);
}

TEST_MAIN("pid_controller properties") {
    RUN(output_bounded);
    RUN(p_only_proportional);
    RUN(zero_error_zero_output);
    RUN(reset_clears_state);
}
TEST_MAIN_END()
