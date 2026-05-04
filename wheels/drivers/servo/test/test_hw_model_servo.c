// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_servo.h"

static const servo_cfg_t standard_cfg = {
    .min_pulse_us = 500,
    .max_pulse_us = 2500,
    .max_angle    = 180,
};

/* ---------- tests ---------- */

TEST(test_angle_0_gives_min_pulse)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    ASSERT_EQ(servo_set_angle(&dev, 0), SERVO_OK);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 500);
    ASSERT_EQ(servo_model_get_angle(&model, 500, 2500, 180), 0);
}

TEST(test_angle_90_gives_center)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    ASSERT_EQ(servo_set_angle(&dev, 90), SERVO_OK);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 1500);
    ASSERT_EQ(servo_model_get_angle(&model, 500, 2500, 180), 90);
}

TEST(test_angle_180_gives_max_pulse)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    ASSERT_EQ(servo_set_angle(&dev, 180), SERVO_OK);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 2500);
    ASSERT_EQ(servo_model_get_angle(&model, 500, 2500, 180), 180);
}

TEST(test_out_of_range_clamped)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    /* angle > max_angle should be clamped to max */
    ASSERT_EQ(servo_set_angle(&dev, 999), SERVO_OK);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 2500);
    ASSERT_EQ(servo_get_angle(&dev), 180);
}

TEST(test_pulse_proportional_to_angle)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    /* Check several angles for proportional mapping */
    uint16_t angles[] = {0, 30, 45, 60, 90, 120, 135, 150, 180};
    for (int i = 0; i < 9; i++) {
        servo_set_angle(&dev, angles[i]);
        uint32_t expected = servo_model_expected_pulse(
            angles[i], 500, 2500, 180);
        ASSERT_EQ(servo_model_get_pulse_us(&model), expected);
    }
}

TEST(test_pulse_history)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    servo_set_angle(&dev, 0);
    servo_set_angle(&dev, 90);
    servo_set_angle(&dev, 180);
    ASSERT_EQ(servo_model_get_history_count(&model), 3);
    ASSERT_EQ(servo_model_get_pulse_at(&model, 0), 500);
    ASSERT_EQ(servo_model_get_pulse_at(&model, 1), 1500);
    ASSERT_EQ(servo_model_get_pulse_at(&model, 2), 2500);
}

TEST(test_direct_pulse_passthrough)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_t dev;
    servo_init(&dev, &pwm, &standard_cfg);

    ASSERT_EQ(servo_set_pulse(&dev, 1234), SERVO_OK);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 1234);
}

TEST(test_custom_config_270_degrees)
{
    servo_model_t model;
    servo_model_init(&model);
    servo_pwm_t pwm = servo_model_pwm(&model);
    servo_cfg_t cfg270 = {
        .min_pulse_us = 500,
        .max_pulse_us = 2500,
        .max_angle    = 270,
    };
    servo_t dev;
    servo_init(&dev, &pwm, &cfg270);

    /* 135 degrees = halfway on a 270-degree servo */
    servo_set_angle(&dev, 135);
    ASSERT_EQ(servo_model_get_pulse_us(&model), 1500);
    ASSERT_EQ(servo_model_get_angle(&model, 500, 2500, 270), 135);
}

TEST_MAIN("servo_hw_model")
    RUN(test_angle_0_gives_min_pulse);
    RUN(test_angle_90_gives_center);
    RUN(test_angle_180_gives_max_pulse);
    RUN(test_out_of_range_clamped);
    RUN(test_pulse_proportional_to_angle);
    RUN(test_pulse_history);
    RUN(test_direct_pulse_passthrough);
    RUN(test_custom_config_270_degrees);
TEST_MAIN_END()
