// SPDX-License-Identifier: MIT
/*
 * test_hw_model_lis3mdl.c — tests using hardware register model
 *
 * Validates LIS3MDL driver against a datasheet-accurate register model.
 * The model enforces read-only WHO_AM_I (0x3D), data register protection,
 * config register writable masks, and continuous measurement trigger.
 */

#include "test_harness.h"
#include "hw_model_lis3mdl.h"
#include "../lis3mdl.h"
#include <math.h>

static lis3mdl_model_t model;

static const lis3mdl_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = lis3mdl_hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const lis3mdl_cfg_t default_cfg = {
    .range = LIS3MDL_RANGE_4G,
    .addr  = LIS3MDL_ADDR_LOW,
};

static void setup(void)
{
    lis3mdl_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_por_defaults)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_err_t err = lis3mdl_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3MDL_OK);
}

TEST(test_hw_init_wrong_id)
{
    setup();
    /* Corrupt WHO_AM_I: 0x3D -> 0x00 */
    model.hw.reset_values[LIS3MDL_REG_WHO_AM_I] = 0x00;
    hw_model_reset(&model.hw);

    lis3mdl_t dev;
    lis3mdl_err_t err = lis3mdl_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3MDL_ERR_ID);
}

TEST(test_hw_init_sets_ctrl_reg1)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* CTRL_REG1 should be 0x30 (XY medium, 10 Hz) */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG1], 0x30u);
}

TEST(test_hw_init_sets_ctrl_reg2_4g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_4G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=0 -> bits[6:5]=00 -> 0x00 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x00u);
}

TEST(test_hw_init_sets_ctrl_reg2_8g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_8G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=1 -> bits[6:5]=01 -> 0x20 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x20u);
}

TEST(test_hw_init_sets_ctrl_reg2_12g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_12G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=2 -> bits[6:5]=10 -> 0x40 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x40u);
}

TEST(test_hw_init_sets_ctrl_reg2_16g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_16G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=3 -> bits[6:5]=11 -> 0x60 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x60u);
}

TEST(test_hw_init_sets_continuous_mode)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* CTRL_REG3 = 0x00 = continuous */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG3], 0x00u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_hw_read_round_trip)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);

    lis3mdl_raw_t raw;
    lis3mdl_err_t err = lis3mdl_read_raw(&dev, &raw);
    ASSERT_EQ(err, LIS3MDL_OK);
    ASSERT_EQ(raw.x, model.true_x);
    ASSERT_EQ(raw.y, model.true_y);
    ASSERT_EQ(raw.z, model.true_z);
}

TEST(test_hw_read_ut_4g)
{
    setup();
    /* Inject 50 uT on X at 4g range */
    lis3mdl_model_inject_ut(&model, 50.0f, -30.0f, 0.0f, 6842.0f);

    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);

    lis3mdl_data_t data;
    lis3mdl_err_t err = lis3mdl_read(&dev, &data);
    ASSERT_EQ(err, LIS3MDL_OK);
    /* Tolerance: truncation from int16 cast */
    ASSERT_NEAR(data.x_ut, 50.0f, 0.5f);
    ASSERT_NEAR(data.y_ut, -30.0f, 0.5f);
    ASSERT_NEAR(data.z_ut, 0.0f, 0.5f);
}

TEST(test_hw_read_ut_16g)
{
    setup();
    lis3mdl_model_inject_ut(&model, 200.0f, -100.0f, 50.0f, 1711.0f);

    lis3mdl_t dev;
    lis3mdl_cfg_t cfg16 = { .range = LIS3MDL_RANGE_16G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg16);

    lis3mdl_data_t data;
    lis3mdl_err_t err = lis3mdl_read(&dev, &data);
    ASSERT_EQ(err, LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 200.0f, 1.0f);
    ASSERT_NEAR(data.y_ut, -100.0f, 1.0f);
    ASSERT_NEAR(data.z_ut, 50.0f, 1.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, LIS3MDL_REG_WHO_AM_I, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_WHO_AM_I], 0x3Du);
}

TEST(test_data_registers_readonly)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);

    uint8_t orig = model.hw.regs[LIS3MDL_REG_OUT_X_L];
    hw_model__write_reg(&model.hw, LIS3MDL_REG_OUT_X_L, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_OUT_X_L], orig);
}

TEST(test_ctrl_reg2_writable_mask)
{
    setup();
    /* CTRL_REG2 only has bits [6:5] writable */
    hw_model__write_reg(&model.hw, LIS3MDL_REG_CTRL_REG2, 0xFF);
    /* Only bits [6:5] should be set */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x60u);
}

TEST(test_different_field_strengths)
{
    setup();
    model.true_x = 100;
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    lis3mdl_data_t d1;
    lis3mdl_read(&dev, &d1);
    float weak = d1.x_ut;

    model.true_x = 5000;
    lis3mdl_model_set_data(&model);
    lis3mdl_data_t d2;
    lis3mdl_read(&dev, &d2);
    float strong = d2.x_ut;

    ASSERT_TRUE(strong > weak);
}

TEST(test_continuous_mode_triggers_data)
{
    setup();
    int count_before = model.measure_count;
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* Init writes CTRL_REG3=0x00 which triggers measurement */
    ASSERT_TRUE(model.measure_count > count_before);
}

TEST_MAIN("lis3mdl hardware model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_wrong_id);
    RUN(test_hw_init_sets_ctrl_reg1);
    RUN(test_hw_init_sets_ctrl_reg2_4g);
    RUN(test_hw_init_sets_ctrl_reg2_8g);
    RUN(test_hw_init_sets_ctrl_reg2_12g);
    RUN(test_hw_init_sets_ctrl_reg2_16g);
    RUN(test_hw_init_sets_continuous_mode);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_read_ut_4g);
    RUN(test_hw_read_ut_16g);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_ctrl_reg2_writable_mask);
    RUN(test_different_field_strengths);
    RUN(test_continuous_mode_triggers_data);
TEST_MAIN_END()
