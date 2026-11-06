// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "veml7700.h"

/* ---------- register layout (mirror driver internals for verification) --- */
#define REG_ALS_CONF   0x00u
#define REG_ALS_DATA   0x04u
#define REG_WHITE_DATA 0x05u

/* Each register is 16-bit LE, so register R takes two bytes of mock_regs:
 * mock_regs[2 * R]     = low byte
 * mock_regs[2 * R + 1] = high byte
 */

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
}

/* Set a 16-bit LE value at a register address in mock_regs */
static void mock_set_reg16(uint8_t reg, uint16_t val)
{
    mock_regs[reg * 2u]      = (uint8_t)(val & 0xFFu);
    mock_regs[reg * 2u + 1u] = (uint8_t)((val >> 8u) & 0xFFu);
}

static uint16_t mock_get_reg16(uint8_t reg)
{
    return (uint16_t)((uint16_t)mock_regs[reg * 2u + 1u] << 8u |
           (uint16_t)mock_regs[reg * 2u]);
}

/* The shared mock counts in bytes, the device in 16-bit command codes */
static bool mock_write_regs16(void *ctx, uint8_t dev_addr, uint8_t reg,
                              const uint8_t *buf, uint16_t len)
{
    return mock_i2c_write_regs(ctx, dev_addr, (uint8_t)(reg * 2u), buf, len);
}

static bool mock_read_regs16(void *ctx, uint8_t dev_addr, uint8_t reg,
                             uint8_t *buf, uint16_t len)
{
    return mock_i2c_read_regs(ctx, dev_addr, (uint8_t)(reg * 2u), buf, len);
}

static const veml7700_i2c_t mock_i2c = {
    .write_regs = mock_write_regs16,
    .read_regs  = mock_read_regs16,
    .ctx        = NULL,
};

static veml7700_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
}

TEST(test_init_writes_config_register)
{
    mock_reset();
    /* gain=2 (bits 12:11 = 01), IT=200ms (bits 9:6 = 0001), SD=0 */
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_2, VEML7700_IT_200MS),
              VEML7700_OK);

    uint16_t conf = mock_get_reg16(REG_ALS_CONF);
    /* gain=1 at bits 12:11 -> 0x0800, IT=1 at bits 9:6 -> 0x0040 */
    uint16_t expected = (uint16_t)((1u << 11u) | (1u << 6u));
    ASSERT_EQ(conf, expected);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_ERR_I2C);
}

TEST(test_null_dev)
{
    ASSERT_EQ(veml7700_init(NULL, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_ERR_PARAM);
}

TEST(test_null_i2c)
{
    ASSERT_EQ(veml7700_init(&dev, NULL, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_ERR_PARAM);
}

TEST(test_read_als_raw)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    mock_set_reg16(REG_ALS_DATA, 1000);
    uint16_t raw = 0;
    ASSERT_EQ(veml7700_read_als(&dev, &raw), VEML7700_OK);
    ASSERT_EQ(raw, 1000);
}

TEST(test_read_als_null_out)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
    ASSERT_EQ(veml7700_read_als(&dev, NULL), VEML7700_ERR_PARAM);
}

TEST(test_read_white_raw)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    mock_set_reg16(REG_WHITE_DATA, 2000);
    uint16_t raw = 0;
    ASSERT_EQ(veml7700_read_white(&dev, &raw), VEML7700_OK);
    ASSERT_EQ(raw, 2000);
}

/* p.6: "six 16 bit command codes", so ALS at 04h and WHITE at 05h do not share a
 * byte. p.7: each is its own 16-bit register. */
TEST(test_als_and_white_are_separate_registers)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
    mock_set_reg16(REG_ALS_DATA, 0x1234);
    mock_set_reg16(REG_WHITE_DATA, 0xABCD);

    uint16_t als = 0;
    uint16_t white = 0;
    ASSERT_EQ(veml7700_read_als(&dev, &als), VEML7700_OK);
    ASSERT_EQ(veml7700_read_white(&dev, &white), VEML7700_OK);
    ASSERT_EQ_HEX(als, 0x1234);
    ASSERT_EQ_HEX(white, 0xABCD);
}

TEST(test_read_lux_gain1_it100ms)
{
    /* gain=1, IT=100ms -> resolution = 0.0672 lux/bit */
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    mock_set_reg16(REG_ALS_DATA, 1000);
    float lux = 0.0f;
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_OK);
    /* 1000 x 0.0672 = 67.2 */
    float expected = 1000.0f * 0.0672f;
    float diff = lux - expected;
    if (diff < 0) diff = -diff;
    ASSERT_TRUE(diff < 0.01f);
}

TEST(test_read_lux_gain2_it800ms)
{
    /* gain=2, IT=800ms -> 0.0042 lux/bit, as printed in the datasheet */
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_2, VEML7700_IT_800MS),
              VEML7700_OK);

    mock_set_reg16(REG_ALS_DATA, 5000);
    float lux = 0.0f;
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_OK);
    float expected = 5000.0f * 0.0042f;
    float diff = lux - expected;
    if (diff < 0) diff = -diff;
    ASSERT_TRUE(diff < 0.01f);
}

TEST(test_read_lux_gain1_8_it25ms)
{
    /* gain=1/8, IT=25ms -> 2.1504 lux/bit: 512 x the 0.0042 at gain=2, IT=800ms */
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1_8, VEML7700_IT_25MS),
              VEML7700_OK);

    mock_set_reg16(REG_ALS_DATA, 100);
    float lux = 0.0f;
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_OK);
    float expected = 100.0f * 2.1504f;
    float diff = lux - expected;
    if (diff < 0) diff = -diff;
    ASSERT_TRUE(diff < 0.1f);
}

