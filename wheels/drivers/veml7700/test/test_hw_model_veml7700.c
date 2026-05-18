// SPDX-License-Identifier: MIT
/*
 * test_hw_model_veml7700.c — tests using hardware register model
 *
 * Validates VEML7700 driver against a datasheet-accurate register model.
 * The model enforces read-only ALS/White data registers, 16-bit LE
 * register protocol, config register behavior, and power on/off.
 */

#include "test_harness.h"
#include "hw_model_veml7700.h"
#include "../veml7700.h"
#include <math.h>

static veml7700_model_t model;

static const veml7700_i2c_t model_i2c = {
    .write_regs = veml7700_model_i2c_write_regs,
    .read_regs  = veml7700_model_i2c_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static void setup(void)
{
    veml7700_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    veml7700_t dev;
    veml7700_err_t err = veml7700_init(&dev, &model_i2c,
                                        VEML7700_GAIN_1, VEML7700_IT_100MS);
    ASSERT_EQ(err, VEML7700_OK);
}

TEST(test_init_null_i2c)
{
    setup();
    veml7700_t dev;
    veml7700_err_t err = veml7700_init(&dev, NULL,
                                        VEML7700_GAIN_1, VEML7700_IT_100MS);
    ASSERT_EQ(err, VEML7700_ERR_PARAM);
}

TEST(test_init_writes_config)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);
    /* gain=0 (GAIN_1), IT=0 (IT_100MS), SD=0 (powered on) -> conf = 0x0000 */
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    uint16_t conf = (uint16_t)((uint16_t)model.hw.regs[conf_base] |
                    ((uint16_t)model.hw.regs[conf_base + 1] << 8));
    ASSERT_EQ(conf, 0x0000u);
}

TEST(test_init_with_gain2_it400)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_2, VEML7700_IT_400MS);
    /* gain=1 at bits[12:11], IT=2 at bits[9:6], SD=0 */
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    uint16_t conf = (uint16_t)((uint16_t)model.hw.regs[conf_base] |
                    ((uint16_t)model.hw.regs[conf_base + 1] << 8));
    /* gain=1 << 11 = 0x0800, IT=2 << 6 = 0x0080 */
    ASSERT_EQ(conf, 0x0880u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_als_raw)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint16_t raw = 0;
    veml7700_err_t err = veml7700_read_als(&dev, &raw);
    ASSERT_EQ(err, VEML7700_OK);
    ASSERT_EQ(raw, 1000);
}

TEST(test_read_white_raw)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint16_t raw = 0;
    veml7700_err_t err = veml7700_read_white(&dev, &raw);
    ASSERT_EQ(err, VEML7700_OK);
    ASSERT_EQ(raw, 1200);
}

TEST(test_read_lux_reasonable)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    float lux = 0.0f;
    veml7700_err_t err = veml7700_read_lux(&dev, &lux);
    ASSERT_EQ(err, VEML7700_OK);
    /* raw=1000, resolution at gain=1, IT=100ms = 0.0576 -> ~57.6 lux */
    ASSERT_TRUE(lux > 50.0f && lux < 65.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_als_data_readonly)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint8_t als_base = VEML_BYTE_ADDR(VEML_REG_ALS_DATA);
    uint8_t orig = model.hw.regs[als_base];
    hw_model__write_reg(&model.hw, als_base, 0xFF);
    ASSERT_EQ(model.hw.regs[als_base], orig);
}

TEST(test_white_data_readonly)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint8_t w_base = VEML_BYTE_ADDR(VEML_REG_WHITE_DATA);
    uint8_t orig = model.hw.regs[w_base];
    hw_model__write_reg(&model.hw, w_base, 0xFF);
    ASSERT_EQ(model.hw.regs[w_base], orig);
}

TEST(test_shutdown_sets_sd_bit)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    veml7700_shutdown(&dev);
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    ASSERT_TRUE(model.hw.regs[conf_base] & 0x01u);
}

TEST(test_power_on_clears_sd_bit)
{
    setup();
    veml7700_t dev;
    veml7700_init(&dev, &model_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    veml7700_shutdown(&dev);
    veml7700_power_on(&dev);
    uint8_t conf_base = VEML_BYTE_ADDR(VEML_REG_ALS_CONF);
    ASSERT_FALSE(model.hw.regs[conf_base] & 0x01u);
}

TEST_MAIN("veml7700 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_i2c);
    RUN(test_init_writes_config);
    RUN(test_init_with_gain2_it400);
    RUN(test_read_als_raw);
    RUN(test_read_white_raw);
    RUN(test_read_lux_reasonable);
    RUN(test_als_data_readonly);
    RUN(test_white_data_readonly);
    RUN(test_shutdown_sets_sd_bit);
    RUN(test_power_on_clears_sd_bit);
TEST_MAIN_END()
