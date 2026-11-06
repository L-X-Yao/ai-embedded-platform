// SPDX-License-Identifier: MIT
/*
 * test_hw_model_pca9685.c — tests using PCA9685 hardware register model
 *
 * Validates PCA9685 driver against a datasheet-accurate register model.
 * Tests reset values, sleep/wake sequences, the RESTART logic, prescaler
 * write protection, channel register layout, and ALL_LED broadcast registers.
 */

#include "test_harness.h"
#include "hw_model_pca9685.h"
#include "../pca9685.h"

/* One PWM cycle at 50 Hz. Equation 1 p.25 gives prescale 121, and one cycle
 * is 4096 * (121 + 1) periods of the 25 MHz oscillator. */
#define REFRESH_50HZ_US 19988u

static pca9685_model_t model;

static const pca9685_i2c_t model_i2c = {
    .write_reg = pca9685_model_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = pca9685_model_delay,
    .ctx       = &model.hw,
};

static void setup(void)
{
    pca9685_model_init(&model);
}

static uint8_t chip_read(uint8_t reg)
{
    uint8_t val = 0xA5;
    (void)hw_model_i2c_read_regs(&model.hw, PCA9685_ADDR_DEFAULT, reg, &val, 1);
    return val;
}

static bool chip_write(uint8_t reg, uint8_t val)
{
    return pca9685_model_write_reg(&model.hw, PCA9685_ADDR_DEFAULT, reg, val);
}

static uint8_t restart_bit(void)
{
    return (uint8_t)(chip_read(PCA9685_REG_MODE1) & PCA9685_MODE1_RESTART);
}

/* p.15: asleep with a channel running, past the end of the refresh cycle. */
static void sleep_with_channel_active(pca9685_t *dev)
{
    (void)pca9685_init(dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    (void)pca9685_set_pwm(dev, 0, 0, 2048);
    (void)pca9685_sleep(dev);
    pca9685_model_advance_us(&model, REFRESH_50HZ_US);
}

/* --- reset values ------------------------------------------------------ */

/* Table 5 p.14, Table 6 p.16, Tables 8 to 10 p.25 and p.26. */
TEST(test_por_register_values)
{
    setup();
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_MODE1), 0x11);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_MODE2), 0x04);
    ASSERT_EQ_HEX(chip_read(0x02), 0xE2);
    ASSERT_EQ_HEX(chip_read(0x03), 0xE4);
    ASSERT_EQ_HEX(chip_read(0x04), 0xE8);
    ASSERT_EQ_HEX(chip_read(0x05), 0xE0);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_PRE_SCALE), 0x1E);
}

/* Table 7 p.21: "LED0_OFF_H[4] R/W 1* LED0 full OFF", and so for every LED. */
TEST(test_por_channels_are_full_off)
{
    setup();
    for (uint8_t ch = 0; ch < 16u; ch++) {
        uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
        ASSERT_EQ_HEX(chip_read(base), 0x00);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 1u)), 0x00);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 2u)), 0x00);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 3u)), 0x10);
        ASSERT_TRUE(pca9685_model_channel_full_off(&model, ch));
    }
}

/* Table 7: bits 7:5 of the high bytes are "non-writable". Table 6: bits 7 to 5
 * of MODE2 are read only. p.26: the LSB of the address registers is read-only. */
TEST(test_read_only_bits_cannot_be_written)
{
    setup();
    ASSERT_TRUE(chip_write(0x07, 0xFF));
    ASSERT_EQ_HEX(chip_read(0x07), 0x1F);
    ASSERT_TRUE(chip_write(0x09, 0xFF));
    ASSERT_EQ_HEX(chip_read(0x09), 0x1F);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE2, 0xFF));
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_MODE2), 0x1F);
    ASSERT_TRUE(chip_write(0x02, 0xFF));
    ASSERT_EQ_HEX(chip_read(0x02), 0xFE);
    ASSERT_TRUE(chip_write(0x05, 0xFF));
    ASSERT_EQ_HEX(chip_read(0x05), 0xFE);
}

/* p.13: "All further addresses are reserved for future use; reserved addresses
 * will not be acknowledged." */
