// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "tcs34725.h"
#include <math.h>

/*
 * TCS34725 command register: real driver ORs 0x80 with register address.
 * The mock sees the CMD-encoded register, so we set mock_regs at the
 * CMD-encoded offset: 0x80 | raw_reg.
 */
#define CMD(r) (0x80u | (r))

/* TYPE 01b (p.14): the auto-increment form a multi-byte read needs. The
 * mock steps on any read, so the data sits at the address this gives. */
#define CMD_AUTO_INC(r) (0xA0u | (r))

#define REG_ENABLE  0x00u
#define REG_ATIME   0x01u
#define REG_CONTROL 0x0Fu
#define REG_ID      0x12u
#define REG_STATUS  0x13u
#define REG_CDATAL  0x14u

static tcs34725_t dev;

static const tcs34725_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Default chip ID = 0x44 at CMD-encoded address */
    mock_regs[CMD(REG_ID)] = 0x44;
    /* AVALID: these tests start after an integration cycle has finished */
    mock_regs[CMD(REG_STATUS)] = 0x01;
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_101MS, TCS34725_GAIN_4X), TCS34725_OK);
    /* Verify integration time written to CMD(ATIME) */
    ASSERT_EQ_HEX(mock_regs[CMD(REG_ATIME)], 0xD5);
    /* Verify gain written to CMD(CONTROL) */
    ASSERT_EQ_HEX(mock_regs[CMD(REG_CONTROL)], 0x01);
    /* Verify enable (PON | AEN = 0x03) */
    ASSERT_EQ_HEX(mock_regs[CMD(REG_ENABLE)], 0x03);
}

TEST(test_init_alt_id)
{
    mock_reset();
    mock_regs[CMD(REG_ID)] = 0x4D;
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_OK);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_regs[CMD(REG_ID)] = 0xFF;
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_ID);
}

TEST(test_init_null)
{
    ASSERT_EQ(tcs34725_init(NULL, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_init(&dev, NULL,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* fail the first I2C operation (read ID) */
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
}

TEST(test_init_refuses_missing_delay)
{
    mock_reset();
    tcs34725_i2c_t no_delay = mock_i2c;
    no_delay.delay_ms = NULL;
    ASSERT_EQ(tcs34725_init(&dev, &no_delay,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);
}

TEST(test_init_pon_write_fail)
{
    mock_reset();
    mock_fail_at = 3;  /* ID, ATIME, CONTROL, then PON */
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
    ASSERT_EQ(mock_delay_ms_count, 0);
}

TEST(test_init_aen_write_fail)
{
    mock_reset();
    mock_fail_at = 4;  /* ID, ATIME, CONTROL, PON, then PON | AEN */
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
    ASSERT_EQ(mock_delay_ms_count, 1);
}

TEST(test_init_bad_gain)
{
    mock_reset();
    ASSERT_EQ(tcs34725_init(&dev, &mock_i2c,
              TCS34725_INTEG_24MS, (tcs34725_gain_t)99), TCS34725_ERR_PARAM);
}

/* ---------- read RGBC ---------- */

TEST(test_read_rgbc)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);

    /* Set RGBC data: C=1000 R=500 G=600 B=300 (16-bit LE) */
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 0] = (uint8_t)(1000 & 0xFF);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 1] = (uint8_t)(1000 >> 8);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 2] = (uint8_t)(500 & 0xFF);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 3] = (uint8_t)(500 >> 8);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 4] = (uint8_t)(600 & 0xFF);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 5] = (uint8_t)(600 >> 8);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 6] = (uint8_t)(300 & 0xFF);
    mock_regs[CMD_AUTO_INC(REG_CDATAL) + 7] = (uint8_t)(300 >> 8);

    tcs34725_rgbc_t rgbc;
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_OK);
    ASSERT_EQ(rgbc.c, 1000);
    ASSERT_EQ(rgbc.r, 500);
    ASSERT_EQ(rgbc.g, 600);
    ASSERT_EQ(rgbc.b, 300);
}

