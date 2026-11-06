// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ads1115.c — tests using hardware register model
 *
 * Validates ADS1115 driver against a register model.
 * The model enforces conversion register read-only, config register
 * OS bit triggering a timed conversion, and 16-bit register layout.
 * Expected values come from TI SBAS444E; the page is next to each test.
 */

#include "test_harness.h"
#include "hw_model_ads1115.h"
#include "../ads1115.h"
#include <math.h>

static ads1115_model_t model;

/* write_reg not used by ADS1115 driver (it uses write_regs for 16-bit) */
static bool dummy_write_reg(void *ctx, uint8_t dev_addr,
                            uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)val;
    return true;
}

static const ads1115_i2c_t model_i2c = {
    .write_reg  = dummy_write_reg,
    .read_regs  = ads1115_model_read_regs,
    .write_regs = ads1115_model_write_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static const ads1115_cfg_t default_cfg = {
    .addr = ADS1115_ADDR_GND,
    .mux  = ADS1115_MUX_AIN0_GND,
    .pga  = ADS1115_PGA_2048,
    .rate = ADS1115_RATE_128,
};

static void setup(void)
{
    ads1115_model_init(&model);
}

static uint16_t reg_read(uint8_t reg)
{
    uint8_t b[2] = {0, 0};
    ads1115_model_read_regs(&model, ADS1115_ADDR_GND, reg, b, 2);
    return (uint16_t)((uint16_t)b[0] << 8 | b[1]);
}

static void reg_write(uint8_t reg, uint16_t val)
{
    uint8_t b[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    ads1115_model_write_regs(&model, ADS1115_ADDR_GND, reg, b, 2);
}

/* Counts the config reads that came back with OS = 0, device busy. */
static int busy_reads;

static bool counting_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                               uint8_t *buf, uint16_t len)
{
    bool ok = ads1115_model_read_regs(ctx, dev_addr, reg, buf, len);
    if (reg == ADS1115_REG_CONFIG && !(buf[0] & 0x80))
        busy_reads++;
    return ok;
}

static const ads1115_i2c_t counting_i2c = {
    .write_reg  = dummy_write_reg,
    .read_regs  = counting_read_regs,
    .write_regs = ads1115_model_write_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

/* --- what the manual says about the registers -------------------------- */

/* p.24 "[reset = 0000h]", p.25 "[reset = 8583h]",
 * p.27 "Lo_thresh ... [reset = 8000h] and Hi_thresh ... [reset = 7FFFh]" */
TEST(test_reset_values)
{
    setup();
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONVERSION), 0x0000);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONFIG), 0x8583);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_LO_THRESH), 0x8000);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_HI_THRESH), 0x7FFF);
}

/* p.25 "When writing: 0b : No effect" */
TEST(test_writing_os_zero_has_no_effect)
{
    setup();
    reg_write(ADS1115_REG_CONFIG, 0x0583);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONFIG), 0x8583);
    ASSERT_EQ(model.conversion_count, 0);
}

/* p.18 "the device powers up in approximately 25 us, resets the OS bit
 * to 0b, and starts a single conversion" */
TEST(test_os_reads_zero_during_conversion)
{
    setup();
    reg_write(ADS1115_REG_CONFIG, 0x8583);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONFIG), 0x0583);
}

/* p.24 "contains the result of the last conversion": a running conversion
 * does not show until it ends. */
TEST(test_conversion_register_keeps_last_result_while_busy)
{
    setup();
    ads1115_model_set_adc(&model, 1234);
    reg_write(ADS1115_REG_CONFIG, 0x8583);
    ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONVERSION), 0x0000);
}

/* The poll loop has to run: the device is busy after the start. */
TEST(test_driver_polls_while_busy)
{
    setup();
    busy_reads = 0;
    ads1115_model_set_adc(&model, 1234);
    ads1115_t dev;
    ads1115_init(&dev, &counting_i2c, &default_cfg);

    int16_t raw = 0;
    ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    ASSERT_EQ(raw, 1234);
    ASSERT_GT(busy_reads, 0);
}

