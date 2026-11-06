// SPDX-License-Identifier: MIT
/*
 * test_hw_model_qmi8658a.c — tests using hardware register model
 *
 * Validates QMI8658A driver against a register model.
 * The model enforces WHO_AM_I read-only, data register read-only,
 * soft reset via 0x60, accel/gyro scaling, the CTRL1 address and byte order
 * bits, and data that moves only with a sensor enabled.
 */

#include "test_harness.h"
#include "hw_model_qmi8658a.h"
#include "../qmi8658a.h"
#include <math.h>

static qmi8658a_model_t model;

static const qmi8658a_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = qmi8658a_model_read_regs,
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

/* --- what the model itself must do ------------------------------------- */

static void read_raw(uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)qmi8658a_model_read_regs(&model.hw, 0x6B, reg, buf, len);
}

/* Table 22 p.31: bit 6 ADDR_AI 1'b0, bit 5 BE 1'b1, the rest 0. */
TEST(test_model_ctrl1_reset_value)
{
    setup();
    ASSERT_EQ_HEX(model.hw.regs[QMI_REG_CTRL1], 0x20);
}

/* Table 31 p.50: "All sensors off", and "this mode is the default state upon
 * initial power up or after a reset." */
TEST(test_model_data_stay_still_with_the_sensors_off)
{
    setup();
    qmi8658a_model_set_accel(&model, 1000, 2000, 3000);
    qmi8658a_model_set_gyro(&model, 4000, 5000, 6000);

    uint8_t raw[12];
    hw_model__write_reg(&model.hw, QMI_REG_CTRL1, 0x40);
    read_raw(QMI_REG_AX_L, raw, 12);
    for (int i = 0; i < 12; i++)
        ASSERT_EQ_HEX(raw[i], 0x00);
}

/* CTRL7 bit 0 aEN, bit 1 gEN: each sensor moves only its own registers. */
TEST(test_model_each_sensor_has_its_own_enable)
{
    setup();
    hw_model__write_reg(&model.hw, QMI_REG_CTRL1, 0x40);
    qmi8658a_model_set_accel(&model, 0x0102, 0, 0);
    qmi8658a_model_set_gyro(&model, 0x0304, 0, 0);

    uint8_t raw[12];
    hw_model__write_reg(&model.hw, QMI_REG_CTRL7, 0x01);
    read_raw(QMI_REG_AX_L, raw, 12);
    ASSERT_EQ_HEX(raw[0], 0x02);
    ASSERT_EQ_HEX(raw[6], 0x00);

    hw_model__write_reg(&model.hw, QMI_REG_CTRL7, 0x02);
    qmi8658a_model_set_accel(&model, 0x0506, 0, 0);
    read_raw(QMI_REG_AX_L, raw, 12);
    ASSERT_EQ_HEX(raw[0], 0x02);
    ASSERT_EQ_HEX(raw[6], 0x04);
}

/* p.76: "If ADDR_AI = 0, the register address will not increase". */
TEST(test_model_burst_repeats_without_addr_ai)
{
    setup();
    hw_model__write_reg(&model.hw, QMI_REG_CTRL1, 0x00);
    hw_model__write_reg(&model.hw, QMI_REG_CTRL7, 0x03);
    qmi8658a_model_set_accel(&model, 0x1234, 0x5678, 0);

    uint8_t raw[4];
    read_raw(QMI_REG_AX_L, raw, 4);
    ASSERT_EQ_HEX(raw[0], 0x34);
    ASSERT_EQ_HEX(raw[1], 0x34);
    ASSERT_EQ_HEX(raw[2], 0x34);
    ASSERT_EQ_HEX(raw[3], 0x34);
}

/* Table 22 p.31: "1: Serial interface (SPI, I2C, I3C) read data Big-Endian" */
TEST(test_model_big_endian_is_the_default)
{
    setup();
    hw_model__write_reg(&model.hw, QMI_REG_CTRL1, 0x60);
    hw_model__write_reg(&model.hw, QMI_REG_CTRL7, 0x03);
    qmi8658a_model_set_accel(&model, 0x1234, 0x5678, 0);

    uint8_t raw[4];
    read_raw(QMI_REG_AX_L, raw, 4);
    ASSERT_EQ_HEX(raw[0], 0x12);
    ASSERT_EQ_HEX(raw[1], 0x34);
    ASSERT_EQ_HEX(raw[2], 0x56);
    ASSERT_EQ_HEX(raw[3], 0x78);
}