/* p.19: AVALID is bit 0 of STATUS, and AINT does not stand in for it */
TEST(test_read_rgbc_not_ready)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    mock_regs[CMD(REG_STATUS)] = 0x10;  /* AINT */

    tcs34725_rgbc_t rgbc = { 1, 2, 3, 4 };
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_ERR_NOT_READY);
    ASSERT_EQ(rgbc.c, 1);
    ASSERT_EQ(rgbc.r, 2);
    ASSERT_EQ(rgbc.g, 3);
    ASSERT_EQ(rgbc.b, 4);

    mock_regs[CMD(REG_STATUS)] = 0x11;
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_OK);
}

TEST(test_read_rgbc_data_i2c_fail)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    mock_fail_at = mock_op_count + 1;  /* the STATUS read passes */

    tcs34725_rgbc_t rgbc;
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_ERR_I2C);
}

TEST(test_read_rgbc_null)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
    ASSERT_EQ(tcs34725_read_rgbc(NULL, &(tcs34725_rgbc_t){0}), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_read_rgbc(&dev, NULL), TCS34725_ERR_PARAM);
}

/* ---------- set integration time / gain ---------- */

TEST(test_set_integration_time)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
    ASSERT_EQ(tcs34725_set_integration_time(&dev, TCS34725_INTEG_700MS),
              TCS34725_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(REG_ATIME)], 0x00);
    ASSERT_EQ(tcs34725_set_integration_time(NULL, TCS34725_INTEG_700MS),
              TCS34725_ERR_PARAM);
}

TEST(test_set_gain)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
    ASSERT_EQ(tcs34725_set_gain(&dev, TCS34725_GAIN_60X), TCS34725_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(REG_CONTROL)], 0x03);
    ASSERT_EQ(tcs34725_set_gain(NULL, TCS34725_GAIN_60X), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_set_gain(&dev, (tcs34725_gain_t)99), TCS34725_ERR_PARAM);
}

/* ---------- enable / disable ---------- */

TEST(test_enable_disable)
{
    mock_reset();
    tcs34725_init(&dev, &mock_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    ASSERT_EQ(tcs34725_disable(&dev), TCS34725_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(REG_ENABLE)], 0x00);

    ASSERT_EQ(tcs34725_enable(&dev), TCS34725_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(REG_ENABLE)], 0x03);

    ASSERT_EQ(tcs34725_enable(NULL), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_disable(NULL), TCS34725_ERR_PARAM);
}

/* ---------- color temp / lux ---------- */

TEST(test_color_temp)
{
    tcs34725_rgbc_t rgbc = { .c = 1000, .r = 400, .g = 500, .b = 300 };
    uint16_t cct = 0;
    ASSERT_EQ(tcs34725_get_color_temp(&rgbc, &cct), TCS34725_OK);
    ASSERT_TRUE(cct > 0);

    /* null safety */
    ASSERT_EQ(tcs34725_get_color_temp(NULL, &cct), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_get_color_temp(&rgbc, NULL), TCS34725_ERR_PARAM);

    /* clear = 0 is invalid */
    tcs34725_rgbc_t zero_c = { .c = 0, .r = 100, .g = 100, .b = 100 };
    ASSERT_EQ(tcs34725_get_color_temp(&zero_c, &cct), TCS34725_ERR_PARAM);
}