/* p.15 "the conversion time is equal to 1 / DR", DR codes from p.26 */
TEST(test_conversion_lasts_one_over_dr)
{
    static const uint32_t sps[8] = {8, 16, 32, 64, 128, 250, 475, 860};
    for (uint16_t dr = 0; dr < 8; dr++) {
        setup();
        uint32_t period_us = 1000000u / sps[dr];
        reg_write(ADS1115_REG_CONFIG, (uint16_t)(0x8503u | (uint16_t)(dr << 5)));

        ads1115_model_delay_ms(&model, (period_us - 900u) / 1000u);
        ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONFIG) & 0x8000, 0);

        ads1115_model_delay_ms(&model, 2);
        ASSERT_EQ_HEX(reg_read(ADS1115_REG_CONFIG) & 0x8000, 0x8000);
    }
}

/* p.18 "Writing a 1b to the OS bit while a conversion is ongoing has no
 * effect." */
TEST(test_start_during_conversion_has_no_effect)
{
    setup();
    reg_write(ADS1115_REG_CONFIG, 0x8583);
    uint32_t done = model.done_us;

    ads1115_model_delay_ms(&model, 4);
    reg_write(ADS1115_REG_CONFIG, 0x8583);
    ASSERT_EQ(model.conversion_count, 1);
    ASSERT_EQ(model.done_us, done);
}

/* p.26 MODE 0b is continuous-conversion, so the OS start of p.25, "when in
 * power-down state", does not apply. */
TEST(test_start_needs_single_shot_mode)
{
    setup();
    reg_write(ADS1115_REG_CONFIG, 0x8483);
    ASSERT_EQ(model.conversion_count, 0);
}

/* p.15 "+-2.048V 62.5uV": the model's default 1000 counts */
TEST(test_default_adc_is_62_5_mv)
{
    setup();
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    float mv = 0.0f;
    ASSERT_EQ(ads1115_read_voltage(&dev, &mv), ADS1115_OK);
    ASSERT_NEAR(mv, 62.5f, 0.001f);
}

static const ads1115_i2c_t delay_i2c = {
    .write_reg  = dummy_write_reg,
    .read_regs  = ads1115_model_read_regs,
    .write_regs = ads1115_model_write_regs,
    .delay_ms   = ads1115_model_delay_ms,
    .ctx        = &model,
};

/* p.15 8 SPS takes 125 ms. p.5 "Data rate variation ... -10% ... 10%". */
TEST(test_read_8sps_with_delay)
{
    ads1115_cfg_t cfg = default_cfg;
    cfg.rate = ADS1115_RATE_8;

    for (uint32_t slow = 0; slow <= 10; slow += 10) {
        setup();
        model.slow_percent = slow;
        ads1115_model_set_adc(&model, -321);
        ads1115_t dev;
        ads1115_init(&dev, &delay_i2c, &cfg);

        int16_t raw = 0;
        ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
        ASSERT_EQ(raw, -321);
        ASSERT_GE(model.now_us, 125000u);
    }
}

/* The header lets delay_ms be NULL, so the same conversion has to finish
 * with bus reads alone. */
TEST(test_read_8sps_without_delay)
{
    ads1115_cfg_t cfg = default_cfg;
    cfg.rate = ADS1115_RATE_8;

    for (uint32_t slow = 0; slow <= 10; slow += 10) {
        setup();
        model.slow_percent = slow;
        ads1115_model_set_adc(&model, 4321);
        ads1115_t dev;
        ads1115_init(&dev, &model_i2c, &cfg);

        int16_t raw = 0;
        ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
        ASSERT_EQ(raw, 4321);
    }
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    ads1115_t dev;
    ads1115_err_t err = ads1115_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, ADS1115_OK);
}

TEST(test_init_null_param)
{
    setup();
    ads1115_t dev;
    ads1115_err_t err = ads1115_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, ADS1115_ERR_PARAM);
}

