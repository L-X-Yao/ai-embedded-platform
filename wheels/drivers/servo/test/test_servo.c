#include "servo.h"
#include <stdio.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) static void name(void)
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL %s:%d: %d != %d\n", __func__, __LINE__, \
               (int)(a), (int)(b)); \
        g_fail++; return; \
    } } while(0)
#define RUN(fn) do { int pf = g_fail; fn(); if(g_fail == pf) { \
    printf("  PASS %s\n", #fn); g_pass++; } } while(0)

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

int main(void)
{
    printf("servo tests:\n");
    RUN(test_init_null);
    RUN(test_init_bad_config);
    RUN(test_angle_0);
    RUN(test_angle_90);
    RUN(test_angle_180);
    RUN(test_angle_clamp);
    RUN(test_hal_failure);
    RUN(test_set_pulse_direct);
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
