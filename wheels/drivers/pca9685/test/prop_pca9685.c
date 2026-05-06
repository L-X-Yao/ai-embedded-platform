// SPDX-License-Identifier: MIT
/*
 * prop_pca9685.c — property-based tests for PCA9685 PWM driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "pca9685.h"

static const pca9685_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    pca9685_t dev;
    uint16_t freq = (uint16_t)(PCA9685_FREQ_MIN + ARBI_UINT16(seed) %
                    (PCA9685_FREQ_MAX - PCA9685_FREQ_MIN + 1));
    pca9685_init(&dev, &prop_i2c, PCA9685_ADDR_DEFAULT, freq);
}

PROPERTY(set_pwm_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, PCA9685_ADDR_DEFAULT, 50);

    uint8_t ch = ARBI_UINT8(seed) % 16;
    uint16_t on  = ARBI_UINT16(seed) & 0x0FFF;
    uint16_t off = ARBI_UINT16(seed) & 0x0FFF;
    pca9685_set_pwm(&dev, ch, on, off);

    float duty = (float)(ARBI_UINT8(seed) % 101);
    pca9685_set_duty(&dev, ch, duty);
}

PROPERTY(servo_sleep_wake_no_crash, 2000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Need valid MODE1 for sleep/wake read */
    mock_regs[0x00] = 0x01; /* just ALLCALL, not sleep */

    pca9685_t dev;
    if (pca9685_init(&dev, &prop_i2c, PCA9685_ADDR_DEFAULT, 50) != PCA9685_OK)
        return;

    uint16_t pulse = (uint16_t)(500 + ARBI_UINT16(seed) % 2001);
    pca9685_set_servo_us(&dev, (uint8_t)(ARBI_UINT8(seed) % 16), pulse);

    pca9685_all_off(&dev);

    uint16_t freq = (uint16_t)(PCA9685_FREQ_MIN +
                    ARBI_UINT16(seed) % (PCA9685_FREQ_MAX - PCA9685_FREQ_MIN + 1));
    pca9685_set_freq(&dev, freq);

    pca9685_sleep(&dev);
    pca9685_wake(&dev);
}

TEST(test_null_dev_guards)
{
    ASSERT_EQ(pca9685_set_pwm(NULL, 0, 0, 1000),     PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_duty(NULL, 0, 50.0f),       PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_servo_us(NULL, 0, 1500),    PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_all_off(NULL),                  PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_freq(NULL, 50),              PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_sleep(NULL),                    PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_wake(NULL),                     PCA9685_ERR_PARAM);
}

TEST(test_init_param_guards)
{
    mock_i2c_reset();
    pca9685_t dev;
    ASSERT_EQ(pca9685_init(NULL, &prop_i2c, 0x40, 50),  PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_init(&dev, NULL, 0x40, 50),        PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 10),   PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 2000), PCA9685_ERR_PARAM);
}

TEST(test_servo_us_zero_freq)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);
    dev.freq_hz = 0;  /* artificially clear to trigger guard */
    ASSERT_EQ(pca9685_set_servo_us(&dev, 0, 1500), PCA9685_ERR_PARAM);
}

TEST(test_set_freq_param_guards)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);
    ASSERT_EQ(pca9685_set_freq(&dev, 0),    PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_freq(&dev, 1527), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_freq(&dev, 50),   PCA9685_OK);
}

TEST(test_set_pwm_param_guards)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);
    ASSERT_EQ(pca9685_set_pwm(&dev, 16, 0, 1000),    PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 4096, 1000),  PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 4096),     PCA9685_ERR_PARAM);
}

TEST(test_set_duty_param_guards)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);
    float nan_val = 0.0f / 0.0f;
    ASSERT_EQ(pca9685_set_duty(&dev, 0, nan_val),   PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_duty(&dev, 0, -1.0f),     PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_duty(&dev, 0, 101.0f),    PCA9685_ERR_PARAM);
}

TEST(test_set_duty_zero_full_channel_guard)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);
    /* channel=16 should fail for 0% (full-off path) */
    ASSERT_EQ(pca9685_set_duty(&dev, 16, 0.0f),   PCA9685_ERR_PARAM);
    /* channel=16 should fail for 100% (full-on path) */
    ASSERT_EQ(pca9685_set_duty(&dev, 16, 100.0f), PCA9685_ERR_PARAM);
}

/* Helper: init a dev and reset the op counter so mock_fail_at works relative
 * to the next call (not counting init's own I2C ops). */
static pca9685_err_t pca9685_init_and_reset_counter(pca9685_t *dev)
{
    mock_i2c_reset();
    pca9685_err_t err = pca9685_init(dev, &prop_i2c, 0x40, 50);
    /* Reset just the op counter so mock_fail_at=0 hits the very next op */
    mock_op_count = 0;
    return err;
}