TEST(test_reserved_addresses_are_not_acknowledged)
{
    setup();
    ASSERT_TRUE(chip_write(0x45, 0x10));
    ASSERT_FALSE(chip_write(0x46, 0x00));
    ASSERT_FALSE(chip_write(0xF9, 0x00));
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_ON_L, 0x00));
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_succeeds)
{
    setup();
    pca9685_t dev;
    pca9685_err_t err = pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    ASSERT_EQ(err, PCA9685_OK);
    /* The init sequence: write SLEEP, write prescale, wake, set RESTART.
     * p.14: "No PWM control is possible when the oscillator is off", so the
     * chip must be awake at the end, not only have woken on the way. */
    ASSERT_TRUE(model.wake_count >= 1);
    ASSERT_FALSE(pca9685_model_is_sleeping(&model));
    /* p.14: a written 1 clears the bit, so none is left behind */
    ASSERT_EQ(restart_bit(), 0);
    ASSERT_FALSE(pca9685_model_restart_too_soon(&model));
}

TEST(test_init_sets_prescaler)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    /* Equation 1 p.25: round(25 MHz / (4096 * 50)) - 1 = 121 */
    ASSERT_TRUE(model.prescale_write_count >= 1);
    ASSERT_EQ(model.hw.regs[PCA9685_REG_PRE_SCALE], 121);
}

TEST(test_init_sleep_before_prescale)
{
    setup();
    /* Awake, as after a restart of the MCU alone. From power-up the chip
     * already sleeps (p.14), so init would have no transition to make. */
    model.hw.regs[PCA9685_REG_MODE1] = PCA9685_MODE1_ALLCALL;
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
    /* Equation 2 p.25: 200 Hz gives 30 = 0x1E */
    ASSERT_EQ_HEX(model.hw.regs[PCA9685_REG_PRE_SCALE], 0x1E);
}

/* p.13: "Writes to PRE_SCALE register are blocked when SLEEP bit is logic 0" */
TEST(test_prescale_is_blocked_while_awake)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    ASSERT_TRUE(chip_write(PCA9685_REG_PRE_SCALE, 0x1E));
    ASSERT_EQ(chip_read(PCA9685_REG_PRE_SCALE), 121);
    ASSERT_TRUE(model.prescale_rejected);
}

/* p.25: "The hardware forces a minimum value that can be loaded into the
 * PRE_SCALE register at '3'." */
TEST(test_prescale_hardware_minimum)
{
    setup();
    ASSERT_TRUE(chip_write(PCA9685_REG_PRE_SCALE, 0x01));
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_PRE_SCALE), 0x03);
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

/* The channel leaves reset full OFF, which ignores the counts (p.24). */
TEST(test_set_pwm_clears_full_off)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    ASSERT_TRUE(pca9685_model_channel_full_off(&model, 4));
    ASSERT_EQ(pca9685_set_pwm(&dev, 4, 0, 2048), PCA9685_OK);
    ASSERT_FALSE(pca9685_model_channel_full_off(&model, 4));
}

TEST(test_set_duty_50_percent)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    pca9685_err_t err = pca9685_set_duty(&dev, 3, 50.0f);
    ASSERT_EQ(err, PCA9685_OK);

    /* 50% of 4096 = 2048. Channel 3 base = 0x06 + 12 = 0x12 */
    ASSERT_EQ(pca9685_model_get_channel_off(&model, 3), 2048);
}

TEST(test_set_duty_0_percent_full_off)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    /* start from a running channel, or reset alone would pass this */
    ASSERT_EQ(pca9685_set_pwm(&dev, 5, 0, 2048), PCA9685_OK);
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

/* p.24: "If LEDn_ON_H[4] and LEDn_OFF_H[4] are set at the same time, the
 * LEDn_OFF_H[4] function takes precedence." */
TEST(test_full_off_beats_full_on)
{
    setup();
    ASSERT_TRUE(chip_write(0x07, 0x10));
    ASSERT_FALSE(pca9685_model_channel_full_on(&model, 0));

    ASSERT_TRUE(chip_write(0x09, 0x00));
    ASSERT_TRUE(pca9685_model_channel_full_on(&model, 0));
}

/* --- ALL_LED ----------------------------------------------------------- */

/* p.25: "allow just four I2C-bus write sequences to fill all the ON and OFF
 * registers with the same patterns." */
TEST(test_all_led_fills_every_channel)
{
    setup();
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_ON_L, 0x34));
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_ON_H, 0x02));
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_OFF_L, 0x78));
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_OFF_H, 0x06));

    for (uint8_t ch = 0; ch < 16u; ch++) {
        uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
        ASSERT_EQ_HEX(chip_read(base), 0x34);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 1u)), 0x02);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 2u)), 0x78);
        ASSERT_EQ_HEX(chip_read((uint8_t)(base + 3u)), 0x06);
    }
}

