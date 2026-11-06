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
    /* Table 7: every channel resets to full OFF */
    for (uint8_t ch = 0; ch < 16u; ch++)
        mock_regs[REG_LED0_ON_L + ch * 4u + 3u] = FULL_ON_BIT;
}

/* p.14: "User writes logic 1 to this bit to clear it to logic 0. A user write
 * of logic 0 will have no effect." */
static bool chip_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    uint8_t restart = (uint8_t)(mock_regs[REG_MODE1] & MODE1_RESTART);
    if (!mock_i2c_write_reg(ctx, dev_addr, reg, val))
        return false;
    if (reg == REG_MODE1)
        mock_regs[reg] = (uint8_t)((val & ~MODE1_RESTART) | (restart & ~val));
    return true;
}

static const pca9685_i2c_t mock_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_delay_ms,
    .ctx       = NULL,
};

/* MODE1 writes in order, each with the delay total at that moment */
#define MODE1_LOG_MAX 8
static uint8_t  mode1_log[MODE1_LOG_MAX];
static uint32_t mode1_log_ms[MODE1_LOG_MAX];
static int      mode1_log_len;

static bool log_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    if (reg == REG_MODE1 && mode1_log_len < MODE1_LOG_MAX) {
        mode1_log[mode1_log_len] = val;
        mode1_log_ms[mode1_log_len] = delay_total_ms;
        mode1_log_len++;
    }
    return chip_write_reg(ctx, dev_addr, reg, val);
}

static const pca9685_i2c_t log_i2c = {
    .write_reg = log_write_reg,
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
    /* p.14: "No PWM control is possible when the oscillator is off" */
    ASSERT_FALSE(mock_regs[REG_MODE1] & MODE1_SLEEP);
}

TEST(test_set_freq_leaves_chip_awake)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_OK);
    ASSERT_FALSE(mock_regs[REG_MODE1] & MODE1_SLEEP);
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

/* p.15: "Allow time for oscillator to stabilize (500 us)", and the only way
 * to wait is the delay callback. */
TEST(test_init_null_delay)
{
    mock_reset();
    pca9685_i2c_t no_delay = { .write_reg = mock_i2c_write_reg,
                               .read_regs = mock_i2c_read_regs,
                               .delay_ms  = NULL, .ctx = NULL };
    ASSERT_EQ(pca9685_init(&dev, &no_delay, 0x40, 50), PCA9685_ERR_PARAM);
    ASSERT_EQ(mock_write_count, 0);
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

/* p.25: "The LEDn_ON and LEDn_OFF count registers should never be programmed
 * with the same values." */
TEST(test_set_pwm_equal_counts_rejected)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);
    int writes = mock_write_count;

    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 0, 0), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 100, 100), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_pwm(&dev, 0, 4095, 4095), PCA9685_ERR_PARAM);
    ASSERT_EQ(mock_write_count, writes);
}

/* Below half a count of 4096, the count rounds to 0 and would equal ON. */
TEST(test_set_duty_tiny_is_full_off)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 2, 0.001f), PCA9685_OK);
    /* Channel 2: base = 0x06 + 2*4 = 0x0E, OFF_H = 0x11 */
    ASSERT_EQ(mock_regs[0x11] & FULL_ON_BIT, FULL_ON_BIT);
}

TEST(test_set_servo_zero_pulse_is_full_off)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_servo_us(&dev, 2, 0), PCA9685_OK);
    ASSERT_EQ(mock_regs[0x11] & FULL_ON_BIT, FULL_ON_BIT);
    ASSERT_EQ(pca9685_set_servo_us(&dev, 16, 0), PCA9685_ERR_PARAM);
}

TEST(test_set_duty_50_percent)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 50.0f), PCA9685_OK);
    /* 50% of 4096 = 2048 */
    uint16_t off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 2048);
}

/* p.17 counts percent of 4096: "LED on time = 20 % = 819.2 ~ 819 counts" and
 * "LED on time = 90 % = 3686 counts". On that basis 75 % is 3072. */
TEST(test_set_duty_counts_of_4096)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 20.0f), PCA9685_OK);
    uint16_t off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 819);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 90.0f), PCA9685_OK);
    off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 3686);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, 75.0f), PCA9685_OK);
    off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 3072);

    /* rounds to 4096, which the 12-bit register cannot hold */
    ASSERT_EQ(pca9685_set_duty(&dev, 0, 99.99f), PCA9685_OK);
    off = (uint16_t)(mock_regs[0x08] | ((uint16_t)(mock_regs[0x09] & 0x0F) << 8));
    ASSERT_EQ(off, 4095);
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

    /* Wake: MODE1 SLEEP bit should be cleared. p.14: RESTART reads 0 after
     * wake, as "User writes logic 1 to this bit to clear it to logic 0." */
    ASSERT_EQ(pca9685_wake(&dev), PCA9685_OK);
    ASSERT_FALSE(mock_regs[REG_MODE1] & MODE1_SLEEP);
    ASSERT_FALSE(mock_regs[REG_MODE1] & MODE1_RESTART);
}

/* p.14: the same rule for a bit that reads 1, and a written 0 leaves it. */
TEST(test_mock_restart_follows_table_5)
{
    mock_reset();
    mock_regs[REG_MODE1] = 0x91;

    ASSERT_TRUE(mock_i2c.write_reg(NULL, 0x40, REG_MODE1, 0x11));
    ASSERT_EQ_HEX(mock_regs[REG_MODE1], 0x91);

    ASSERT_TRUE(mock_i2c.write_reg(NULL, 0x40, REG_MODE1, 0x91));
    ASSERT_EQ_HEX(mock_regs[REG_MODE1], 0x11);
}

