// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_dps310.h"
#include "dps310.h"

static const dps310_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static dps310_t dev;
static dps310_cfg_t default_cfg = {
    .addr   = 0x77,
    .osrs_t = DPS310_OVERSAMPLING_1,
    .osrs_p = DPS310_OVERSAMPLING_1,
};

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(hw_regs[0x08], 0xC0);  /* MEAS_CFG reset value (p.32) */
    dps310_err_t e = dps310_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, DPS310_OK);
    /* Verify driver read the product ID */
    ASSERT_TRUE(hw_read_count > 0);
}

TEST(test_hw_read_25c_round_trip)
{
    hw_model_reset();
    hw_model_inject_temperature(25.0f);
    hw_model_inject_pressure(101000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 101000.0f, 50.0f);
}

TEST(test_hw_read_0c_round_trip)
{
    hw_model_reset();
    hw_model_inject_temperature(0.0f);
    hw_model_inject_pressure(100000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.01f);
    ASSERT_NEAR(data.pressure_pa, 100000.0f, 1.0f);
}

TEST(test_hw_read_negative_temp)
{
    hw_model_reset();
    hw_model_inject_temperature(-15.0f);
    hw_model_inject_pressure(95000.0f);
    dps310_init(&dev, &hw_i2c, &default_cfg);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, -15.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 95000.0f, 50.0f);
}

TEST(test_hw_cold_boot_waits_for_coefficients)
{
    hw_model_power_on();
    ASSERT_EQ(dps310_init(&dev, &hw_i2c, &default_cfg), DPS310_OK);
    hw_model_inject_temperature(25.0f);
    hw_model_inject_pressure(101000.0f);

    dps310_data_t data;
    ASSERT_EQ(dps310_read(&dev, &data), DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 101000.0f, 50.0f);
    ASSERT_TRUE(hw_uptime_ms >= HW_COEF_RDY_MS);
}

TEST(test_hw_reset_side_effect)
{
    dps310_cfg_t cfg = {
        .addr   = 0x77,
        .osrs_t = DPS310_OVERSAMPLING_16,
        .osrs_p = DPS310_OVERSAMPLING_16,
    };
    hw_model_reset();
    dps310_init(&dev, &hw_i2c, &cfg);

    dps310_err_t e = dps310_reset(&dev);
    ASSERT_EQ(e, DPS310_OK);
    /* p.36: the power-on sequence runs again, so configuration is back at
     * its reset value and both ready bits are low until start-up ends */
    ASSERT_EQ(hw_regs[0x06], 0x00);
    ASSERT_EQ(hw_regs[0x07], 0x00);
    ASSERT_EQ(hw_regs[0x09], 0x00);
    ASSERT_EQ(hw_regs[0x08], 0x00);
    hw_model_delay_ms(NULL, HW_COEF_RDY_MS);
    ASSERT_EQ(hw_regs[0x08], 0xC0);
}

TEST(test_hw_init_after_soft_reset)
{
    hw_model_reset();
    dps310_init(&dev, &hw_i2c, &default_cfg);
    dps310_reset(&dev);

    /* Table 8 counts start-up from a soft reset as well */
    ASSERT_EQ(dps310_init(&dev, &hw_i2c, &default_cfg), DPS310_OK);
    ASSERT_NEAR(dev.calib.c00, 100000.0f, 1.0f);
    ASSERT_TRUE(hw_uptime_ms >= HW_COEF_RDY_MS);
}

TEST(test_hw_soft_reset_needs_low_nibble)
{
    hw_model_reset();
    hw_model_write_reg(NULL, 0x77, 0x0C, 0x80);  /* FIFO_FLUSH only */
    ASSERT_EQ(hw_regs[0x08], 0xC0);
    hw_model_write_reg(NULL, 0x77, 0x0C, 0x09);  /* SOFT_RST, no flush */
    ASSERT_EQ(hw_regs[0x08], 0x00);
}

TEST(test_hw_meas_cfg_status_bits_read_only)
{
    hw_model_reset();
    hw_model_write_reg(NULL, 0x77, 0x08, 0x00);
    ASSERT_EQ(hw_regs[0x08], 0xC0);
}

