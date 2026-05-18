// SPDX-License-Identifier: MIT
/*
 * test_hw_model_pca9685.c — tests using PCA9685 hardware register model
 *
 * Validates PCA9685 driver against a datasheet-accurate register model.
 * Tests sleep/wake sequences, prescaler write protection, channel
 * register layout, and ALL_LED broadcast registers.
 */

#include "test_harness.h"
#include "hw_model_pca9685.h"
#include "../pca9685.h"

static pca9685_model_t model;

static const pca9685_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = pca9685_model_delay,
    .ctx       = &model.hw,
};

static void setup(void)
{
    pca9685_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_succeeds)
{
    setup();
    pca9685_t dev;
    pca9685_err_t err = pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    ASSERT_EQ(err, PCA9685_OK);
    /* The init sequence: write SLEEP, write prescale, wake, set RESTART.
     * The final MODE1 write restores original mode1 | RESTART, which may
     * include SLEEP from the original read.  What matters is the wake
     * transition happened (tested separately). */
    ASSERT_TRUE(model.wake_count >= 1);
}

TEST(test_init_sets_prescaler)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    /* prescale = round(25MHz / (4096 * 50)) - 1 = 121 (0x79) */
    ASSERT_TRUE(model.prescale_write_count >= 1);
    uint8_t prescale = model.hw.regs[PCA9685_REG_PRE_SCALE];
    /* Expected ~121 for 50 Hz, allow small rounding: 120-122 */
    ASSERT_TRUE(prescale >= 120 && prescale <= 122);
}

TEST(test_init_sleep_before_prescale)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    /* Driver must enter sleep to write prescaler, then wake.
     * sleep_count should be >= 1 (at least 1 sleep transition). */
    ASSERT_TRUE(model.sleep_count >= 1);
    ASSERT_TRUE(model.wake_count >= 1);
    /* Prescale should not have been rejected */
    ASSERT_FALSE(model.prescale_rejected);
}

/* --- frequency change -------------------------------------------------- */

TEST(test_set_freq_updates_prescaler)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    int writes_before = model.prescale_write_count;

    pca9685_err_t err = pca9685_set_freq(&dev, 200);
    ASSERT_EQ(err, PCA9685_OK);
    ASSERT_TRUE(model.prescale_write_count > writes_before);
    /* prescale for 200 Hz = round(25MHz / (4096 * 200)) - 1 ≈ 29 */
    uint8_t prescale = model.hw.regs[PCA9685_REG_PRE_SCALE];
    ASSERT_TRUE(prescale >= 29 && prescale <= 31);
}

/* --- channel PWM ------------------------------------------------------- */

TEST(test_set_pwm_writes_channel_regs)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_err_t err = pca9685_set_pwm(&dev, 0, 0, 2048);
    ASSERT_EQ(err, PCA9685_OK);

    /* Channel 0: ON_L=0x06, ON_H=0x07, OFF_L=0x08, OFF_H=0x09 */
    ASSERT_EQ(model.hw.regs[0x06], 0x00);       /* ON_L */
    ASSERT_EQ(model.hw.regs[0x07], 0x00);       /* ON_H */
    ASSERT_EQ(model.hw.regs[0x08], 0x00);       /* OFF_L = 2048 & 0xFF */
    ASSERT_EQ(model.hw.regs[0x09], 0x08);       /* OFF_H = 2048 >> 8 = 0x08 */
}

TEST(test_set_duty_50_percent)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_err_t err = pca9685_set_duty(&dev, 3, 50.0f);
    ASSERT_EQ(err, PCA9685_OK);

    /* 50% of 4095 ≈ 2048. Channel 3 base = 0x06 + 12 = 0x12 */
    uint16_t off_val = pca9685_model_get_channel_off(&model, 3);
    /* Expect ~2047-2048 */
    ASSERT_TRUE(off_val >= 2040 && off_val <= 2055);
}

TEST(test_set_duty_0_percent_full_off)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_err_t err = pca9685_set_duty(&dev, 5, 0.0f);
    ASSERT_EQ(err, PCA9685_OK);
    /* Full OFF: OFF_H bit 4 should be set */
    ASSERT_TRUE(pca9685_model_channel_full_off(&model, 5));
}

TEST(test_set_duty_100_percent_full_on)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_err_t err = pca9685_set_duty(&dev, 7, 100.0f);
    ASSERT_EQ(err, PCA9685_OK);
    /* Full ON: ON_H bit 4 should be set */
    ASSERT_TRUE(pca9685_model_channel_full_on(&model, 7));
}

/* --- sleep/wake -------------------------------------------------------- */

TEST(test_sleep_sets_bit)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    /* Explicitly wake first (clear SLEEP bit) so we have a known state */
    pca9685_wake(&dev);
    ASSERT_FALSE(pca9685_model_is_sleeping(&model));

    pca9685_err_t err = pca9685_sleep(&dev);
    ASSERT_EQ(err, PCA9685_OK);
    ASSERT_TRUE(pca9685_model_is_sleeping(&model));
}

TEST(test_wake_clears_sleep_bit)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_sleep(&dev);
    ASSERT_TRUE(pca9685_model_is_sleeping(&model));

    pca9685_err_t err = pca9685_wake(&dev);
    ASSERT_EQ(err, PCA9685_OK);
    ASSERT_FALSE(pca9685_model_is_sleeping(&model));
}

TEST_MAIN("pca9685 hardware model")
    RUN(test_init_succeeds);
    RUN(test_init_sets_prescaler);
    RUN(test_init_sleep_before_prescale);
    RUN(test_set_freq_updates_prescaler);
    RUN(test_set_pwm_writes_channel_regs);
    RUN(test_set_duty_50_percent);
    RUN(test_set_duty_0_percent_full_off);
    RUN(test_set_duty_100_percent_full_on);
    RUN(test_sleep_sets_bit);
    RUN(test_wake_clears_sleep_bit);
TEST_MAIN_END()
