// SPDX-License-Identifier: MIT
/*
 * prop_servo.c — property-based tests for servo PWM driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "servo.h"

static uint32_t mock_last_pulse;
static bool mock_set_pulse_us(void *ctx, uint32_t pulse_us)
{
    (void)ctx;
    mock_last_pulse = pulse_us;
    return true;
}

static const servo_pwm_t prop_pwm = {
    .set_pulse_us = mock_set_pulse_us,
    .ctx          = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_last_pulse = 0;
    servo_t dev;
    servo_cfg_t cfg = {
        .min_pulse_us = (uint16_t)(500 + ARBI_UINT16(seed) % 500),
        .max_pulse_us = (uint16_t)(2000 + ARBI_UINT16(seed) % 500),
        .max_angle    = (uint16_t)(90 + ARBI_UINT16(seed) % 180),
    };
    servo_init(&dev, &prop_pwm, &cfg);
}

PROPERTY(angle_clamped, 5000)
{
    mock_last_pulse = 0;
    servo_t dev;
    servo_cfg_t cfg = { .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle = 180 };
    servo_init(&dev, &prop_pwm, &cfg);

    uint16_t angle = ARBI_UINT16(seed);
    servo_set_angle(&dev, angle);

    /* Current angle must be clamped to [0, max_angle] */
    uint16_t actual = servo_get_angle(&dev);
    PROP_ASSERT(actual <= cfg.max_angle);
}

PROPERTY(pulse_bounded, 5000)
{
    mock_last_pulse = 0;
    servo_t dev;
    servo_cfg_t cfg = { .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle = 180 };
    servo_init(&dev, &prop_pwm, &cfg);

    uint16_t angle = ARBI_UINT16(seed) % 181;
    servo_err_t err = servo_set_angle(&dev, angle);
    if (err != SERVO_OK) return;

    PROP_ASSERT(mock_last_pulse >= cfg.min_pulse_us);
    PROP_ASSERT(mock_last_pulse <= cfg.max_pulse_us);
}

TEST_MAIN("prop_servo")
    RUN(init_no_crash);
    RUN(angle_clamped);
    RUN(pulse_bounded);
TEST_MAIN_END()