TEST(test_hw_reading_result_clears_ready_flag)
{
    uint8_t buf[3];
    hw_model_reset();

    hw_model_write_reg(NULL, 0x77, 0x08, 0x02);
    ASSERT_EQ(hw_regs[0x08], 0xC2);  /* measuring, no flag yet */
    hw_model_delay_ms(NULL, 4);
    ASSERT_EQ(hw_regs[0x08], 0xE0);  /* TMP_RDY, MEAS_CTRL back to 000 */
    hw_model_read_regs(NULL, 0x77, 0x03, buf, 3);
    ASSERT_EQ(hw_regs[0x08], 0xC0);

    hw_model_write_reg(NULL, 0x77, 0x08, 0x01);
    ASSERT_EQ(hw_regs[0x08], 0xC1);
    hw_model_delay_ms(NULL, 4);
    ASSERT_EQ(hw_regs[0x08], 0xD0);  /* PRS_RDY, MEAS_CTRL back to 000 */
    hw_model_read_regs(NULL, 0x77, 0x00, buf, 3);
    ASSERT_EQ(hw_regs[0x08], 0xC0);
}

/* Table 16 (p.30), rounded up to whole ms: 3.6, 5.2, 8.4, 14.8, 27.6, 53.2,
 * 104.4 and 206.8 ms. "The temperature measurement time versus temperature
 * oversampling rate is similar". */
TEST(test_hw_measurement_time_follows_table_16)
{
    static const uint32_t ms[8] = { 4, 6, 9, 15, 28, 54, 105, 207 };

    for (uint8_t prc = 0; prc < 8; prc++) {
        hw_model_reset();
        hw_model_write_reg(NULL, 0x77, 0x06, prc);
        hw_model_write_reg(NULL, 0x77, 0x08, 0x01);
        hw_model_delay_ms(NULL, ms[prc] - 1u);
        ASSERT_EQ(hw_regs[0x08] & 0x10, 0);
        hw_model_delay_ms(NULL, 1);
        ASSERT_EQ(hw_regs[0x08] & 0x10, 0x10);

        hw_model_reset();
        hw_model_write_reg(NULL, 0x77, 0x07, prc);
        hw_model_write_reg(NULL, 0x77, 0x08, 0x02);
        hw_model_delay_ms(NULL, ms[prc] - 1u);
        ASSERT_EQ(hw_regs[0x08] & 0x20, 0);
        hw_model_delay_ms(NULL, 1);
        ASSERT_EQ(hw_regs[0x08] & 0x20, 0x20);
    }
}

/* Reset value 00H (p.25) until a measurement has ended */
TEST(test_hw_result_appears_when_the_measurement_ends)
{
    uint8_t buf[3] = { 0xAA, 0xAA, 0xAA };
    hw_model_reset();
    hw_model_inject_temperature(25.0f);

    hw_model_write_reg(NULL, 0x77, 0x08, 0x02);
    hw_model_read_regs(NULL, 0x77, 0x03, buf, 3);
    ASSERT_EQ(buf[0] | buf[1] | buf[2], 0);

    hw_model_delay_ms(NULL, 4);
    hw_model_read_regs(NULL, 0x77, 0x03, buf, 3);
    ASSERT_TRUE((buf[0] | buf[1] | buf[2]) != 0);
}

/* p.32: 011 and 100 are "na", and no measurement starts */
TEST(test_hw_undefined_meas_ctrl_starts_nothing)
{
    hw_model_reset();
    hw_model_write_reg(NULL, 0x77, 0x08, 0x03);
    hw_model_delay_ms(NULL, 300);
    ASSERT_EQ(hw_regs[0x08] & 0x30, 0);
}

TEST(test_hw_read_waits_for_both_measurements)
{
    dps310_cfg_t cfg = {
        .addr   = 0x77,
        .osrs_t = DPS310_OVERSAMPLING_64,
        .osrs_p = DPS310_OVERSAMPLING_64,
    };
    hw_model_reset();
    ASSERT_EQ(dps310_init(&dev, &hw_i2c, &cfg), DPS310_OK);

    uint32_t before = hw_uptime_ms;
    dps310_data_t data;
    ASSERT_EQ(dps310_read(&dev, &data), DPS310_OK);
    ASSERT_TRUE(hw_uptime_ms - before >= 210u);
}

/* Table 15 (p.25): these registers have only (r) fields */
TEST(test_hw_read_only_registers_ignore_writes)
{
    static const uint8_t regs[] = {
        0x00, 0x02, 0x03, 0x05, 0x0A, 0x0B, 0x0D, 0x10, 0x21, 0x28,
    };

    hw_model_reset();
    for (unsigned i = 0; i < sizeof(regs); i++) {
        uint8_t before = hw_regs[regs[i]];
        hw_model_write_reg(NULL, 0x77, regs[i], 0x5A);
        ASSERT_EQ(hw_regs[regs[i]], before);
    }
}