/* Table 7 p.21: "LED0_OFF_H[4] R/W 1* LED0 full OFF", so a channel must leave
 * full OFF on the way to full ON or a PWM count. */
TEST(test_mock_channels_reset_to_full_off)
{
    mock_reset();
    for (uint8_t ch = 0; ch < 16u; ch++)
        ASSERT_EQ_HEX(mock_regs[REG_LED0_ON_L + ch * 4u + 3u], FULL_ON_BIT);
}

/* p.15: a sleep with channels active leaves RESTART at 1, and "The SLEEP bit
 * must be logic 0 for at least 500 us, before a logic 1 is written into the
 * RESTART bit." 0xB1 is RESTART, AI, SLEEP and ALLCALL. */
TEST(test_wake_writes_restart_after_500us)
{
    mock_reset();
    pca9685_init(&dev, &log_i2c, 0x40, 50);
    mock_regs[REG_MODE1] = 0xB1;
    mode1_log_len = 0;

    ASSERT_EQ(pca9685_wake(&dev), PCA9685_OK);
    ASSERT_EQ(mode1_log_len, 2);
    ASSERT_EQ(mode1_log[0] & (MODE1_SLEEP | MODE1_RESTART), 0);
    ASSERT_EQ(mode1_log[1] & (MODE1_SLEEP | MODE1_RESTART), MODE1_RESTART);
    ASSERT_GE(mode1_log_ms[1] - mode1_log_ms[0], 1);
}

TEST(test_set_freq_writes_restart_after_500us)
{
    mock_reset();
    pca9685_init(&dev, &log_i2c, 0x40, 50);
    mock_regs[REG_MODE1] = 0xB1;
    mode1_log_len = 0;

    ASSERT_EQ(pca9685_set_freq(&dev, 100), PCA9685_OK);
    ASSERT_EQ(mode1_log_len, 3);
    ASSERT_EQ(mode1_log[1] & (MODE1_SLEEP | MODE1_RESTART), 0);
    ASSERT_EQ(mode1_log[2] & (MODE1_SLEEP | MODE1_RESTART), MODE1_RESTART);
    ASSERT_GE(mode1_log_ms[2] - mode1_log_ms[1], 1);
}

/* p.14: "User writes logic 1 to this bit to clear it to logic 0. A user write
 * of logic 0 will have no effect." A sleep that finds RESTART set, as a second
 * sleep does, must write 0 there or wake has nothing left to restart. 0xB1 is
 * RESTART, AI, SLEEP and ALLCALL. */
TEST(test_sleep_writes_restart_as_zero)
{
    mock_reset();
    pca9685_init(&dev, &log_i2c, 0x40, 50);
    mock_regs[REG_MODE1] = 0xB1;
    mode1_log_len = 0;

    ASSERT_EQ(pca9685_sleep(&dev), PCA9685_OK);
    ASSERT_EQ(mode1_log_len, 1);
    ASSERT_EQ(mode1_log[0] & MODE1_RESTART, 0);
    ASSERT_EQ(mode1_log[0] & MODE1_SLEEP, MODE1_SLEEP);
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

TEST(test_set_duty_nan_rejected)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    /* NaN must be rejected -- previously bypassed the < 0 / > 100 guard */
    float nan_val = 0.0f / 0.0f;
    ASSERT_EQ(pca9685_set_duty(&dev, 0, nan_val), PCA9685_ERR_PARAM);
}

TEST(test_set_duty_negative_rejected)
{
    mock_reset();
    pca9685_init(&dev, &mock_i2c, 0x40, 50);

    ASSERT_EQ(pca9685_set_duty(&dev, 0, -1.0f), PCA9685_ERR_PARAM);
    ASSERT_EQ(pca9685_set_duty(&dev, 0, -0.001f), PCA9685_ERR_PARAM);
}

TEST_MAIN("pca9685")
    RUN(test_init_ok);
    RUN(test_set_freq_leaves_chip_awake);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_callbacks);
    RUN(test_init_null_delay);
    RUN(test_init_freq_too_low);
    RUN(test_init_freq_too_high);
    RUN(test_set_pwm_channel0);
    RUN(test_set_pwm_channel15);
    RUN(test_set_pwm_channel_out_of_range);
    RUN(test_set_pwm_equal_counts_rejected);
    RUN(test_set_duty_tiny_is_full_off);
    RUN(test_set_servo_zero_pulse_is_full_off);
    RUN(test_set_duty_50_percent);
    RUN(test_set_duty_counts_of_4096);
    RUN(test_set_duty_0_percent);
    RUN(test_set_duty_100_percent);
    RUN(test_set_duty_nan_rejected);
    RUN(test_set_duty_negative_rejected);
    RUN(test_set_servo_us);
    RUN(test_all_off);
    RUN(test_set_freq_prescaler_calc);
    RUN(test_set_freq_range_validation);
    RUN(test_sleep_wake);
    RUN(test_mock_restart_follows_table_5);
    RUN(test_mock_channels_reset_to_full_off);
    RUN(test_wake_writes_restart_after_500us);
    RUN(test_set_freq_writes_restart_after_500us);
    RUN(test_sleep_writes_restart_as_zero);
    RUN(test_i2c_failure);
TEST_MAIN_END()
