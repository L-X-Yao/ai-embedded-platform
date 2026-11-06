// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "servo.h"

static uint32_t mock_last_pulse;
static bool mock_fail;

static bool mock_set_pulse(void *ctx, uint32_t pulse_us)
{
    (void)ctx;
    if (mock_fail) return false;
    mock_last_pulse = pulse_us;
    return true;
}

static servo_pwm_t mock_pwm = {
    .set_pulse_us = mock_set_pulse,
    .ctx = NULL
};

static servo_cfg_t standard_cfg = {
    .min_pulse_us = 500,
    .max_pulse_us = 2500,
    .max_angle = 180
};

TEST(test_init_null)
{
    ASSERT_EQ(servo_init(NULL, &mock_pwm, &standard_cfg), SERVO_ERR_PARAM);
    servo_t dev;
    ASSERT_EQ(servo_init(&dev, NULL, &standard_cfg), SERVO_ERR_PARAM);
    ASSERT_EQ(servo_init(&dev, &mock_pwm, NULL), SERVO_ERR_PARAM);
}

TEST(test_init_bad_config)
{
    servo_t dev;
    servo_cfg_t bad = {.min_pulse_us = 2500, .max_pulse_us = 500, .max_angle = 180};
    ASSERT_EQ(servo_init(&dev, &mock_pwm, &bad), SERVO_ERR_PARAM);
    servo_cfg_t bad2 = {.min_pulse_us = 500, .max_pulse_us = 2500, .max_angle = 0};
    ASSERT_EQ(servo_init(&dev, &mock_pwm, &bad2), SERVO_ERR_PARAM);
}

TEST(test_angle_0)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 0), SERVO_OK);
    ASSERT_EQ(mock_last_pulse, 500);
    ASSERT_EQ(servo_get_angle(&dev), 0);
}

TEST(test_angle_90)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 90), SERVO_OK);
    ASSERT_EQ(mock_last_pulse, 1500);
    ASSERT_EQ(servo_get_angle(&dev), 90);
}

TEST(test_angle_180)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 180), SERVO_OK);
    ASSERT_EQ(mock_last_pulse, 2500);
}

TEST(test_angle_clamp)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 999), SERVO_OK);
    ASSERT_EQ(mock_last_pulse, 2500);
    ASSERT_EQ(servo_get_angle(&dev), 180);
}

TEST(test_hal_failure)
{
    mock_fail = true;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 90), SERVO_ERR_HAL);
}

TEST(test_set_pulse_direct)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_pulse(&dev, 1234), SERVO_OK);
    ASSERT_EQ(mock_last_pulse, 1234);
}

/* ---------- get_angle after set_pulse ---------- */

/* 500..2500 us over 180 degrees is 2000/180 us per degree */
TEST(test_set_pulse_moves_the_reported_angle)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);

    ASSERT_EQ(servo_set_angle(&dev, 30), SERVO_OK);
    ASSERT_EQ(servo_set_pulse(&dev, 2500), SERVO_OK);
    ASSERT_EQ(servo_get_angle(&dev), 180);

    ASSERT_EQ(servo_set_pulse(&dev, 500), SERVO_OK);
    ASSERT_EQ(servo_get_angle(&dev), 0);

    ASSERT_EQ(servo_set_pulse(&dev, 1500), SERVO_OK);
    ASSERT_EQ(servo_get_angle(&dev), 90);
}

TEST(test_pulse_from_an_angle_reads_as_that_angle)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);

    for (uint16_t angle = 0; angle <= 180; angle++) {
        ASSERT_EQ(servo_set_angle(&dev, angle), SERVO_OK);
        uint32_t pulse = mock_last_pulse;
        ASSERT_EQ(servo_set_angle(&dev, 0), SERVO_OK);
        ASSERT_EQ(servo_set_pulse(&dev, pulse), SERVO_OK);
        ASSERT_EQ(servo_get_angle(&dev), angle);
    }
}

/* The servo did not move, so neither does the angle */
TEST(test_failed_set_pulse_keeps_the_angle)
{
    mock_fail = false;
    servo_t dev;
    servo_init(&dev, &mock_pwm, &standard_cfg);
    ASSERT_EQ(servo_set_angle(&dev, 30), SERVO_OK);

    ASSERT_EQ(servo_set_pulse(&dev, 2501), SERVO_ERR_PARAM);
    ASSERT_EQ(servo_get_angle(&dev), 30);

    mock_fail = true;
    ASSERT_EQ(servo_set_pulse(&dev, 2500), SERVO_ERR_HAL);
    mock_fail = false;
    ASSERT_EQ(servo_get_angle(&dev), 30);
}

/* The largest cfg: the product must not leave uint32_t */
TEST(test_set_pulse_with_the_widest_config)
{
    mock_fail = false;
    servo_t dev;
    servo_cfg_t wide = { .min_pulse_us = 0, .max_pulse_us = 65535, .max_angle = 65535 };
    servo_init(&dev, &mock_pwm, &wide);

    ASSERT_EQ(servo_set_pulse(&dev, 65535), SERVO_OK);
    ASSERT_EQ(servo_get_angle(&dev), 65535);
}

/* ---------- a device init refused ---------- */

static void assert_device_refused(servo_t *dev)
{
    ASSERT_EQ(servo_set_angle(dev, 90), SERVO_ERR_PARAM);
    ASSERT_EQ(servo_set_pulse(dev, 1500), SERVO_ERR_PARAM);
    ASSERT_EQ(servo_get_angle(dev), 0);
}

TEST(test_zeroed_device_refused)
{
    static servo_t zeroed;
    assert_device_refused(&zeroed);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    servo_t dev;
    servo_pwm_t no_callback = { .set_pulse_us = NULL, .ctx = NULL };
    servo_cfg_t bad = { .min_pulse_us = 2500, .max_pulse_us = 500, .max_angle = 180 };

    memset(&dev, 0xA5, sizeof(dev));
    ASSERT_EQ(servo_init(&dev, NULL, &standard_cfg), SERVO_ERR_PARAM);
    assert_device_refused(&dev);

    memset(&dev, 0xA5, sizeof(dev));
    ASSERT_EQ(servo_init(&dev, &mock_pwm, NULL), SERVO_ERR_PARAM);
    assert_device_refused(&dev);

    memset(&dev, 0xA5, sizeof(dev));
    ASSERT_EQ(servo_init(&dev, &no_callback, &standard_cfg), SERVO_ERR_PARAM);
    assert_device_refused(&dev);

    memset(&dev, 0xA5, sizeof(dev));
    ASSERT_EQ(servo_init(&dev, &mock_pwm, &bad), SERVO_ERR_PARAM);
    assert_device_refused(&dev);
}

TEST_MAIN("servo")
    RUN(test_init_null);
    RUN(test_init_bad_config);
    RUN(test_angle_0);
    RUN(test_angle_90);
    RUN(test_angle_180);
    RUN(test_angle_clamp);
    RUN(test_hal_failure);
    RUN(test_set_pulse_direct);
    RUN(test_set_pulse_moves_the_reported_angle);
    RUN(test_pulse_from_an_angle_reads_as_that_angle);
    RUN(test_failed_set_pulse_keeps_the_angle);
    RUN(test_set_pulse_with_the_widest_config);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