/* Table 15: bit 7 of PRS_CFG is "-", the other bits are (rw) */
TEST(test_hw_config_registers_keep_their_rw_fields)
{
    hw_model_reset();
    hw_model_write_reg(NULL, 0x77, 0x06, 0xFF);
    ASSERT_EQ_HEX(hw_regs[0x06], 0x7F);
    hw_model_write_reg(NULL, 0x77, 0x07, 0xFF);
    ASSERT_EQ_HEX(hw_regs[0x07], 0xFF);
    hw_model_write_reg(NULL, 0x77, 0x09, 0xFF);
    ASSERT_EQ_HEX(hw_regs[0x09], 0xFF);
}

/* Figure 4 (p.11): "No chip access", all pads are inputs, for the first
 * 2.5 ms. The model counts whole ms, so that is up to 3. */
TEST(test_hw_no_chip_access_after_power_on)
{
    uint8_t id = 0;
    hw_model_power_on();

    ASSERT_FALSE(hw_model_read_regs(NULL, 0x77, 0x0D, &id, 1));
    ASSERT_FALSE(hw_model_write_reg(NULL, 0x77, 0x06, 0x01));
    hw_model_delay_ms(NULL, 2);
    ASSERT_FALSE(hw_model_read_regs(NULL, 0x77, 0x0D, &id, 1));
    hw_model_delay_ms(NULL, 1);
    ASSERT_TRUE(hw_model_read_regs(NULL, 0x77, 0x0D, &id, 1));
    ASSERT_EQ_HEX(id, 0x10);
}

/* p.36: "A soft reset will run though the same sequences as in power-on
 * reset." */
TEST(test_hw_no_chip_access_after_soft_reset)
{
    uint8_t id = 0;
    hw_model_reset();

    ASSERT_TRUE(hw_model_write_reg(NULL, 0x77, 0x0C, 0x09));
    ASSERT_FALSE(hw_model_read_regs(NULL, 0x77, 0x0D, &id, 1));
    hw_model_delay_ms(NULL, 3);
    ASSERT_TRUE(hw_model_read_regs(NULL, 0x77, 0x0D, &id, 1));
}

TEST(test_hw_tmp_ext_follows_coef_source)
{
    hw_model_reset();
    ASSERT_EQ(dps310_init(&dev, &hw_i2c, &default_cfg), DPS310_OK);
    ASSERT_EQ(hw_regs[0x07] & 0x80, 0x80);

    hw_model_reset();
    hw_regs[0x28] = 0x00;  /* a part calibrated against the ASIC sensor */
    ASSERT_EQ(dps310_init(&dev, &hw_i2c, &default_cfg), DPS310_OK);
    ASSERT_EQ(hw_regs[0x07] & 0x80, 0);
}

TEST(test_hw_wrong_id)
{
    hw_model_reset();
    hw_regs[0x0D] = 0xFF;
    dps310_err_t e = dps310_init(&dev, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, DPS310_ERR_ID);
}

TEST_MAIN("dps310_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_25c_round_trip);
    RUN(test_hw_read_0c_round_trip);
    RUN(test_hw_read_negative_temp);
    RUN(test_hw_cold_boot_waits_for_coefficients);
    RUN(test_hw_reset_side_effect);
    RUN(test_hw_init_after_soft_reset);
    RUN(test_hw_soft_reset_needs_low_nibble);
    RUN(test_hw_meas_cfg_status_bits_read_only);
    RUN(test_hw_reading_result_clears_ready_flag);
    RUN(test_hw_measurement_time_follows_table_16);
    RUN(test_hw_result_appears_when_the_measurement_ends);
    RUN(test_hw_undefined_meas_ctrl_starts_nothing);
    RUN(test_hw_read_waits_for_both_measurements);
    RUN(test_hw_read_only_registers_ignore_writes);
    RUN(test_hw_config_registers_keep_their_rw_fields);
    RUN(test_hw_no_chip_access_after_power_on);
    RUN(test_hw_no_chip_access_after_soft_reset);
    RUN(test_hw_tmp_ext_follows_coef_source);
    RUN(test_hw_wrong_id);
TEST_MAIN_END()
