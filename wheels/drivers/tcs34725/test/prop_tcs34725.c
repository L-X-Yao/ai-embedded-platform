// SPDX-License-Identifier: MIT
/*
 * prop_tcs34725.c — property-based tests for TCS34725 color sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "tcs34725.h"

static const tcs34725_i2c_t prop_i2c = {
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
    mock_regs[0x12] = 0x44; /* chip ID */

    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c,
                  TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x12] = 0x44;

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &prop_i2c,
        (tcs34725_integ_t)(ARBI_UINT8(seed)),
        (tcs34725_gain_t)(ARBI_UINT8(seed) % 4));
    if (err != TCS34725_OK) return;

    tcs34725_rgbc_t rgbc;
    tcs34725_read_rgbc(&dev, &rgbc);

    float lux;
    tcs34725_get_lux(&rgbc, &lux);
    uint16_t cct;
    tcs34725_get_color_temp(&rgbc, &cct);
}

PROPERTY(config_functions_no_crash, 2000)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44; /* chip ID at CMD(0x12) */

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &prop_i2c,
        TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
    if (err != TCS34725_OK) return;

    tcs34725_gain_t gain = (tcs34725_gain_t)(ARBI_UINT8(seed) % 4);
    tcs34725_set_gain(&dev, gain);

    tcs34725_integ_t integ = (tcs34725_integ_t)(ARBI_UINT8(seed));
    tcs34725_set_integration_time(&dev, integ);

    tcs34725_disable(&dev);
    tcs34725_enable(&dev);
}

PROPERTY(color_math_no_crash, 5000)
{
    tcs34725_rgbc_t rgbc;
    rgbc.c = ARBI_UINT16(seed);
    rgbc.r = ARBI_UINT16(seed);
    rgbc.g = ARBI_UINT16(seed);
    rgbc.b = ARBI_UINT16(seed);

    float lux = 0.0f;
    tcs34725_get_lux(&rgbc, &lux);

    uint16_t cct = 0;
    tcs34725_get_color_temp(&rgbc, &cct);
}

TEST(test_init_i2c_fail_id_read)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    /* fail on the very first I2C op (read ID = op 0) */
    mock_fail_at = 0;
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &prop_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
}

TEST(test_init_i2c_fail_atime)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    /* fail on write to ATIME (3rd I2C op: read_ID=0, write_ATIME=1) */
    mock_fail_at = 1;
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &prop_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
}

TEST(test_init_i2c_fail_control)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    /* fail on write to CONTROL (op index 2) */
    mock_fail_at = 2;
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &prop_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
}

TEST(test_init_i2c_fail_enable)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    /* fail on write to ENABLE (op index 3) */
    mock_fail_at = 3;
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &prop_i2c,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_I2C);
}

TEST(test_read_rgbc_i2c_fail)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    mock_fail_next = 1;
    tcs34725_rgbc_t rgbc;
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_ERR_I2C);
}

TEST(test_set_integration_time_i2c_fail)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    mock_fail_next = 1;
    ASSERT_EQ(tcs34725_set_integration_time(&dev, TCS34725_INTEG_700MS),
              TCS34725_ERR_I2C);
}

TEST(test_set_gain_i2c_fail)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    mock_fail_next = 1;
    ASSERT_EQ(tcs34725_set_gain(&dev, TCS34725_GAIN_60X), TCS34725_ERR_I2C);
}

TEST(test_enable_i2c_fail)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    mock_fail_next = 1;
    ASSERT_EQ(tcs34725_enable(&dev), TCS34725_ERR_I2C);
}

TEST(test_disable_i2c_fail)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);

    mock_fail_next = 1;
    ASSERT_EQ(tcs34725_disable(&dev), TCS34725_ERR_I2C);
}

TEST(test_color_temp_cct_clamped)
{
    /* Force the CCT formula to produce a negative or huge value */
    /* Use values that push cct negative: B' below zero, IR = 30 */
    tcs34725_rgbc_t rgbc = { .c = 100, .r = 60, .g = 100, .b = 0 };
    uint16_t cct = 1;
    ASSERT_EQ(tcs34725_get_color_temp(&rgbc, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 0);

    /* Use values that push cct >= 65535: R' = 0.5, B' = 99.5 */
    tcs34725_rgbc_t rgbc2 = { .c = 101, .r = 1, .g = 1, .b = 100 };
    uint16_t cct2 = 0;
    ASSERT_EQ(tcs34725_get_color_temp(&rgbc2, &cct2), TCS34725_OK);
    ASSERT_EQ(cct2, 65535);
}

TEST(test_lux_all_zeros)
{
    tcs34725_rgbc_t rgbc = { .c = 0, .r = 0, .g = 0, .b = 0 };
    float lux = -1.0f;
    ASSERT_EQ(tcs34725_get_lux(&rgbc, &lux), TCS34725_OK);
    ASSERT_NEAR(lux, 0.0f, 0.001f);
}

TEST(test_init_null_callbacks)
{
    tcs34725_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &bad,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, NULL,
              TCS34725_INTEG_24MS, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);
}

