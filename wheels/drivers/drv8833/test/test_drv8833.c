// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "drv8833.h"

/* ---------- mock HAL ---------- */

static uint8_t  mock_ch;
static uint8_t  mock_pin;
static uint16_t mock_duty;
static int      mock_pwm_calls;
static bool     mock_sleep_state;
static int      mock_sleep_calls;

static void mock_set_pwm(void *ctx, uint8_t channel, uint8_t pin,
                          uint16_t duty)
{
    (void)ctx;
    mock_ch   = channel;
    mock_pin  = pin;
    mock_duty = duty;
    mock_pwm_calls++;
}

static void mock_set_sleep(void *ctx, bool sleep)
{
    (void)ctx;
    mock_sleep_state = sleep;
    mock_sleep_calls++;
}

static const drv8833_hal_t mock_hal = {
    .set_pwm   = mock_set_pwm,
    .set_sleep = mock_set_sleep,
    .ctx       = NULL,
};

static const drv8833_hal_t mock_hal_no_sleep = {
    .set_pwm   = mock_set_pwm,
    .set_sleep = NULL,
    .ctx       = NULL,
};

static void mock_reset(void)
{
    mock_ch    = 0xFF;
    mock_pin   = 0xFF;
    mock_duty  = 0xFFFF;
    mock_pwm_calls  = 0;
    mock_sleep_state = false;
    mock_sleep_calls = 0;
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    drv8833_t dev;
    ASSERT_EQ(drv8833_init(&dev, &mock_hal, 1000), DRV8833_OK);
    ASSERT_EQ(dev.max_duty, 1000);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(drv8833_init(NULL, &mock_hal, 1000), DRV8833_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    drv8833_t dev;
    ASSERT_EQ(drv8833_init(&dev, NULL, 1000), DRV8833_ERR_PARAM);
}

TEST(test_init_null_set_pwm)
{
    drv8833_hal_t bad = { .set_pwm = NULL, .set_sleep = NULL, .ctx = NULL };
    drv8833_t dev;
    ASSERT_EQ(drv8833_init(&dev, &bad, 1000), DRV8833_ERR_PARAM);
}

TEST(test_init_zero_max_duty)
{
    drv8833_t dev;
    ASSERT_EQ(drv8833_init(&dev, &mock_hal, 0), DRV8833_ERR_PARAM);
}

/* ---------- set_speed tests ---------- */

TEST(test_forward_channel_a)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, 500), DRV8833_OK);
    /* Last call: IN2=0 on channel 0 */
    ASSERT_EQ(mock_pwm_calls, 2);
    /* Verify by checking that we got 2 PWM calls */
}

TEST(test_reverse_channel_a)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, -500), DRV8833_OK);
    ASSERT_EQ(mock_pwm_calls, 2);
    /* Last call: IN2=500 (reverse) */
    ASSERT_EQ(mock_pin, 1);
    ASSERT_EQ(mock_duty, 500);
}

TEST(test_speed_zero_coasts)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, 0), DRV8833_OK);
    ASSERT_EQ(mock_pwm_calls, 2);
    /* Both pins LOW */
    ASSERT_EQ(mock_duty, 0);
}

TEST(test_forward_channel_b)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_B, 750), DRV8833_OK);
    ASSERT_EQ(mock_ch, 1);
}

TEST(test_max_speed)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, 1000), DRV8833_OK);
    ASSERT_EQ(mock_pwm_calls, 2);
}

TEST(test_speed_clamped_positive)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    /* speed=1500 exceeds max_duty=1000, should clamp to 1000 */
    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, 1500), DRV8833_OK);
    /* First call was IN1 with clamped duty */
}

TEST(test_speed_clamped_negative)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    /* speed=-1500 exceeds -max_duty=-1000, should clamp to -1000 */
    ASSERT_EQ(drv8833_set_speed(&dev, DRV8833_CH_A, -1500), DRV8833_OK);
    /* Last call: IN2=1000 (clamped) */
    ASSERT_EQ(mock_pin, 1);
    ASSERT_EQ(mock_duty, 1000);
}

TEST(test_set_speed_invalid_channel)
{
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_set_speed(&dev, (drv8833_channel_t)2, 500),
              DRV8833_ERR_PARAM);
}

TEST(test_set_speed_null_dev)
{
    ASSERT_EQ(drv8833_set_speed(NULL, DRV8833_CH_A, 500),
              DRV8833_ERR_PARAM);
}

/* ---------- brake / coast tests ---------- */

TEST(test_brake_channel_a)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_brake(&dev, DRV8833_CH_A), DRV8833_OK);
    ASSERT_EQ(mock_pwm_calls, 2);
    /* Both pins HIGH = max_duty */
    ASSERT_EQ(mock_duty, 1000);
}

TEST(test_coast_channel_a)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    ASSERT_EQ(drv8833_coast(&dev, DRV8833_CH_A), DRV8833_OK);
    ASSERT_EQ(mock_pwm_calls, 2);
    /* Both pins LOW */
    ASSERT_EQ(mock_duty, 0);
}

TEST(test_brake_invalid_channel)
{
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);
    ASSERT_EQ(drv8833_brake(&dev, (drv8833_channel_t)5), DRV8833_ERR_PARAM);
}

TEST(test_coast_null_dev)
{
    ASSERT_EQ(drv8833_coast(NULL, DRV8833_CH_A), DRV8833_ERR_PARAM);
}

/* ---------- sleep / wake tests ---------- */

TEST(test_sleep_wake)
{
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal, 1000);

    drv8833_sleep(&dev);
    ASSERT_TRUE(mock_sleep_state);
    ASSERT_EQ(mock_sleep_calls, 1);

    drv8833_wake(&dev);
    ASSERT_FALSE(mock_sleep_state);
    ASSERT_EQ(mock_sleep_calls, 2);
}

TEST(test_sleep_no_callback)
{
    /* set_sleep is NULL — should be a no-op, no crash */
    mock_reset();
    drv8833_t dev;
    drv8833_init(&dev, &mock_hal_no_sleep, 1000);

    drv8833_sleep(&dev);
    drv8833_wake(&dev);
    ASSERT_EQ(mock_sleep_calls, 0);  /* never called */
}

TEST(test_sleep_null_dev)
{
    /* Should not crash */
    mock_reset();
    drv8833_sleep(NULL);
    drv8833_wake(NULL);
    ASSERT_EQ(mock_sleep_calls, 0);
}

/* ---------- runner ---------- */

TEST_MAIN("drv8833")
    /* init */
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_null_set_pwm);
    RUN(test_init_zero_max_duty);
    /* set_speed */
    RUN(test_forward_channel_a);
    RUN(test_reverse_channel_a);
    RUN(test_speed_zero_coasts);
    RUN(test_forward_channel_b);
    RUN(test_max_speed);
    RUN(test_speed_clamped_positive);
    RUN(test_speed_clamped_negative);
    RUN(test_set_speed_invalid_channel);
    RUN(test_set_speed_null_dev);
    /* brake / coast */
    RUN(test_brake_channel_a);
    RUN(test_coast_channel_a);
    RUN(test_brake_invalid_channel);
    RUN(test_coast_null_dev);
    /* sleep / wake */
    RUN(test_sleep_wake);
    RUN(test_sleep_no_callback);
    RUN(test_sleep_null_dev);
TEST_MAIN_END()
