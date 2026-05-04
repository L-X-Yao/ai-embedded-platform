/*
 * test_hw_model_lis3dh.c — tests using hardware register model
 *
 * Validates LIS3DH driver against a datasheet-accurate register model.
 * The model enforces WHO_AM_I read-only, data register read-only,
 * auto-increment addressing, and acceleration scaling.
 */

#include "test_harness.h"
#include "hw_model_lis3dh.h"
#include "../lis3dh.h"
#include <math.h>

static lis3dh_model_t model;

static const lis3dh_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const lis3dh_cfg_t default_cfg = {
    .addr  = LIS3DH_ADDR_LOW,
    .range = LIS3DH_RANGE_2G,
    .odr   = LIS3DH_ODR_100HZ,
};

static void setup(void)
{
    lis3dh_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_who_am_i)
{
    setup();
    lis3dh_t dev;
    lis3dh_err_t err = lis3dh_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3DH_OK);
}

TEST(test_init_wrong_who_am_i)
{
    setup();
    model.hw.reset_values[LIS3DH_REG_WHO_AM_I] = 0xFF;
    hw_model_reset(&model.hw);
    lis3dh_model_set_data_regs(&model);

    lis3dh_t dev;
    lis3dh_err_t err = lis3dh_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3DH_ERR_ID);
}

TEST(test_init_writes_ctrl_reg1)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);
    /* CTRL_REG1: ODR[7:4]=5 (100Hz), all axes enabled [2:0]=7 */
    uint8_t ctrl1 = model.hw.regs[LIS3DH_REG_CTRL_REG1];
    ASSERT_EQ(ctrl1 & 0x07, 0x07);          /* XYZ enabled */
    ASSERT_EQ((ctrl1 >> 4) & 0x0F, 5);      /* ODR=100Hz */
}

TEST(test_init_writes_ctrl_reg4)
{
    setup();
    lis3dh_t dev;
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = LIS3DH_RANGE_4G,
                         .odr = LIS3DH_ODR_100HZ };
    lis3dh_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG4: BDU[7]=1, FS[5:4]=1 (4g) */
    uint8_t ctrl4 = model.hw.regs[LIS3DH_REG_CTRL_REG4];
    ASSERT_TRUE(ctrl4 & 0x80);              /* BDU set */
    ASSERT_EQ((ctrl4 >> 4) & 0x03, 1);     /* FS = 4g */
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_accel_1g_z)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);
    /* Model default: z=16000 left-justified, at 2g scale=16000.0 -> 1.0g */
    lis3dh_model_set_accel(&model, 0, 0, 16000);

    lis3dh_accel_t accel;
    lis3dh_err_t err = lis3dh_read_accel(&dev, &accel);
    ASSERT_EQ(err, LIS3DH_OK);
    ASSERT_NEAR(accel.x_g, 0.0f, 0.01f);
    ASSERT_NEAR(accel.y_g, 0.0f, 0.01f);
    ASSERT_NEAR(accel.z_g, 1.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);
    /* -1g on X */
    lis3dh_model_set_accel(&model, -16000, 0, 0);

    lis3dh_accel_t accel;
    lis3dh_read_accel(&dev, &accel);
    ASSERT_NEAR(accel.x_g, -1.0f, 0.01f);
}

TEST(test_read_raw_values)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);
    lis3dh_model_set_accel(&model, 0x0100, 0x0200, 0x0300);

    lis3dh_raw_t raw;
    lis3dh_err_t err = lis3dh_read_raw(&dev, &raw);
    ASSERT_EQ(err, LIS3DH_OK);
    ASSERT_EQ(raw.x, 0x0100);
    ASSERT_EQ(raw.y, 0x0200);
    ASSERT_EQ(raw.z, 0x0300);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, LIS3DH_REG_WHO_AM_I, 0x00);
    ASSERT_EQ(model.hw.regs[LIS3DH_REG_WHO_AM_I], 0x33);
}

TEST(test_data_registers_readonly)
{
    setup();
    lis3dh_model_set_accel(&model, 0, 0, 16000);
    uint8_t original = model.hw.regs[LIS3DH_REG_OUT_Z_H];
    hw_model__write_reg(&model.hw, LIS3DH_REG_OUT_Z_H, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3DH_REG_OUT_Z_H], original);
}

TEST(test_set_range_updates_config)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);

    lis3dh_set_range(&dev, LIS3DH_RANGE_8G);
    uint8_t ctrl4 = model.hw.regs[LIS3DH_REG_CTRL_REG4];
    ASSERT_EQ((ctrl4 >> 4) & 0x03, 2);     /* FS=8g */
}

TEST(test_set_odr_updates_config)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);

    lis3dh_set_odr(&dev, LIS3DH_ODR_400HZ);
    uint8_t ctrl1 = model.hw.regs[LIS3DH_REG_CTRL_REG1];
    ASSERT_EQ((ctrl1 >> 4) & 0x0F, 7);     /* ODR=400Hz */
}

TEST(test_different_accel_values)
{
    setup();
    lis3dh_t dev;
    lis3dh_init(&dev, &model_i2c, &default_cfg);

    lis3dh_model_set_accel(&model, 4000, 0, 16000);
    lis3dh_accel_t a1;
    lis3dh_read_accel(&dev, &a1);

    lis3dh_model_set_accel(&model, 8000, 0, 16000);
    lis3dh_accel_t a2;
    lis3dh_read_accel(&dev, &a2);

    ASSERT_TRUE(a2.x_g > a1.x_g);
}

TEST_MAIN("lis3dh hardware model")
    RUN(test_init_reads_who_am_i);
    RUN(test_init_wrong_who_am_i);
    RUN(test_init_writes_ctrl_reg1);
    RUN(test_init_writes_ctrl_reg4);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_raw_values);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_set_range_updates_config);
    RUN(test_set_odr_updates_config);
    RUN(test_different_accel_values);
TEST_MAIN_END()