TEST(test_init_bad_gain_over_range)
{
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    ASSERT_EQ(tcs34725_init(&dev, &prop_i2c,
              TCS34725_INTEG_24MS, (tcs34725_gain_t)99), TCS34725_ERR_PARAM);
}

TEST(test_null_guards_api)
{
    /* read_rgbc null out */
    mock_i2c_reset();
    mock_regs[0x92] = 0x44;
    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c, TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
    ASSERT_EQ(tcs34725_read_rgbc(&dev, NULL), TCS34725_ERR_PARAM);

    /* set_integration_time null dev */
    ASSERT_EQ(tcs34725_set_integration_time(NULL, TCS34725_INTEG_24MS),
              TCS34725_ERR_PARAM);

    /* set_gain null dev */
    ASSERT_EQ(tcs34725_set_gain(NULL, TCS34725_GAIN_1X), TCS34725_ERR_PARAM);

    /* set_gain bad gain */
    ASSERT_EQ(tcs34725_set_gain(&dev, (tcs34725_gain_t)99), TCS34725_ERR_PARAM);

    /* enable/disable null dev */
    ASSERT_EQ(tcs34725_enable(NULL),  TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_disable(NULL), TCS34725_ERR_PARAM);

    /* get_color_temp null args */
    uint16_t cct = 0;
    ASSERT_EQ(tcs34725_get_color_temp(NULL, &cct),              TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_get_color_temp(&(tcs34725_rgbc_t){0}, NULL), TCS34725_ERR_PARAM);

    /* get_color_temp c == 0 */
    tcs34725_rgbc_t zero_c = { .c = 0, .r = 100, .g = 100, .b = 100 };
    ASSERT_EQ(tcs34725_get_color_temp(&zero_c, &cct), TCS34725_ERR_PARAM);

    /* get_lux null args */
    float lux = 0.0f;
    ASSERT_EQ(tcs34725_get_lux(NULL, &lux),              TCS34725_ERR_PARAM);
    ASSERT_EQ(tcs34725_get_lux(&(tcs34725_rgbc_t){0}, NULL), TCS34725_ERR_PARAM);
}

TEST(test_color_temp_denom_near_zero)
{
    /* The denominator is R' = R - IR, with IR = (R + G + B - C) / 2 */
    tcs34725_rgbc_t rgbc = { .c = 100, .r = 50, .g = 100, .b = 50 };
    uint16_t cct = 0;
    /* R' is exactly 0 */
    ASSERT_EQ(tcs34725_get_color_temp(&rgbc, &cct), TCS34725_ERR_PARAM);

    /* R' is the smallest step above 0, and B' = 0.5 too: 3810 * 1 + 1391 */
    tcs34725_rgbc_t tiny = { .c = 101, .r = 50, .g = 100, .b = 50 };
    ASSERT_EQ(tcs34725_get_color_temp(&tiny, &cct), TCS34725_OK);
    ASSERT_EQ(cct, 5201);
}

TEST_MAIN("prop_tcs34725")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(config_functions_no_crash);
    RUN(color_math_no_crash);
    RUN(test_init_i2c_fail_id_read);
    RUN(test_init_i2c_fail_atime);
    RUN(test_init_i2c_fail_control);
    RUN(test_init_i2c_fail_enable);
    RUN(test_read_rgbc_i2c_fail);
    RUN(test_set_integration_time_i2c_fail);
    RUN(test_set_gain_i2c_fail);
    RUN(test_enable_i2c_fail);
    RUN(test_disable_i2c_fail);
    RUN(test_color_temp_cct_clamped);
    RUN(test_lux_all_zeros);
    RUN(test_init_null_callbacks);
    RUN(test_init_null_i2c);
    RUN(test_init_bad_gain_over_range);
    RUN(test_null_guards_api);
    RUN(test_color_temp_denom_near_zero);
TEST_MAIN_END()
