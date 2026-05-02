// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "pca9685.h"

/* ---------- delay mock ---------- */
static uint32_t delay_total_ms;

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    delay_total_ms += ms;
}

/* ---------- chip-specific mock setup ---------- */

/* PCA9685 register addresses mirrored in mock */
#define REG_MODE1       0x00u
#define REG_PRE_SCALE   0xFEu
#define REG_LED0_ON_L   0x06u
#define REG_ALL_OFF_H   0xFDu
#define MODE1_SLEEP     0x10u
#define MODE1_RESTART   0x80u
#define FULL_ON_BIT     0x10u

static void mock_reset(void)
{
    mock_i2c_reset();
    delay_total_ms = 0;
    /* Default MODE1: sleep + allcall */
    mock_regs[REG_MODE1] = 0x11;
    /* Default prescaler for ~200Hz */
    mock_regs[REG_PRE_SCALE] = 30;
}

static const pca9685_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_delay_ms,
    .ctx       = NULL,
};

static pca9685_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(pca9685_init(&dev, &mock_i2c, 0x40, 50), PCA9685_OK);
    ASSERT_EQ(dev.dev_addr, 0x40);
    ASSERT_EQ(dev.freq_hz, 50);
    /* Verify prescaler was written (50Hz: round(25M/(4096*50))-1 = 121) */
    ASSERT_EQ(mock_regs[REG_PRE_SCALE], 121);
}

TEST(test_init_null_dev)
{
    mock_reset();
    ASSERT_EQ(pca9685_init(NULL, &mock_i2c, 0x40, 50), PCA9685_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    mock_reset();
    ASSERT_EQ(pca9685_init(&dev, NULL, 0x40, 50), PCA9685_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset();
    pca9685_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    ASSERT_EQ(pca9685_init(&dev, &bad, 0x40, 50), PCA9685_ERR_PARAM);
}

TEST(test_init_freq_too_low)
{
    mock_reset();
    ASSERT_EQ(pca9685_init(&dev, &mock_i2c, 0x40, 10), PCA9685_ERR_PARAM);
}

TEST(test_init_freq_too_high)
{
    mock_reset();
    ASSERT_EQ(pca9685_init(&dev, &mock_i2c, 0x40, 2000), PCA9685_ERR_PARAM);
}

TEST(test_set_pwm_channel0)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 2048), PCA9685_OK);
    /* Channel 0: base = 0x06
     * ON_L=0x06, ON_H=0x07, OFF_L=0x08, OFF_H=0x09 */
    ASSERT_EQ(mock_regs[0x06], 0x00);       /* ON_L */
    ASSERT_EQ(mock_regs[0x07], 0x00);       /* ON_H */
    ASSERT_EQ(mock_regs[0x08], 0x00);       /* OFF_L: 2048 & 0xFF = 0 */
    ASSERT_EQ(mock_regs[0x09], 0x08);       /* OFF_H: 2048 >> 8 = 8 */
}

TEST(test_set_pwm_channel15)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_pwm(&dev, 15, 100, 3000), PCA9685_OK);
    /* Channel 15: base = 0x06 + 15*4 = 0x42 */
    uint8_t base = 0x42;
    ASSERT_EQ(mock_regs[base],     (uint8_t)(100 & 0xFF));
    ASSERT_EQ(mock_regs[base + 1], (uint8_t)((100 >> 8) & 0x0F));
    ASSERT_EQ(mock_regs[base + 2], (uint8_t)(3000 & 0xFF));
    ASSERT_EQ(mock_regs[base + 3], (uint8_t)((3000 >> 8) & 0x0F));
}

TEST(test_set_pwm_channel_out_of_range)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_pwm(&dev, 16, 0, 1000), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_pwm(&dev, 255, 0, 1000), PCA9685_ERR_PARAM);
}

TEST(test_set_duty_50_percent)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 50.0f), PCA9685_OK);
    /* 50% of 4095 = 2047.5, rounded to 2048 */
    uint16_t off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 2048);
}

TEST(test_set_duty_0_percent)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 0.0f), PCA9685_OK);
    /* Full OFF: OFF_H bit 4 set */
    ASSERT_EQ(mock_regs[0x09] & FULL_ON_BIT, FULL_ON_BIT);
}

TEST(test_set_duty_100_percent)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 100.0f), PCA9685_OK);
    /* Full ON: ON_H bit 4 set */
    ASSERT_EQ(mock_regs[0x07] & FULL_ON_BIT, FULL_ON_BIT);
    /* OFF_H bit 4 should NOT be set */
    ASSERT_EQ(mock_regs[0x09] & FULL_ON_BIT, 0);
}

TEST(test_set_servo_us)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* At 50Hz: period = 20000us, 1500us pulse = 1500/20000 * 4096 = 307.2 -> 307 */
    ASSERT_EQ(pca9685_set_servo_us(&dev, 0, 1500), PCA9685_OK);
    uint16_t off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    /* 1500 * 4096 / 20000 = 307.2 -> truncated to 307 */
    ASSERT_EQ(off, 307);
}

TEST(test_all_off)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* Set a channel first */
    pca9685_set_pwm(&dev, 0, 0, 2048);
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_OK);

    /* ALL_OFF_H (0xFD) should have FULL_ON_BIT set */
    ASSERT_EQ(mock_regs[REG_ALL_OFF_H] & FULL_ON_BIT, FULL_ON_BIT);
}

TEST(test_set_freq_prescaler_calc)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* Change to 1000Hz: prescale = round(25M/(4096*1000))-1 = 5 */
    ASSERT_EQ(pca9685_set_freq(&dev, 1000), PCA9685_OK);
    ASSERT_EQ(mock_regs[REG_PRE_SCALE], 5);
    ASSERT_EQ(dev.freq_hz, 1000);
}

TEST(test_set_freq_range_validation)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_freq(&dev, 23), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_freq(&dev, 1527), PCA9685_ERR_PARAM);
    /* Boundary values should work */
    ASSERT_EQ(pca9685_set_freq(&dev, 24), PCA9685_OK);
    ASSERT_EQ(pca9685_set_freq(&dev, 1526), PCA9685_OK);
}

TEST(test_sleep_wake)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* Sleep: MODE1 SLEEP bit should be set */
    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    ASSERT_TRUE(mock_regs[REG_MODE1] & MODE1_SLEEP);

    /* Wake: MODE1 SLEEP bit should be cleared, RESTART set */
    ASSERT_EQ(pca9685_wake(&dev), PCA9685_OK);
    ASSERT_FALSE(mock_regs[REG_MODE1] & MODE1_SLEEP);
    ASSERT_TRUE(mock_regs[REG_MODE1] & MODE1_RESTART);
}

TEST(test_i2c_failure)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* Make next I2C op fail */
    mock_fail_next = 1;
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 1000), PCA9685_ERR_I2C);

    mock_fail_next = 1;
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_ERR_I2C);

    mock_fail_next = 1;
    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_ERR_I2C);
}

TEST_MAIN("pca9685")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_callbacks);
    RUN(test_init_freq_too_low);
    RUN(test_init_freq_too_high);
    RUN(test_set_pwm_channel0);
    RUN(test_set_pwm_channel15);
    RUN(test_set_pwm_channel_out_of_range);
    RUN(test_set_duty_50_percent);
    RUN(test_set_duty_0_percent);
    RUN(test_set_duty_100_percent);
    RUN(test_set_servo_us);
    RUN(test_all_off);
    RUN(test_set_freq_prescaler_calc);
    RUN(test_set_freq_range_validation);
    RUN(test_sleep_wake);
    RUN(test_i2c_failure);
TEST_MAIN_END()