TEST(test_i2c_fail_set_pwm)
{
    pca9685_t dev;

    /* first write in set_pwm fails */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 0;
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 1000), PCA9685_ERR_I2C);

    /* second write in set_pwm fails */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 1;
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 1000), PCA9685_ERR_I2C);

    /* third write in set_pwm fails */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 2;
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 1000), PCA9685_ERR_I2C);

    /* fourth write in set_pwm fails */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 3;
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 1000), PCA9685_ERR_I2C);
}

TEST(test_i2c_fail_all_off)
{
    pca9685_t dev;

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 0;
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 1;
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 2;
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 3;
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_ERR_I2C);
}

TEST(test_i2c_fail_set_duty_full_off)
{
    pca9685_t dev;

    /* 0% path: 4 writes; fail on the 4th (index 3) */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 3;
    ASSERT_EQ(pca9685_set_duty(&dev, 0, 0.0f), PCA9685_ERR_I2C);
}

TEST(test_i2c_fail_sleep_wake)
{
    pca9685_t dev;

    /* sleep: first op is read_regs */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 0;
    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_ERR_I2C);

    /* sleep: second op is write MODE1 */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 1;
    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_ERR_I2C);

    /* wake: first op is read_regs */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 0;
    ASSERT_EQ(pca9685_wake(&dev), PCA9685_ERR_I2C);

    /* wake: second op is write MODE1 (clear SLEEP) */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 1;
    ASSERT_EQ(pca9685_wake(&dev), PCA9685_ERR_I2C);

    /* wake: third op is write MODE1 (set RESTART) */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 2;
    ASSERT_EQ(pca9685_wake(&dev), PCA9685_ERR_I2C);
}

TEST(test_init_i2c_fail)
{
    mock_i2c_reset();
    pca9685_t dev;
    /* fail on initial MODE1 write */
    mock_fail_next = 1;
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);
}

TEST(test_set_servo_us_clamp)
{
    mock_i2c_reset();
    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, 0x40, 50);

    /* Very large pulse should clamp to COUNT_MAX */
    ASSERT_EQ(pca9685_set_servo_us(&dev, 0, 65535), PCA9685_OK);
}

TEST(test_set_prescaler_i2c_failures)
{
    pca9685_t dev;

    /* set_prescaler is called during init (after initial MODE1 write).
     * Init does: write MODE1 (op0), then set_prescaler which does:
     *   read  MODE1 (op1), write MODE1 sleep (op2), write PRE_SCALE (op3),
     *   write MODE1 wake  (op4), [delay], write MODE1 restart (op5).
     * Fail each op inside set_prescaler. */

    mock_i2c_reset();
    mock_fail_at = 1;   /* fail read MODE1 inside set_prescaler */
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);

    mock_i2c_reset();
    mock_fail_at = 2;   /* fail write sleep mode */
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);

    mock_i2c_reset();
    mock_fail_at = 3;   /* fail write PRE_SCALE */
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);

    mock_i2c_reset();
    mock_fail_at = 4;   /* fail write wake mode */
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);

    mock_i2c_reset();
    mock_fail_at = 5;   /* fail write restart */
    ASSERT_EQ(pca9685_init(&dev, &prop_i2c, 0x40, 50), PCA9685_ERR_I2C);
}

TEST(test_set_freq_i2c_failures)
{
    pca9685_t dev;

    /* set_freq calls set_prescaler — same 5-op sequence,
     * but now op count starts from 0 after pca9685_init_and_reset_counter */
    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 0;   /* fail read MODE1 */
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 1;   /* fail write sleep */
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 2;   /* fail write PRE_SCALE */
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 3;   /* fail write wake */
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_ERR_I2C);

    pca9685_init_and_reset_counter(&dev);
    mock_fail_at = 4;   /* fail write restart */
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_ERR_I2C);
}

TEST_MAIN("prop_pca9685")
    RUN(init_no_crash);
    RUN(set_pwm_no_crash);
    RUN(servo_sleep_wake_no_crash);
    RUN(test_null_dev_guards);
    RUN(test_init_param_guards);
    RUN(test_servo_us_zero_freq);
    RUN(test_set_freq_param_guards);
    RUN(test_set_pwm_param_guards);
    RUN(test_set_duty_param_guards);
    RUN(test_set_duty_zero_full_channel_guard);
    RUN(test_i2c_fail_set_pwm);
    RUN(test_i2c_fail_all_off);
    RUN(test_i2c_fail_set_duty_full_off);
    RUN(test_i2c_fail_sleep_wake);
    RUN(test_init_i2c_fail);
    RUN(test_set_servo_us_clamp);
    RUN(test_set_prescaler_i2c_failures);
    RUN(test_set_freq_i2c_failures);
TEST_MAIN_END()
