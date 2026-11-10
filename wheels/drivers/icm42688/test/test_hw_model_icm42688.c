// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_icm42688.h"
#include "icm42688.h"

static icm42688_model_t model;

static const icm42688_spi_t hw_spi = {
    .cs_low  = icm42688_model_cs_low,
    .cs_high = icm42688_model_cs_high,
    .xfer    = icm42688_model_xfer,
    .ctx     = &model,
};

static icm42688_err_t hw_setup(icm42688_t *dev)
{
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_model_init(&model);
    return icm42688_init(dev, &hw_spi, &cfg);
}

/* A register access as the host makes it */
static uint8_t spi_read_reg(uint8_t reg)
{
    hw_spi.cs_low(hw_spi.ctx);
    hw_spi.xfer(hw_spi.ctx, (uint8_t)(reg | ICM_SPI_READ_BIT));
    uint8_t val = hw_spi.xfer(hw_spi.ctx, 0x00);
    hw_spi.cs_high(hw_spi.ctx);
    return val;
}

static void spi_write_reg(uint8_t reg, uint8_t val)
{
    hw_spi.cs_low(hw_spi.ctx);
    hw_spi.xfer(hw_spi.ctx, reg);
    hw_spi.xfer(hw_spi.ctx, val);
    hw_spi.cs_high(hw_spi.ctx);
}

/* Section 14 of DS-000347 Rev 1.6: Reset value of each bank 0 register with a
 * bearing on the driver, and of every one that does not reset to 0 */
static const struct { uint8_t reg, val; } reset_values[] = {
    {0x11, 0x00},   /* DEVICE_CONFIG */
    {0x13, 0x05},   /* DRIVE_CONFIG */
    {0x14, 0x00},   /* INT_CONFIG */
    {0x2D, 0x10},   /* INT_STATUS */
    {0x30, 0xFF},   /* FIFO_DATA */
    {0x34, 0x04},   /* APEX_DATA3 */
    {0x4C, 0x30},   /* INTF_CONFIG0 */
    {0x4D, 0x91},   /* INTF_CONFIG1 */
    {0x4E, 0x00},   /* PWR_MGMT0 */
    {0x4F, 0x06},   /* GYRO_CONFIG0 */
    {0x50, 0x06},   /* ACCEL_CONFIG0 */
    {0x51, 0x16},   /* GYRO_CONFIG1 */
    {0x52, 0x11},   /* GYRO_ACCEL_CONFIG0 */
    {0x53, 0x0D},   /* ACCEL_CONFIG1 */
    {0x54, 0x23},   /* TMST_CONFIG */
    {0x56, 0x82},   /* APEX_CONFIG0 */
    {0x62, 0x10},   /* FSYNC_CONFIG */
    {0x64, 0x10},   /* INT_CONFIG1 */
    {0x65, 0x10},   /* INT_SOURCE0 */
    {0x75, 0x47},   /* WHO_AM_I */
    {0x76, 0x00},   /* REG_BANK_SEL */
};

static void assert_reset_values(void)
{
    for (size_t i = 0; i < sizeof(reset_values) / sizeof(reset_values[0]); i++) {
        ASSERT_EQ_HEX(model.hw.regs[reset_values[i].reg], reset_values[i].val);
    }
    for (int reg = ICM_REG_TEMP_DATA1; reg <= ICM_REG_GYRO_DATA_Z1; reg += 2) {
        ASSERT_EQ_HEX(model.hw.regs[reg], 0x80);
    }
}

TEST(test_hw_power_on_reset_values)
{
    icm42688_model_init(&model);
    assert_reset_values();
}

/* p.64: SOFT_RESET_CONFIG, "1: Enable reset". GYRO_CONFIG0 (p.77) and
 * ACCEL_CONFIG0 (p.78) go back to 0x06, which is 1 kHz (section 14.37) */
TEST(test_hw_soft_reset_restores_reset_values)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);
    icm42688_model_set_accel(&model, 1, 2, 3);
    ASSERT_EQ(model.hw.regs[0x50], 0x26);
    ASSERT_EQ(model.hw.regs[0x4E], 0x0F);

    ASSERT_EQ(icm42688_soft_reset(&dev), ICM42688_OK);

    ASSERT_EQ(model.reset_count, 1);
    assert_reset_values();
}