/* Table 22: "0: Serial interface (SPI, I2C, I3C) read data Little-Endian" */
TEST(test_model_little_endian_with_be_clear)
{
    setup();
    hw_model__write_reg(&model.hw, QMI_REG_CTRL1, 0x40);
    hw_model__write_reg(&model.hw, QMI_REG_CTRL7, 0x03);
    qmi8658a_model_set_accel(&model, 0x1234, 0x5678, 0);

    uint8_t raw[4];
    read_raw(QMI_REG_AX_L, raw, 4);
    ASSERT_EQ_HEX(raw[0], 0x34);
    ASSERT_EQ_HEX(raw[1], 0x12);
    ASSERT_EQ_HEX(raw[2], 0x78);
    ASSERT_EQ_HEX(raw[3], 0x56);
}

/* --- the driver against the model -------------------------------------- */

/* All six axes through the real address and byte order: each value differs
 * in both bytes, so a swap or a repeated register cannot pass. */
TEST(test_read_all_six_axes)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    ASSERT_EQ(qmi8658a_init(&dev, &model_i2c, &cfg), QMI8658A_OK);
    qmi8658a_model_set_accel(&model, 0x0123, 0x0456, -0x0789);
    qmi8658a_model_set_gyro(&model, 0x0ABC, -0x0DEF, 0x1234);

    qmi8658a_data_t data;
    ASSERT_EQ(qmi8658a_read(&dev, &data), QMI8658A_OK);
    ASSERT_NEAR(data.ax_g, 291.0f / 4096.0f, 0.0001f);
    ASSERT_NEAR(data.ay_g, 1110.0f / 4096.0f, 0.0001f);
    ASSERT_NEAR(data.az_g, -1929.0f / 4096.0f, 0.0001f);
    ASSERT_NEAR(data.gx_dps, 2748.0f / 16.0f, 0.001f);
    ASSERT_NEAR(data.gy_dps, -3567.0f / 16.0f, 0.001f);
    ASSERT_NEAR(data.gz_dps, 4660.0f / 16.0f, 0.001f);
}

/* Data set before init reaches the registers only once init enables the
 * sensors, so a failed enable cannot read as +1 g. */
TEST(test_init_starts_the_sensors)
{
    setup();
    qmi8658a_model_set_accel(&model, 0, 0, 4096);
    ASSERT_EQ_HEX(model.hw.regs[0x3A], 0x00);

    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    ASSERT_EQ(qmi8658a_init(&dev, &model_i2c, &cfg), QMI8658A_OK);

    qmi8658a_data_t data;
    ASSERT_EQ(qmi8658a_read(&dev, &data), QMI8658A_OK);
    ASSERT_NEAR(data.az_g, 1.0f, 0.001f);
}

/* Table 31: the reset goes back to "All sensors off", so nothing updates. */
TEST(test_reset_stops_the_data)
{
    setup();
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    ASSERT_EQ(qmi8658a_init(&dev, &model_i2c, &cfg), QMI8658A_OK);
    ASSERT_EQ(qmi8658a_reset(&dev), QMI8658A_OK);
    ASSERT_EQ_HEX(model.hw.regs[QMI_REG_CTRL1], 0x20);

    qmi8658a_model_set_accel(&model, 0, 0, 4096);
    ASSERT_EQ_HEX(model.hw.regs[0x3A], 0x00);
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
    RUN(test_model_ctrl1_reset_value);
    RUN(test_model_data_stay_still_with_the_sensors_off);
    RUN(test_model_each_sensor_has_its_own_enable);
    RUN(test_model_burst_repeats_without_addr_ai);
    RUN(test_model_big_endian_is_the_default);
    RUN(test_model_little_endian_with_be_clear);
    RUN(test_read_all_six_axes);
    RUN(test_init_starts_the_sensors);
    RUN(test_reset_stops_the_data);
TEST_MAIN_END()