/* Table 4 p.13: ALL_LED registers are "write/read zero". */
TEST(test_all_led_reads_zero)
{
    setup();
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_ON_L, 0x34));
    ASSERT_TRUE(chip_write(PCA9685_REG_ALL_OFF_H, 0x10));
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_ALL_ON_L), 0x00);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_ALL_ON_H), 0x00);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_ALL_OFF_L), 0x00);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_ALL_OFF_H), 0x00);
}

TEST(test_all_off_reaches_every_channel)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 2048), PCA9685_OK);
    ASSERT_EQ(pca9685_set_pwm(&dev, 15, 100, 3000), PCA9685_OK);
    ASSERT_EQ(pca9685_set_duty(&dev, 7, 100.0f), PCA9685_OK);

    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_OK);

    for (uint8_t ch = 0; ch < 16u; ch++) {
        ASSERT_TRUE(pca9685_model_channel_full_off(&model, ch));
        ASSERT_FALSE(pca9685_model_channel_full_on(&model, ch));
    }
}

/* --- sleep/wake -------------------------------------------------------- */

TEST(test_sleep_sets_bit)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

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

/* --- RESTART ----------------------------------------------------------- */

/* p.15: "the RESTART bit (MODE1 bit 7) will be set to logic 1 at the end of
 * the PWM refresh cycle." */
TEST(test_restart_sets_at_end_of_refresh_cycle)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 2048), PCA9685_OK);

    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    pca9685_model_advance_us(&model, REFRESH_50HZ_US - 1u);
    ASSERT_EQ(restart_bit(), 0);

    pca9685_model_advance_us(&model, 1u);
    ASSERT_EQ_HEX(restart_bit(), PCA9685_MODE1_RESTART);
}

/* p.14: "User writes logic 1 to this bit to clear it to logic 0." */
TEST(test_written_one_clears_restart)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1, mode1));
    ASSERT_EQ(restart_bit(), 0);
}

/* p.14: "A user write of logic 0 will have no effect." */
TEST(test_written_zero_leaves_restart)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           (uint8_t)(mode1 & ~PCA9685_MODE1_RESTART)));
    ASSERT_EQ_HEX(restart_bit(), PCA9685_MODE1_RESTART);
}

/* The bit is the chip's to set: a master can only clear it. */
TEST(test_restart_cannot_be_set_by_writing)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           (uint8_t)(mode1 | PCA9685_MODE1_RESTART)));
    ASSERT_EQ(restart_bit(), 0);
}

/* p.15: "All PWM channels will restart and the RESTART bit will clear", and
 * "The contents of each PWM register are held valid when the clock is off." */
TEST(test_wake_restarts_the_channels)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);
    ASSERT_FALSE(pca9685_model_channels_running(&model));

    ASSERT_EQ(pca9685_wake(&dev), PCA9685_OK);
    ASSERT_EQ(restart_bit(), 0);
    ASSERT_TRUE(pca9685_model_channels_running(&model));
    ASSERT_FALSE(pca9685_model_restart_too_soon(&model));
    ASSERT_EQ(pca9685_model_get_channel_off(&model, 0), 2048);
}

/* Table 5 p.14: "0* Restart disabled." A second sleep must not clear the bit
 * that the first one left, or wake restarts nothing. */
TEST(test_second_sleep_keeps_the_restart)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    ASSERT_EQ_HEX(restart_bit(), PCA9685_MODE1_RESTART);

    ASSERT_EQ(pca9685_wake(&dev), PCA9685_OK);
    ASSERT_TRUE(pca9685_model_channels_running(&model));
    ASSERT_EQ(restart_bit(), 0);
}

/* p.15: "The SLEEP bit must be logic 0 for at least 500 us, before a logic 1
 * is written into the RESTART bit." */
TEST(test_restart_inside_500us_is_caught)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    uint8_t awake = (uint8_t)(mode1 & ~(PCA9685_MODE1_SLEEP |
                                        PCA9685_MODE1_RESTART));
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1, awake));
    pca9685_model_advance_us(&model, 499u);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           (uint8_t)(awake | PCA9685_MODE1_RESTART)));
    ASSERT_TRUE(pca9685_model_restart_too_soon(&model));
    ASSERT_FALSE(pca9685_model_channels_running(&model));
}

TEST(test_restart_after_500us_runs)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    uint8_t awake = (uint8_t)(mode1 & ~(PCA9685_MODE1_SLEEP |
                                        PCA9685_MODE1_RESTART));
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1, awake));
    pca9685_model_advance_us(&model, 500u);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           (uint8_t)(awake | PCA9685_MODE1_RESTART)));
    ASSERT_FALSE(pca9685_model_restart_too_soon(&model));
    ASSERT_TRUE(pca9685_model_channels_running(&model));
}

