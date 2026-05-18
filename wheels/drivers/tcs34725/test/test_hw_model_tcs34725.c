// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tcs34725.c — tests using hardware register model
 *
 * Validates TCS34725 driver against a datasheet-accurate register model.
 * The model enforces read-only chip ID (0x44), RGBC data register
 * protection, command protocol (0x80|reg), enable/disable behavior.
 */

#include "test_harness.h"
#include "hw_model_tcs34725.h"
#include "../tcs34725.h"
#include <math.h>

static tcs34725_model_t model;

static const tcs34725_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    tcs34725_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[TCS_REG_ID] = 0xFF;
    hw_model_reset(&model.hw);

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_ERR_ID);
}

TEST(test_init_accepts_variant_id)
{
    setup();
    /* 0x4D is an accepted variant */
    model.hw.reset_values[TCS_REG_ID] = 0x4Du;
    hw_model_reset(&model.hw);

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_OK);
}

TEST(test_init_sets_integration_time)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    /* ATIME register at 0x81 should be 0xD5 */
    ASSERT_EQ(model.hw.regs[TCS_REG_ATIME], 0xD5u);
}

TEST(test_init_sets_gain)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_4X);
    /* Control register at 0x8F should be 0x01 */
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x01u);
}

TEST(test_init_enables_pon_aen)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    /* Enable register should have PON (0x01) + AEN (0x02) = 0x03 */
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x03u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_rgbc)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);

    tcs34725_rgbc_t rgbc;
    tcs34725_err_t err = tcs34725_read_rgbc(&dev, &rgbc);
    ASSERT_EQ(err, TCS34725_OK);
    ASSERT_EQ(rgbc.c, 5000);
    ASSERT_EQ(rgbc.r, 1800);
    ASSERT_EQ(rgbc.g, 2200);
    ASSERT_EQ(rgbc.b, 1000);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[TCS_REG_ID], 0x44u);
}

TEST(test_rgbc_data_readonly)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);

    uint8_t orig = model.hw.regs[TCS_REG_CDATAL];
    hw_model__write_reg(&model.hw, TCS_REG_CDATAL, 0xFF);
    ASSERT_EQ(model.hw.regs[TCS_REG_CDATAL], orig);
}

TEST(test_disable_clears_enable)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x03u);

    tcs34725_disable(&dev);
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x00u);
}

TEST(test_change_gain_runtime)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x00u);

    tcs34725_set_gain(&dev, TCS34725_GAIN_60X);
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x03u);
}

TEST_MAIN("tcs34725 hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_accepts_variant_id);
    RUN(test_init_sets_integration_time);
    RUN(test_init_sets_gain);
    RUN(test_init_enables_pon_aen);
    RUN(test_read_rgbc);
    RUN(test_chip_id_readonly);
    RUN(test_rgbc_data_readonly);
    RUN(test_disable_clears_enable);
    RUN(test_change_gain_runtime);
TEST_MAIN_END()
