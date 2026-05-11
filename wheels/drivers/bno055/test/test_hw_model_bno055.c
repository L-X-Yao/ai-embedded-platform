// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bno055.h"
#include "../bno055.h"

static bno055_model_t model;

static void model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const bno055_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = model_delay,
    .ctx       = &model.hw,
};

static const bno055_cfg_t default_cfg = {
    .addr = BNO055_DEV_ADDR,
    .mode = BNO055_MODE_NDOF,
};

static void setup(void)
{
    bno055_model_init(&model);
}

TEST(test_init_reads_chip_id)
{
    setup();
    bno055_t dev;
    bno055_err_t err = bno055_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BNO055_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[BNO055_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);
    bno055_t dev;
    bno055_err_t err = bno055_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BNO055_ERR_ID);
}

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, BNO055_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BNO055_REG_CHIP_ID], 0xA0);
}

TEST(test_read_euler_180_heading)
{
    setup();
    bno055_t dev;
    bno055_init(&dev, &model_i2c, &default_cfg);

    /* Default: 180 degrees heading (2880 LSB) */
    bno055_euler_t euler;
    bno055_err_t err = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(err, BNO055_OK);
    ASSERT_NEAR(euler.heading_deg, 180.0f, 0.1f);
}

TEST(test_different_euler)
{
    setup();
    bno055_t dev;
    bno055_init(&dev, &model_i2c, &default_cfg);

    /* 45 deg heading, -30 roll, 15 pitch */
    bno055_model_set_euler(&model, 720, -480, 240);
    bno055_euler_t euler;
    bno055_read_euler(&dev, &euler);
    ASSERT_NEAR(euler.heading_deg, 45.0f, 0.1f);
    ASSERT_NEAR(euler.roll_deg, -30.0f, 0.1f);
    ASSERT_NEAR(euler.pitch_deg, 15.0f, 0.1f);
}

TEST(test_euler_registers_readonly)
{
    setup();
    bno055_t dev;
    bno055_init(&dev, &model_i2c, &default_cfg);

    uint8_t original = model.hw.regs[BNO055_REG_EUL_HEAD];
    hw_model__write_reg(&model.hw, BNO055_REG_EUL_HEAD, 0xFF);
    ASSERT_EQ(model.hw.regs[BNO055_REG_EUL_HEAD], original);
}

TEST(test_mode_switch_count)
{
    setup();
    bno055_t dev;
    int before = model.mode_switch_count;
    bno055_init(&dev, &model_i2c, &default_cfg);
    /* Config mode + NDOF mode = at least 2 mode writes */
    ASSERT_TRUE(model.mode_switch_count >= before + 2);
}

TEST(test_multiple_reads)
{
    setup();
    bno055_t dev;
    bno055_init(&dev, &model_i2c, &default_cfg);

    bno055_euler_t e1, e2;
    bno055_read_euler(&dev, &e1);
    bno055_read_euler(&dev, &e2);
    ASSERT_NEAR(e1.heading_deg, e2.heading_deg, 0.01f);
}

TEST_MAIN("bno055 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_chip_id_readonly);
    RUN(test_read_euler_180_heading);
    RUN(test_different_euler);
    RUN(test_euler_registers_readonly);
    RUN(test_mode_switch_count);
    RUN(test_multiple_reads);
TEST_MAIN_END()
