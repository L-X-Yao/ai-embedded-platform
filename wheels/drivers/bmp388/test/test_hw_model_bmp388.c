// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bmp388.h"
#include "bmp388.h"

static const bmp388_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
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

static void poke(uint8_t reg, uint8_t val)
{
    ASSERT_TRUE(hw_model_write_reg(NULL, 0x76, reg, val));
}

static uint8_t peek(uint8_t reg)
{
    uint8_t val = 0xA5;
    (void)hw_model_read_regs(NULL, 0x76, reg, &val, 1);
    return val;
}

/* p.39, softreset: "Triggers a reset, all user configuration settings are
 * overwritten with their default state". Table 24 has the defaults. */
TEST(test_hw_model_softreset_restores_the_defaults)
{
    hw_model_reset();
    poke(0x19, 0x40);  /* INT_CTRL */
    poke(0x1C, 0x2D);  /* OSR */
    poke(0x1D, 0x05);  /* ODR */
    poke(0x1F, 0x04);  /* CONFIG */
    poke(0x1B, 0x13);  /* forced, both channels */
    hw_model_advance_us(300000);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x60);

    poke(0x7E, 0xB6);
    hw_model_advance_us(HW_RESET_US);

    ASSERT_EQ_HEX(hw_regs[0x19], 0x02);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x02);
    ASSERT_EQ_HEX(hw_regs[0x1D], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x1F], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x1B], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x03], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x06], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x09], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x31], 0x90);  /* the NVM is no user setting */
}

/* p.11: "Completion of the power-on-reset or soft reset is indicated by the bit
 * por_detected[0]. The bit is cleared after reading." */
TEST(test_hw_model_por_detected)
{
    hw_model_reset();
    ASSERT_EQ_HEX(peek(0x10), 0x01);
    ASSERT_EQ_HEX(peek(0x10), 0x00);

    poke(0x7E, 0xB6);
    ASSERT_EQ_HEX(peek(0x10), 0x00);
    hw_model_advance_us(HW_RESET_US);
    ASSERT_EQ_HEX(peek(0x10), 0x01);
    ASSERT_EQ_HEX(peek(0x10), 0x00);
}

/* A write during the reset is lost, because the reset overwrites it */
TEST(test_hw_model_write_during_the_reset_is_lost)
{
    hw_model_reset();
    poke(0x7E, 0xB6);
    poke(0x1C, 0x2D);
    hw_model_advance_us(HW_RESET_US);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x02);
}

/* p.39: "Register will always read as 0x00" */
TEST(test_hw_model_cmd_reads_zero)
{
    hw_model_reset();
    poke(0x7E, 0xB0);
    ASSERT_EQ_HEX(peek(0x7E), 0x00);
}

/* Table 24: these are read only */
TEST(test_hw_model_read_only_registers)
{
    hw_model_reset();
    poke(0x00, 0x00);
    poke(0x03, 0x60);
    poke(0x04, 0x11);
    poke(0x31, 0x00);
    ASSERT_EQ_HEX(hw_regs[0x00], 0x50);
    ASSERT_EQ_HEX(hw_regs[0x03], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x04], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x31], 0x90);
}

/* p.27: Tconv = 234 us + press_en * (392 us + 2^osr_p * 2000 us)
 *               + temp_en * (313 us + 2^osr_t * 2000 us)
 * p.13, Figure 5: pressure first. p.11: "When the measurement is finished, the
 * sensor returns to sleep mode". Table 40: 010011 is forced with both on. */
TEST(test_hw_model_measurement_time)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 8000000);
    poke(0x1C, 0x00);  /* x1, x1 */
    poke(0x1B, 0x13);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0);
    ASSERT_EQ(hw_regs[0x1B] & 0x30, 0x10);

    hw_model_advance_us(234u + 392u + 2000u - 1u);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0);
    hw_model_advance_us(1);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x20);
    ASSERT_EQ_HEX(hw_regs[0x06], (6000000u >> 16) & 0xFFu);
    ASSERT_EQ_HEX(hw_regs[0x09], 0x80);

    hw_model_advance_us(313u + 2000u - 1u);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x20);
    hw_model_advance_us(1);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x60);
    ASSERT_EQ_HEX(hw_regs[0x09], (8000000u >> 16) & 0xFFu);
    ASSERT_EQ_HEX(hw_regs[0x1B], 0x03);
}

