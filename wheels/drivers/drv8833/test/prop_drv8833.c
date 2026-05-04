// SPDX-License-Identifier: MIT
/*
 * prop_drv8833.c — property-based tests for DRV8833 dual motor driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "drv8833.h"

static uint16_t mock_duty[2][2]; /* [channel][pin] */
static bool     mock_sleeping;

static void mock_set_pwm(void *ctx, uint8_t channel, uint8_t pin, uint16_t duty)
{
    (void)ctx;
    if (channel < 2 && pin < 2) mock_duty[channel][pin] = duty;
}

static void mock_set_sleep(void *ctx, bool sleep)
{
    (void)ctx;
    mock_sleeping = sleep;
}

static const drv8833_hal_t prop_hal = {
    .set_pwm   = mock_set_pwm,
    .set_sleep = mock_set_sleep,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    memset(mock_duty, 0, sizeof(mock_duty));
    mock_sleeping = false;
    drv8833_t dev;
    uint16_t max_duty = (uint16_t)(100 + ARBI_UINT16(seed) % 10000);
    drv8833_init(&dev, &prop_hal, max_duty);
}

PROPERTY(speed_no_crash, 5000)
{
    memset(mock_duty, 0, sizeof(mock_duty));
    drv8833_t dev;
    drv8833_init(&dev, &prop_hal, 1000);

    drv8833_channel_t ch = (drv8833_channel_t)(ARBI_UINT8(seed) & 1);
    int16_t speed = (int16_t)ARBI_UINT16(seed);
    drv8833_set_speed(&dev, ch, speed);
    drv8833_brake(&dev, ch);
    drv8833_coast(&dev, ch);
}

PROPERTY(sleep_wake_no_crash, 5000)
{
    mock_sleeping = false;
    drv8833_t dev;
    drv8833_init(&dev, &prop_hal, 1000);

    drv8833_sleep(&dev);
    drv8833_wake(&dev);
}

TEST_MAIN("prop_drv8833")
    RUN(init_no_crash);
    RUN(speed_no_crash);
    RUN(sleep_wake_no_crash);
TEST_MAIN_END()