/* p.77 and p.78: GYRO_CONFIG0 and ACCEL_CONFIG0 reset to 0x06, FS_SEL 0, so
 * +/-2000 dps and +/-16 g. Tables 1 and 2: 16.4 LSB/dps and 2048 LSB/g. The chip
 * scales by those after a reset, whatever init had set. */
TEST(test_hw_readings_after_a_soft_reset_use_the_reset_ranges)
{
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_range = ICM42688_ACC_RANGE_2G;
    cfg.gyro_range = ICM42688_GYRO_RANGE_250DPS;
    icm42688_model_init(&model);
    ASSERT_EQ(icm42688_init(&dev, &hw_spi, &cfg), ICM42688_OK);

    ASSERT_EQ(icm42688_soft_reset(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_LN),
              ICM42688_OK);
    icm42688_model_set_accel(&model, 2048, 0, 0);
    icm42688_model_set_gyro(&model, 164, 0, 0);
    icm42688_model_set_temp(&model, 0);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.001f);
    ASSERT_NEAR(data.gx_dps, 10.0f, 0.01f);
}

/* Init after the reset programs the ranges again, and the scale follows. */
TEST(test_hw_init_after_a_soft_reset_brings_the_ranges_back)
{
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_range = ICM42688_ACC_RANGE_2G;
    cfg.gyro_range = ICM42688_GYRO_RANGE_250DPS;
    icm42688_model_init(&model);
    ASSERT_EQ(icm42688_init(&dev, &hw_spi, &cfg), ICM42688_OK);
    ASSERT_EQ(icm42688_soft_reset(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_init(&dev, &hw_spi, &cfg), ICM42688_OK);

    icm42688_model_set_accel(&model, 16384, 0, 0);
    icm42688_model_set_gyro(&model, 131, 0, 0);
    icm42688_model_set_temp(&model, 0);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.001f);
    ASSERT_NEAR(data.gx_dps, 1.0f, 0.01f);
}

/* p.77 and p.78: FS_SEL in [7:5], ODR in [3:0]. p.76: GYRO_MODE [3:2] and
 * ACCEL_MODE [1:0], 11 for Low Noise. Init asks for 2000 dps, 8 g, 1 kHz. */
TEST(test_hw_init_programs_the_sensors)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);

    ASSERT_EQ_HEX(spi_read_reg(0x4F), 0x06);
    ASSERT_EQ_HEX(spi_read_reg(0x50), 0x26);
    ASSERT_EQ_HEX(spi_read_reg(0x4E), 0x0F);
}

/* p.58: "software should change ODR to a value of 500Hz or lower, because
 * accelerometer LP mode does not support ODR values above 500Hz". A soft reset
 * returns the chip to 1 kHz, whatever init set. */
TEST(test_hw_lp_refused_at_the_rate_a_soft_reset_leaves)
{
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_odr = ICM42688_ODR_200HZ;
    icm42688_model_init(&model);
    ASSERT_EQ(icm42688_init(&dev, &hw_spi, &cfg), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
              ICM42688_OK);

    ASSERT_EQ(icm42688_soft_reset(&dev), ICM42688_OK);

    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
              ICM42688_ERR_PARAM);
}

/* p.69: RESET_DONE_INT "automatically sets to 1 when software reset is
 * complete. The bit clears to 0 after the register has been read." */
TEST(test_hw_int_status_clears_on_read)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);
    ASSERT_EQ(spi_read_reg(0x2D), 0x10);
    ASSERT_EQ(spi_read_reg(0x2D), 0x00);

    ASSERT_EQ(icm42688_soft_reset(&dev), ICM42688_OK);
    ASSERT_EQ(spi_read_reg(0x2D), 0x10);
}