/* DN40 p.6 and Table 1 on p.13, TCS3472 row: CT = 3810 * B' / R' + 1391 */
TEST(test_color_temp_dn40)
{
    uint16_t cct = 0;

    /* IR = (1800 + 2200 + 1000 - 5000) / 2 = 0: 3810 * 1000 / 1800 + 1391 */
    tcs34725_rgbc_t no_ir = { .c = 5000, .r = 1800, .g = 2200, .b = 1000 };
    ASSERT_EQ(tcs34725_get_color_temp(&no_ir, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 3507);

    /* IR = (400 + 500 + 300 - 1000) / 2 = 100, so R' = 300 and B' = 200:
     * 3810 * 200 / 300 + 1391 */
    tcs34725_rgbc_t with_ir = { .c = 1000, .r = 400, .g = 500, .b = 300 };
    ASSERT_EQ(tcs34725_get_color_temp(&with_ir, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 3931);

    /* The ratio has no unit: the same light at 4x the counts */
    tcs34725_rgbc_t scaled = { .c = 4000, .r = 1600, .g = 2000, .b = 1200 };
    ASSERT_EQ(tcs34725_get_color_temp(&scaled, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 3931);
}

TEST(test_color_temp_no_red_left)
{
    uint16_t cct = 7;

    /* IR = 50 takes all of R, so B' / R' has no value */
    tcs34725_rgbc_t zero_r = { .c = 100, .r = 50, .g = 100, .b = 50 };
    ASSERT_EQ(tcs34725_get_color_temp(&zero_r, &cct), TCS34725_ERR_PARAM);

    /* IR = 100 is more than R */
    tcs34725_rgbc_t neg_r = { .c = 100, .r = 10, .g = 200, .b = 90 };
    ASSERT_EQ(tcs34725_get_color_temp(&neg_r, &cct), TCS34725_ERR_PARAM);
    ASSERT_EQ(cct, 7);
}

TEST(test_color_temp_clamped)
{
    uint16_t cct = 7;

    /* IR = 30, R' = 30, B' = -30: 1391 - 3810 is below zero */
    tcs34725_rgbc_t low = { .c = 100, .r = 60, .g = 100, .b = 0 };
    ASSERT_EQ(tcs34725_get_color_temp(&low, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 0);

    /* IR = 0.5, R' = 0.5, B' = 99.5: about 760000 K */
    tcs34725_rgbc_t high = { .c = 101, .r = 1, .g = 1, .b = 100 };
    ASSERT_EQ(tcs34725_get_color_temp(&high, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 65535);
}

TEST(test_lux)
{
    tcs34725_rgbc_t rgbc = { .c = 1000, .r = 400, .g = 500, .b = 300 };
    float lux = 0.0f;
    ASSERT_EQ(tcs34725_get_lux(&rgbc, &lux), TCS34725_OK);
    /* -0.32466*400 + 1.57837*500 + -0.73191*300 = -129.86 + 789.19 - 219.57 = 439.76 */
    ASSERT_NEAR(lux, 439.76f, 1.0f);

    /* null safety */
    ASSERT_EQ(tcs34725_get_lux(NULL, &lux), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_get_lux(&rgbc, NULL), TCS34725_ERR_PARAM);
}

TEST(test_lux_clamp_negative)
{
    /* High red, low green/blue -> formula may go negative -> should clamp to 0 */
    tcs34725_rgbc_t rgbc = { .c = 500, .r = 500, .g = 10, .b = 10 };
    float lux = -1.0f;
    ASSERT_EQ(tcs34725_get_lux(&rgbc, &lux), TCS34725_OK);
    ASSERT_NEAR(lux, 0.0f, 0.01f);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(tcs34725_t *d)
{
    tcs34725_rgbc_t rgbc;
    ASSERT_EQ(tcs34725_read_rgbc(d, &rgbc), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_set_integration_time(d, TCS34725_INTEG_24MS), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_set_gain(d, TCS34725_GAIN_4X), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_enable(d), TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_disable(d), TCS34725_ERR_PARAM);
}

TEST(test_device_refused_after_wrong_id)
{
    tcs34725_t d;
    mock_reset();
    mock_regs[CMD(REG_ID)] = 0xFF;
    ASSERT_EQ(tcs34725_init(&d, &mock_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X),
              TCS34725_ERR_ID);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const tcs34725_i2c_t zeroed;
    tcs34725_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tcs34725_init(&d, NULL, TCS34725_INTEG_24MS, TCS34725_GAIN_1X),
              TCS34725_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tcs34725_init(&d, &zeroed, TCS34725_INTEG_24MS, TCS34725_GAIN_1X),
              TCS34725_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("tcs34725")
    RUN(test_init_ok);
    RUN(test_init_alt_id);
    RUN(test_init_bad_id);
    RUN(test_init_null);
    RUN(test_init_i2c_fail);
    RUN(test_init_refuses_missing_delay);
    RUN(test_init_pon_write_fail);
    RUN(test_init_aen_write_fail);
    RUN(test_init_bad_gain);
    RUN(test_read_rgbc);
    RUN(test_read_rgbc_not_ready);
    RUN(test_read_rgbc_data_i2c_fail);
    RUN(test_read_rgbc_null);
    RUN(test_set_integration_time);
    RUN(test_set_gain);
    RUN(test_enable_disable);
    RUN(test_color_temp);
    RUN(test_color_temp_dn40);
    RUN(test_color_temp_no_red_left);
    RUN(test_color_temp_clamped);
    RUN(test_lux);
    RUN(test_lux_clamp_negative);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