TEST(test_init_invalid_pga)
{
    setup();
    ads1115_t dev;
    ads1115_cfg_t bad_cfg = default_cfg;
    bad_cfg.pga = (ads1115_pga_t)99;
    ads1115_err_t err = ads1115_init(&dev, &model_i2c, &bad_cfg);
    ASSERT_EQ(err, ADS1115_ERR_PARAM);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_raw_positive)
{
    setup();
    ads1115_model_set_adc(&model, 1000);
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1115_err_t err = ads1115_read_raw(&dev, &raw);
    ASSERT_EQ(err, ADS1115_OK);
    ASSERT_EQ(raw, 1000);
}

TEST(test_read_raw_negative)
{
    setup();
    ads1115_model_set_adc(&model, -5000);
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1115_read_raw(&dev, &raw);
    ASSERT_EQ(raw, -5000);
}

TEST(test_read_voltage_positive)
{
    setup();
    /* At PGA_2048: scale = 2048/32768 = 0.0625 mV/LSB */
    /* 16000 counts * 0.0625 = 1000 mV */
    ads1115_model_set_adc(&model, 16000);
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    float mv;
    ads1115_err_t err = ads1115_read_voltage(&dev, &mv);
    ASSERT_EQ(err, ADS1115_OK);
    ASSERT_NEAR(mv, 1000.0f, 1.0f);
}

TEST(test_read_triggers_conversion)
{
    setup();
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    int before = model.conversion_count;
    int16_t raw;
    ads1115_read_raw(&dev, &raw);
    ASSERT_EQ(model.conversion_count, before + 1);
}

/* --- config tests ------------------------------------------------------ */

TEST(test_set_channel)
{
    setup();
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    ads1115_err_t err = ads1115_set_channel(&dev, ADS1115_MUX_AIN2_GND);
    ASSERT_EQ(err, ADS1115_OK);
    ASSERT_EQ((int)dev.cfg.mux, (int)ADS1115_MUX_AIN2_GND);
}

TEST(test_conversion_register_readonly)
{
    setup();
    ads1115_model_set_adc(&model, 1234);
    /* Trigger conversion to populate */
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);
    int16_t raw;
    ads1115_read_raw(&dev, &raw);

    /* Try to overwrite conversion register via model */
    uint8_t conv_slot = ADS_SLOT(ADS1115_REG_CONVERSION);
    uint8_t original_msb = model.hw.regs[conv_slot];
    hw_model__write_reg(&model.hw, conv_slot, 0xFF);
    ASSERT_EQ(model.hw.regs[conv_slot], original_msb);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    ads1115_model_set_adc(&model, 2048);
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    int16_t r1, r2;
    ads1115_read_raw(&dev, &r1);
    ads1115_read_raw(&dev, &r2);
    ASSERT_EQ(r1, r2);
    ASSERT_EQ(model.conversion_count, 2);
}

TEST(test_different_adc_values)
{
    setup();
    ads1115_t dev;
    ads1115_init(&dev, &model_i2c, &default_cfg);

    ads1115_model_set_adc(&model, 1000);
    float mv1;
    ads1115_read_voltage(&dev, &mv1);

    ads1115_model_set_adc(&model, 2000);
    float mv2;
    ads1115_read_voltage(&dev, &mv2);

    ASSERT_TRUE(mv2 > mv1);
}

TEST_MAIN("ads1115 hardware model")
    RUN(test_reset_values);
    RUN(test_writing_os_zero_has_no_effect);
    RUN(test_os_reads_zero_during_conversion);
    RUN(test_conversion_register_keeps_last_result_while_busy);
    RUN(test_driver_polls_while_busy);
    RUN(test_conversion_lasts_one_over_dr);
    RUN(test_start_during_conversion_has_no_effect);
    RUN(test_start_needs_single_shot_mode);
    RUN(test_default_adc_is_62_5_mv);
    RUN(test_read_8sps_with_delay);
    RUN(test_read_8sps_without_delay);
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_invalid_pga);
    RUN(test_read_raw_positive);
    RUN(test_read_raw_negative);
    RUN(test_read_voltage_positive);
    RUN(test_read_triggers_conversion);
    RUN(test_set_channel);
    RUN(test_conversion_register_readonly);
    RUN(test_multiple_reads_consistent);
    RUN(test_different_adc_values);
TEST_MAIN_END()