/* The same formula, with oversampling and with one channel off */
TEST(test_hw_model_measurement_time_follows_the_settings)
{
    hw_model_reset();
    poke(0x1C, 0x0D);  /* osr_t 001 = x2, osr_p 101 = x32 */
    poke(0x1B, 0x13);
    hw_model_advance_us(234u + 392u + 32u * 2000u + 313u + 2u * 2000u - 1u);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x20);
    hw_model_advance_us(1);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x60);

    hw_model_reset();
    poke(0x1C, 0x00);
    poke(0x1B, 0x12);  /* temperature alone */
    hw_model_advance_us(234u + 313u + 2000u - 1u);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0);
    hw_model_advance_us(1);
    ASSERT_EQ(hw_regs[0x03] & 0x60, 0x40);
}

/* Table 22, p.26: x1 and x1 take 4.82 ms typical, and the driver must wait them */
TEST(test_hw_read_waits_out_the_conversion)
{
    hw_model_reset();
    hw_model_inject_adc(6000000, 7000000);
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);

    uint32_t start = hw_now_us;
    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
    ASSERT_GE(hw_now_us - start, 4820u);
    ASSERT_NEAR(data.temperature_c, manual_t_lin(7000000), 0.01);
}

/* p.39: the reset overwrites OSR, but the driver still reports the configuration
 * it was given, so the configured oversampling must survive bmp388_reset */
TEST(test_hw_reset_keeps_the_configured_oversampling)
{
    bmp388_cfg_t cfg = default_cfg;
    cfg.osrs_t = BMP388_OVERSAMPLING_16;
    cfg.osrs_p = BMP388_OVERSAMPLING_8;

    hw_model_reset();
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &cfg), BMP388_OK);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x23);  /* 100 011 */

    ASSERT_EQ(bmp388_reset(&dev), BMP388_OK);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x23);

    hw_model_inject_adc(6000000, 7000000);
    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&dev, &data), BMP388_OK);
    ASSERT_NEAR(data.temperature_c, manual_t_lin(7000000), 0.01);
}

/* p.11: por_detected marks the end of the reset. It is still set from power-up
 * until someone reads it, and init does not, so a set bit alone proves nothing. */
TEST(test_hw_reset_waits_for_the_reset_to_finish)
{
    hw_model_reset();
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);
    ASSERT_TRUE(hw_regs[0x10] & 0x01);

    uint32_t start = hw_now_us;
    ASSERT_EQ(bmp388_reset(&dev), BMP388_OK);
    ASSERT_GE(hw_now_us - start, HW_RESET_US);
    ASSERT_EQ(hw_regs[0x10] & 0x01, 0);  /* the driver read it */
}

/* The manual gives no time, so the wait goes by the flag and not by the clock */
TEST(test_hw_reset_polls_a_slow_reset)
{
    hw_model_reset();
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);
    hw_reset_us = 20000;

    uint32_t start = hw_now_us;
    ASSERT_EQ(bmp388_reset(&dev), BMP388_OK);
    ASSERT_GE(hw_now_us - start, 20000u);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x00);
}

TEST(test_hw_reset_that_never_finishes_times_out)
{
    hw_model_reset();
    ASSERT_EQ(bmp388_init(&dev, &hw_i2c, &default_cfg), BMP388_OK);
    hw_reset_stuck = true;
    ASSERT_EQ(bmp388_reset(&dev), BMP388_ERR_TIMEOUT);
}

/* Manual Table 24, p.31: no flag is set before a measurement */
TEST(test_hw_model_por_status)
{
    hw_model_reset();
    ASSERT_EQ_HEX(hw_regs[0x03], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x06], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x09], 0x80);
    ASSERT_EQ_HEX(hw_regs[0x1C], 0x02);
    ASSERT_EQ_HEX(hw_regs[0x10], 0x01);
    ASSERT_EQ_HEX(hw_regs[0x15], 0x01);
    ASSERT_EQ_HEX(hw_regs[0x17], 0x02);
    ASSERT_EQ_HEX(hw_regs[0x18], 0x02);
    ASSERT_EQ_HEX(hw_regs[0x19], 0x02);
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
    RUN(test_hw_model_softreset_restores_the_defaults);
    RUN(test_hw_model_por_detected);
    RUN(test_hw_model_write_during_the_reset_is_lost);
    RUN(test_hw_model_cmd_reads_zero);
    RUN(test_hw_model_read_only_registers);
    RUN(test_hw_model_measurement_time);
    RUN(test_hw_model_measurement_time_follows_the_settings);
    RUN(test_hw_read_waits_out_the_conversion);
    RUN(test_hw_reset_keeps_the_configured_oversampling);
    RUN(test_hw_reset_waits_for_the_reset_to_finish);
    RUN(test_hw_reset_polls_a_slow_reset);
    RUN(test_hw_reset_that_never_finishes_times_out);
    RUN(test_hw_model_por_status);
    RUN(test_hw_both_measured);
    RUN(test_hw_read_waits_for_both_flags);
    RUN(test_hw_skip_pressure);
    RUN(test_hw_skip_temperature);
TEST_MAIN_END()