/* Section 14: WHO_AM_I is "Serial IF: R", the sense registers "SYNCR" */
TEST(test_hw_writes_leave_read_only_registers)
{
    icm42688_model_init(&model);

    spi_write_reg(0x75, 0x00);
    ASSERT_EQ(spi_read_reg(0x75), 0x47);

    spi_write_reg(0x1D, 0x12);
    spi_write_reg(0x24, 0x34);
    ASSERT_EQ(spi_read_reg(0x1D), 0x80);
    ASSERT_EQ(spi_read_reg(0x24), 0x00);
}

/* Section 14.59: REG_BANK_SEL "is accessible from all register banks". The
 * rest of bank 0 is only there with bank 0 selected, which is why init selects
 * it first. Bank 1 is not modelled and reads as 0. */
TEST(test_hw_init_selects_bank_0_first)
{
    icm42688_model_init(&model);
    spi_write_reg(0x76, 0x01);
    ASSERT_EQ(spi_read_reg(0x76), 0x01);
    ASSERT_EQ(spi_read_reg(0x75), 0x00);

    spi_write_reg(0x4E, 0x0F);
    ASSERT_EQ(model.hw.regs[0x4E], 0x00);

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    ASSERT_EQ(icm42688_init(&dev, &hw_spi, &cfg), ICM42688_OK);
    ASSERT_EQ(spi_read_reg(0x76), 0x00);
}

/* High bytes of the sense registers reset to 0x80, low bytes to 0x00
 * (DS-000347 Rev 1.6 p.65 to p.68) */
TEST(test_hw_sense_registers_reset_to_no_sample)
{
    icm42688_model_init(&model);
    for (int reg = ICM_REG_TEMP_DATA1; reg <= ICM_REG_GYRO_DATA_Z1; reg += 2) {
        ASSERT_EQ(model.hw.regs[reg], 0x80);
        ASSERT_EQ(model.hw.regs[reg + 1], 0x00);
    }
}

/* -32768 until the first sample (p.60), so init followed at once by a read
 * has nothing to report */
TEST(test_hw_read_before_first_sample)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);

    float temp = 0.0f;
    ASSERT_EQ(icm42688_read_temperature(&dev, &temp), ICM42688_ERR_NOT_READY);
}

/* Once all sensors have a sample the read goes through */
TEST(test_hw_read_after_first_sample)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);
    icm42688_model_set_temp(&model, 0);
    icm42688_model_set_accel(&model, 4096, 0, -4096);
    icm42688_model_set_gyro(&model, 164, 0, 0);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.01f);      /* p.66: 0 is 25 C */
    ASSERT_NEAR(data.ax_g, 1.0f, 0.001f);               /* 4096 LSB/g at 8 g */
    ASSERT_NEAR(data.az_g, -1.0f, 0.001f);
    ASSERT_NEAR(data.gx_dps, 10.0f, 0.01f);             /* 16.4 LSB/dps */
}

/* Accel only: the gyro stays at -32768 and must not block the read */
TEST(test_hw_read_gyro_off_after_reset_values)
{
    icm42688_t dev;
    ASSERT_EQ(hw_setup(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_OFF),
              ICM42688_OK);
    icm42688_model_set_temp(&model, 0);
    icm42688_model_set_accel(&model, 4096, 0, 0);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.001f);
}

TEST_MAIN("icm42688 hw model")
    RUN(test_hw_power_on_reset_values);
    RUN(test_hw_soft_reset_restores_reset_values);
    RUN(test_hw_readings_after_a_soft_reset_use_the_reset_ranges);
    RUN(test_hw_init_after_a_soft_reset_brings_the_ranges_back);
    RUN(test_hw_init_programs_the_sensors);
    RUN(test_hw_lp_refused_at_the_rate_a_soft_reset_leaves);
    RUN(test_hw_int_status_clears_on_read);
    RUN(test_hw_writes_leave_read_only_registers);
    RUN(test_hw_init_selects_bank_0_first);
    RUN(test_hw_sense_registers_reset_to_no_sample);
    RUN(test_hw_read_before_first_sample);
    RUN(test_hw_read_after_first_sample);
    RUN(test_hw_read_gyro_off_after_reset_values);
TEST_MAIN_END()