TEST(test_read_lux_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
    mock_fail_next = 1;
    float lux = 0.0f;
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_ERR_I2C);
}

TEST(test_set_gain)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    ASSERT_EQ(veml7700_set_gain(&dev, VEML7700_GAIN_1_4), VEML7700_OK);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);
    /* gain=3 (1/4) at bits 12:11 -> 0x1800, IT=0 -> 0, SD=0 */
    uint16_t expected = (uint16_t)(3u << 11u);
    ASSERT_EQ(conf, expected);
}

TEST(test_set_integration_time)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    ASSERT_EQ(veml7700_set_integration_time(&dev, VEML7700_IT_400MS), VEML7700_OK);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);
    /* gain=0, IT=2 at bits 9:6 -> 0x0080, SD=0 */
    uint16_t expected = (uint16_t)(2u << 6u);
    ASSERT_EQ(conf, expected);
}

TEST(test_shutdown_sets_sd_bit)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    ASSERT_EQ(veml7700_shutdown(&dev), VEML7700_OK);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);
    /* SD bit (bit 0) should be set */
    ASSERT_TRUE((conf & 0x01u) != 0u);
}

TEST(test_power_on_clears_sd_bit)
{
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);

    ASSERT_EQ(veml7700_shutdown(&dev), VEML7700_OK);
    ASSERT_EQ(veml7700_power_on(&dev), VEML7700_OK);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);
    /* SD bit should be cleared */
    ASSERT_TRUE((conf & 0x01u) == 0u);
}

/* ---------- a failed write leaves the chip's setting in force ---------- */

/* gain=1/8, IT=100ms -> 0.5376 lux/bit: 128 x the 0.0042 at gain=2, IT=800ms */
TEST(test_failed_set_gain_keeps_the_scale)
{
    veml7700_t dev;
    float lux = 0.0f;
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1_8, VEML7700_IT_100MS),
              VEML7700_OK);
    mock_set_reg16(REG_ALS_DATA, 1000);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);

    mock_fail_next = 1;
    ASSERT_EQ(veml7700_set_gain(&dev, VEML7700_GAIN_2), VEML7700_ERR_I2C);

    ASSERT_EQ(mock_get_reg16(REG_ALS_CONF), conf);
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_OK);
    ASSERT_NEAR(lux, 537.6f, 0.1f);
}

/* gain=1, IT=100ms -> 0.0672 lux/bit */
TEST(test_failed_set_integration_time_keeps_the_scale)
{
    veml7700_t dev;
    float lux = 0.0f;
    mock_reset();
    ASSERT_EQ(veml7700_init(&dev, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_OK);
    mock_set_reg16(REG_ALS_DATA, 1000);
    uint16_t conf = mock_get_reg16(REG_ALS_CONF);

    mock_fail_next = 1;
    ASSERT_EQ(veml7700_set_integration_time(&dev, VEML7700_IT_800MS),
              VEML7700_ERR_I2C);

    ASSERT_EQ(mock_get_reg16(REG_ALS_CONF), conf);
    ASSERT_EQ(veml7700_read_lux(&dev, &lux), VEML7700_OK);
    ASSERT_NEAR(lux, 67.2f, 0.1f);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(veml7700_t *d)
{
    uint16_t raw = 0;
    float lux = 0.0f;
    ASSERT_EQ(veml7700_read_als(d, &raw), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_read_white(d, &raw), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_read_lux(d, &lux), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_set_gain(d, VEML7700_GAIN_1), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_set_integration_time(d, VEML7700_IT_100MS),
              VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_power_on(d), VEML7700_ERR_PARAM);
    ASSERT_EQ(veml7700_shutdown(d), VEML7700_ERR_PARAM);
}

TEST(test_device_refused_after_failed_init)
{
    veml7700_t d;
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(veml7700_init(&d, &mock_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_ERR_I2C);
    assert_device_refused(&d);
}

TEST(test_zeroed_device_refused)
{
    static veml7700_t zeroed;
    assert_device_refused(&zeroed);
}

/* d holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    veml7700_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(veml7700_init(&d, NULL, VEML7700_GAIN_1, VEML7700_IT_100MS),
              VEML7700_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(veml7700_init(&d, &mock_i2c, (veml7700_gain_t)7, VEML7700_IT_100MS),
              VEML7700_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("veml7700")
    RUN(test_init_ok);
    RUN(test_init_writes_config_register);
    RUN(test_init_i2c_fail);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_read_als_raw);
    RUN(test_read_als_null_out);
    RUN(test_read_white_raw);
    RUN(test_als_and_white_are_separate_registers);
    RUN(test_read_lux_gain1_it100ms);
    RUN(test_read_lux_gain2_it800ms);
    RUN(test_read_lux_gain1_8_it25ms);
    RUN(test_read_lux_i2c_fail);
    RUN(test_set_gain);
    RUN(test_set_integration_time);
    RUN(test_shutdown_sets_sd_bit);
    RUN(test_power_on_clears_sd_bit);
    RUN(test_failed_set_gain_keeps_the_scale);
    RUN(test_failed_set_integration_time_keeps_the_scale);
    RUN(test_device_refused_after_failed_init);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
