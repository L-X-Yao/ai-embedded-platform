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
    RUN(test_hw_sense_registers_reset_to_no_sample);
    RUN(test_hw_read_before_first_sample);
    RUN(test_hw_read_after_first_sample);
    RUN(test_hw_read_gyro_off_after_reset_values);
TEST_MAIN_END()
