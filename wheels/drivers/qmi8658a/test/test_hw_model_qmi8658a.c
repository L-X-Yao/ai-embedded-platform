// SPDX-License-Identifier: MIT
/*
 * test_hw_model_qmi8658a.c — tests using hardware register model
 *
 * Validates QMI8658A driver against a register model.
 * The model enforces WHO_AM_I read-only, data register read-only,
 * soft reset via 0x60, and accel/gyro scaling.
 */

#include "test_harness.h"
#include "hw_model_qmi8658a.h"
#include "../qmi8658a.h"
#include <math.h>

static qmi8658a_model_t model;

static const qmi8658a_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    qmi8658a_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_who_am_i)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_err_t err = qmi8658a_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(err, QMI8658A_OK);
}

TEST(test_init_wrong_who_am_i)
{
    setup();
    model.hw.reset_values[QMI_REG_WHO_AM_I] = 0xFF;
    hw_model_reset(&model.hw);
    qmi8658a_model_set_data_regs(&model);

    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_err_t err = qmi8658a_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(err, QMI8658A_ERR_CHIP_ID);
}

TEST(test_init_writes_ctrl1)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);
    /* CTRL1 should be 0x40 (address auto-increment) */
    ASSERT_EQ(model.hw.regs[QMI_REG_CTRL1], 0x40);
}

TEST(test_init_writes_ctrl2)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    /* default_cfg: acc_range=8G(2), acc_odr=500(4) */
    qmi8658a_init(&dev, &model_i2c, &cfg);
    /* CTRL2: acc_range[7:4]=2, acc_odr[3:0]=4 => 0x24 */
    ASSERT_EQ(model.hw.regs[QMI_REG_CTRL2], 0x24);
}

TEST(test_init_writes_ctrl7)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);
    /* CTRL7: accel+gyro enable = 0x03 */
    ASSERT_EQ(model.hw.regs[QMI_REG_CTRL7], 0x03);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_accel_1g_z)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);
    /* default: az=4096 at 8g range (4096 LSB/g) -> 1.0g */
    qmi8658a_model_set_accel(&model, 0, 0, 4096);

    qmi8658a_data_t data;
    qmi8658a_err_t err = qmi8658a_read(&dev, &data);
    ASSERT_EQ(err, QMI8658A_OK);
    ASSERT_NEAR(data.ax_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.ay_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.az_g, 1.0f, 0.01f);
}

TEST(test_read_gyro_zero)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);
    qmi8658a_model_set_gyro(&model, 0, 0, 0);

    qmi8658a_data_t data;
    qmi8658a_err_t err = qmi8658a_read(&dev, &data);
    ASSERT_EQ(err, QMI8658A_OK);
    ASSERT_NEAR(data.gx_dps, 0.0f, 0.01f);
    ASSERT_NEAR(data.gy_dps, 0.0f, 0.01f);
    ASSERT_NEAR(data.gz_dps, 0.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);
    qmi8658a_model_set_accel(&model, -4096, 0, 0);

    qmi8658a_data_t data;
    qmi8658a_read(&dev, &data);
    ASSERT_NEAR(data.ax_g, -1.0f, 0.01f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, QMI_REG_WHO_AM_I, 0x00);
    ASSERT_EQ(model.hw.regs[QMI_REG_WHO_AM_I], 0x05);
}

TEST(test_data_registers_readonly)
{
    setup();
    qmi8658a_model_set_accel(&model, 0, 0, 4096);
    uint8_t original = model.hw.regs[QMI_REG_AX_L];
    hw_model__write_reg(&model.hw, QMI_REG_AX_L, 0xFF);
    ASSERT_EQ(model.hw.regs[QMI_REG_AX_L], original);
}

TEST(test_soft_reset)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);

    /* Write config */
    ASSERT_EQ(model.hw.regs[QMI_REG_CTRL7], 0x03);

    int before = model.reset_count;
    qmi8658a_reset(&dev);
    ASSERT_TRUE(model.reset_count > before);
    /* After reset, CTRL7 back to 0 */
    ASSERT_EQ(model.hw.regs[QMI_REG_CTRL7], 0x00);
}

TEST(test_different_accel_values)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &model_i2c, &cfg);

    qmi8658a_model_set_accel(&model, 1000, 0, 4096);
    qmi8658a_data_t d1;
    qmi8658a_read(&dev, &d1);

    qmi8658a_model_set_accel(&model, 2000, 0, 4096);
    qmi8658a_data_t d2;
    qmi8658a_read(&dev, &d2);

    ASSERT_TRUE(d2.ax_g > d1.ax_g);
}

TEST_MAIN("qmi8658a hardware model")
    RUN(test_init_reads_who_am_i);
    RUN(test_init_wrong_who_am_i);
    RUN(test_init_writes_ctrl1);
    RUN(test_init_writes_ctrl2);
    RUN(test_init_writes_ctrl7);
    RUN(test_read_accel_1g_z);
    RUN(test_read_gyro_zero);
    RUN(test_read_accel_negative);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_soft_reset);
    RUN(test_different_accel_values);
TEST_MAIN_END()