/* One write that clears SLEEP and sets RESTART has no 500 us between them. */
TEST(test_restart_in_the_waking_write_is_caught)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);

    uint8_t mode1 = chip_read(PCA9685_REG_MODE1);
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           (uint8_t)(mode1 & ~PCA9685_MODE1_SLEEP)));
    ASSERT_TRUE(pca9685_model_restart_too_soon(&model));
    ASSERT_FALSE(pca9685_model_channels_running(&model));
}

/* A delay that does not delay: the driver's wait has to be real. */
static void no_wait(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
}

TEST(test_model_sees_a_wait_that_does_not_happen)
{
    setup();
    pca9685_i2c_t hal = model_i2c;
    hal.delay_ms = no_wait;

    pca9685_t dev;
    ASSERT_EQ(pca9685_init(&dev, &hal, PCA9685_ADDR_DEFAULT, 50), PCA9685_OK);
    ASSERT_TRUE(pca9685_model_restart_too_soon(&model));
}

/* p.15: "if the user does an orderly shutdown of all the PWM channels before
 * setting the SLEEP bit, the RESTART bit will be cleared." */
TEST(test_orderly_shutdown_leaves_restart_clear)
{
    setup();
    pca9685_t dev;
    pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 2048), PCA9685_OK);
    ASSERT_EQ(pca9685_all_off(&dev), PCA9685_OK);

    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    pca9685_model_advance_us(&model, 2u * REFRESH_50HZ_US);
    ASSERT_EQ(restart_bit(), 0);
}

/* p.15: "If the MODE2 OCH bit is logic 0, write to any PWM register then issue
 * an I2C-bus STOP." is one more action that clears the bit. */
TEST(test_pwm_write_clears_restart)
{
    setup();
    pca9685_t dev;
    sleep_with_channel_active(&dev);
    ASSERT_EQ_HEX(restart_bit(), PCA9685_MODE1_RESTART);

    ASSERT_TRUE(chip_write(0x0A, 0x00));
    ASSERT_EQ(restart_bit(), 0);
}

/* p.14: "This bit is a 'sticky bit', that is, it cannot be cleared by writing
 * a logic 0 to it." */
TEST(test_extclk_is_sticky)
{
    setup();
    ASSERT_TRUE(chip_write(PCA9685_REG_MODE1,
                           PCA9685_MODE1_SLEEP | PCA9685_MODE1_EXTCLK));
    pca9685_t dev;
    ASSERT_EQ(pca9685_init(&dev, &model_i2c, PCA9685_ADDR_DEFAULT, 50),
              PCA9685_OK);
    ASSERT_EQ_HEX(chip_read(PCA9685_REG_MODE1) & PCA9685_MODE1_EXTCLK,
                  PCA9685_MODE1_EXTCLK);
}

TEST_MAIN("pca9685 hardware model")
    RUN(test_por_register_values);
    RUN(test_por_channels_are_full_off);
    RUN(test_read_only_bits_cannot_be_written);
    RUN(test_reserved_addresses_are_not_acknowledged);
    RUN(test_init_succeeds);
    RUN(test_init_sets_prescaler);
    RUN(test_init_sleep_before_prescale);
    RUN(test_set_freq_updates_prescaler);
    RUN(test_prescale_is_blocked_while_awake);
    RUN(test_prescale_hardware_minimum);
    RUN(test_set_pwm_writes_channel_regs);
    RUN(test_set_pwm_clears_full_off);
    RUN(test_set_duty_50_percent);
    RUN(test_set_duty_0_percent_full_off);
    RUN(test_set_duty_100_percent_full_on);
    RUN(test_full_off_beats_full_on);
    RUN(test_all_led_fills_every_channel);
    RUN(test_all_led_reads_zero);
    RUN(test_all_off_reaches_every_channel);
    RUN(test_sleep_sets_bit);
    RUN(test_wake_clears_sleep_bit);
    RUN(test_restart_sets_at_end_of_refresh_cycle);
    RUN(test_written_one_clears_restart);
    RUN(test_written_zero_leaves_restart);
    RUN(test_restart_cannot_be_set_by_writing);
    RUN(test_wake_restarts_the_channels);
    RUN(test_second_sleep_keeps_the_restart);
    RUN(test_restart_inside_500us_is_caught);
    RUN(test_restart_after_500us_runs);
    RUN(test_restart_in_the_waking_write_is_caught);
    RUN(test_model_sees_a_wait_that_does_not_happen);
    RUN(test_orderly_shutdown_leaves_restart_clear);
    RUN(test_pwm_write_clears_restart);
    RUN(test_extclk_is_sticky);
TEST_MAIN_END()
