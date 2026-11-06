// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bmp388.h"
#include "bmp388.h"

static const bmp388_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static bmp388_t dev;
static bmp388_cfg_t default_cfg = {
    .addr = 0x76,
    .osrs_t = BMP388_OVERSAMPLING_1,
    .osrs_p = BMP388_OVERSAMPLING_1,
};

/* Manual 9.1 and 9.2 in double, from the NVM values hw_model_reset plants */
static double manual_t_lin(uint32_t raw)
{
    double par_t1 = 26000.0 * 256.0;
    double par_t2 = 18000.0 / 1073741824.0;     /* 2^30 */
    double par_t3 = -10.0 / 281474976710656.0;  /* 2^48 */
    double d = (double)raw - par_t1;
    return d * par_t2 + d * d * par_t3;
}

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    bmp388_err_t e = bmp388_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_OK);
    /* Verify driver read the chip ID */
    ASSERT_TRUE(hw_read_count > 0);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    bmp388_init(&dev, &hw_i2c, &default_cfg);

    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_OK);
    /* Verify compensated values are in physically valid range */
    ASSERT_TRUE(data.temperature_c >= -40.0f && data.temperature_c <= 85.0f);
    ASSERT_TRUE(data.pressure_pa >= 30000.0f && data.pressure_pa <= 125000.0f);
}

TEST(test_hw_reset_side_effect)
{
    hw_model_reset();
    bmp388_init(&dev, &hw_i2c, &default_cfg);

    bmp388_err_t e = bmp388_reset(&dev);
    ASSERT_EQ(e, BMP388_OK);
    /* After reset, drdy should still be asserted by hw_model */
    ASSERT_TRUE(hw_regs[0x03] & 0x20);
}

/* Manual Table 24, p.31: no flag is set before a measurement */
TEST(test_hw_model_por_status)
{
    hw_model_reset();
    ASSERT_EQ_HEX(hw_regs[0x03], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x06], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x09], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x02);
}

/* Table 40, p.36: 01 0011 is forced mode, temp_en and press_en */
TEST(test_hw_both_measured)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);
    ASSERT_EQ(hw_pwr_ctrl_len, 0);

    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
    ASSERT_EQ(hw_pwr_ctrl_len, 1);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[0], 0x13);
    ASSERT_EQ(hw_press_measured, 1);
    ASSERT_EQ(hw_temp_measured, 1);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0);  /* the burst read cleared both flags */
}

/* p.32: each result has its own flag, and the manual never says drdy_press
 * covers the temperature, so the read must wait for both */
TEST(test_hw_read_waits_for_both_flags)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 7000000);
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);
    hw_temp_late_polls = 3;

    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
    ASSERT_NEAR(data.temperature_c, manual_t_lin(7000000), 0.01);
}

/* p.14: "Pressure measurement can be enabled or skipped", useful "if BMP388 is
 * used as temperature sensor". Forced mode with temp_en alone is 01 0010. */
TEST(test_hw_skip_pressure)
{
    bmp388_cfg_t cfg = default_cfg;
    cfg.osrs_p = BMP388_OVERSAMPLING_SKIP;

    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &cfg), BMP388_OK);

    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
    ASSERT_EQ(hw_pwr_ctrl_len, 1);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[0], 0x12);
    ASSERT_EQ(hw_press_measured, 0);
    ASSERT_EQ(hw_temp_measured, 1);
    ASSERT_NEAR(data.temperature_c, manual_t_lin(8000000), 0.01);
    ASSERT_NEAR(data.pressure_pa, 0.0f, 0.0f);
}

/* p.14: "Temperature measurement can be enabled or skipped", useful "to measure
 * pressure extremely rapidly". 9.2 and 9.3: the pressure formula needs t_lin, so
 * init measures temperature once, 01 0010, and each read is 01 0001. */
TEST(test_hw_skip_temperature)
{
    bmp388_cfg_t cfg = default_cfg;
    cfg.osrs_t = BMP388_OVERSAMPLING_SKIP;

    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &cfg), BMP388_OK);
    ASSERT_EQ(hw_pwr_ctrl_len, 1);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[0], 0x12);
    ASSERT_EQ(hw_temp_measured, 1);
    ASSERT_EQ(hw_press_measured, 0);

    hw_model_inject_adc(6100000, 9000000);
    for (int i = 0; i < 3; i++)
    {
        bmp388_data_t data;
        ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
        ASSERT_NEAR(data.temperature_c, manual_t_lin(8000000), 0.01);
    }
    ASSERT_EQ(hw_pwr_ctrl_len, 4);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[1], 0x11);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[2], 0x11);
    ASSERT_EQ_HEX(hw_pwr_ctrl_log[3], 0x11);
    ASSERT_EQ(hw_temp_measured, 1);
    ASSERT_EQ(hw_press_measured, 3);
}

TEST_MAIN("bmp388_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_reset_side_effect);
    RUN(test_hw_model_por_status);
    RUN(test_hw_both_measured);
    RUN(test_hw_read_waits_for_both_flags);
    RUN(test_hw_skip_pressure);
    RUN(test_hw_skip_temperature);
TEST_MAIN_END()
