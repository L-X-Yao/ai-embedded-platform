// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ads1015.c — tests using hardware register model
 *
 * Validates ADS1015 driver against a register model.
 * The model enforces 12-bit left-justified conversion results,
 * config register OS bit triggering, and read-only conversion register.
 */

#include "test_harness.h"
#include "hw_model_ads1015.h"
#include "../ads1015.h"
#include <math.h>

static ads1015_model_t model;

/* write_reg not used by ADS1015 driver (it uses write_regs for 16-bit) */
static bool dummy_write_reg(void *ctx, uint8_t dev_addr,
                            uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)reg; (void)val;
    return true;
}

static const ads1015_i2c_t model_i2c = {
    .write_reg  = dummy_write_reg,
    .read_regs  = ads1015_model_read_regs,
    .write_regs = ads1015_model_write_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static const ads1015_cfg_t default_cfg = {
    .addr = ADS1015_ADDR_GND,
    .pga  = ADS1015_PGA_2048,
    .rate = ADS1015_RATE_1600,
};

static void setup(void)
{
    ads1015_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    ads1015_t dev;
    ads1015_err_t err = ads1015_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, ADS1015_OK);
}

TEST(test_init_null_param)
{
    setup();
    ads1015_t dev;
    ads1015_err_t err = ads1015_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, ADS1015_ERR_PARAM);
}

TEST(test_init_invalid_pga)
{
    setup();
    ads1015_t dev;
    ads1015_cfg_t bad_cfg = default_cfg;
    bad_cfg.pga = (ads1015_pga_t)99;
    ads1015_err_t err = ads1015_init(&dev, &model_i2c, &bad_cfg);
    ASSERT_EQ(err, ADS1015_ERR_PARAM);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_raw_positive)
{
    setup();
    ads1015_model_set_adc(&model, 1000);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1015_err_t err = ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);
    ASSERT_EQ(err, ADS1015_OK);
    ASSERT_EQ(raw, 1000);
}

TEST(test_read_raw_negative)
{
    setup();
    ads1015_model_set_adc(&model, -1024);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);
    ASSERT_EQ(raw, -1024);
}

TEST(test_read_raw_max)
{
    setup();
    ads1015_model_set_adc(&model, 2047);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);
    ASSERT_EQ(raw, 2047);
}

TEST(test_read_raw_min)
{
    setup();
    ads1015_model_set_adc(&model, -2048);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);
    ASSERT_EQ(raw, -2048);
}

TEST(test_read_voltage_positive)
{
    setup();
    /* At PGA_2048: scale = 2048/2048 = 1.0 mV/LSB */
    /* 1000 counts * 1.0 = 1000 mV */
    ads1015_model_set_adc(&model, 1000);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    float mv;
    ads1015_err_t err = ads1015_read_single(&dev, ADS1015_MUX_AIN0_GND, &mv);
    ASSERT_EQ(err, ADS1015_OK);
    ASSERT_NEAR(mv, 1000.0f, 1.0f);
}

TEST(test_read_triggers_conversion)
{
    setup();
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int before = model.conversion_count;
    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);
    ASSERT_EQ(model.conversion_count, before + 1);
}

/* --- config tests ------------------------------------------------------ */

TEST(test_set_pga)
{
    setup();
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    ads1015_err_t err = ads1015_set_pga(&dev, ADS1015_PGA_4096);
    ASSERT_EQ(err, ADS1015_OK);
    ASSERT_EQ((int)dev.cfg.pga, (int)ADS1015_PGA_4096);
}

TEST(test_set_rate)
{
    setup();
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    ads1015_err_t err = ads1015_set_rate(&dev, ADS1015_RATE_3300);
    ASSERT_EQ(err, ADS1015_OK);
    ASSERT_EQ((int)dev.cfg.rate, (int)ADS1015_RATE_3300);
}

TEST(test_conversion_register_readonly)
{
    setup();
    ads1015_model_set_adc(&model, 1234);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);
    int16_t raw;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw);

    /* Try to overwrite conversion register via model */
    uint8_t conv_slot = ADS1015_SLOT(ADS1015_REG_CONVERSION);
    uint8_t original_msb = model.hw.regs[conv_slot];
    hw_model__write_reg(&model.hw, conv_slot, 0xFF);
    ASSERT_EQ(model.hw.regs[conv_slot], original_msb);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    ads1015_model_set_adc(&model, 500);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t r1, r2;
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &r1);
    ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &r2);
    ASSERT_EQ(r1, r2);
    ASSERT_EQ(model.conversion_count, 2);
}

TEST(test_different_adc_values)
{
    setup();
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    ads1015_model_set_adc(&model, 500);
    float mv1;
    ads1015_read_single(&dev, ADS1015_MUX_AIN0_GND, &mv1);

    ads1015_model_set_adc(&model, 1500);
    float mv2;
    ads1015_read_single(&dev, ADS1015_MUX_AIN0_GND, &mv2);

    ASSERT_TRUE(mv2 > mv1);
}

/* --- the device, from SBAS473F ----------------------------------------- */

static uint16_t read_reg(uint8_t reg)
{
    uint8_t buf[2] = { 0, 0 };

    (void)ads1015_model_read_regs(&model, ADS1015_ADDR_GND, reg, buf, 2);
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static void write_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)val };

    (void)ads1015_model_write_regs(&model, ADS1015_ADDR_GND, reg, buf, 2);
}

/* p.20: Conversion [reset = 0000h]. p.21: Config [reset = 8583h].
 * p.23: Lo_thresh [reset = 8000h], Hi_thresh [reset = 7FFFh]. */
TEST(test_reset_values)
{
    setup();
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONVERSION), 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG), 0x8583);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_LO_THRESH), 0x8000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_HI_THRESH), 0x7FFF);
}

/* p.23: bits 3:0 of the thresholds are RESERVED, R-0h and R-Fh */
TEST(test_threshold_reserved_bits_are_read_only)
{
    setup();
    write_reg(ADS1015_REG_LO_THRESH, 0x123F);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_LO_THRESH), 0x1230);
    write_reg(ADS1015_REG_HI_THRESH, 0x4560);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_HI_THRESH), 0x456F);
}

/* p.21, OS "When writing: 0b : No effect". The other fields still change. */
TEST(test_writing_os_0_has_no_effect)
{
    setup();
    write_reg(ADS1015_REG_CONFIG, 0x4383);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG), 0xC383);
    ASSERT_EQ(model.conversion_count, 0);
}

/* p.21, OS "When reading: 0b : Device is currently performing a conversion,
 * 1b : Device is not currently performing a conversion" */
TEST(test_os_reads_0_during_a_conversion)
{
    setup();
    model.conv_reads = 3;
    write_reg(ADS1015_REG_CONFIG, 0x8583);
    for (int i = 0; i < 3; i++)
        ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x8000);
}

/* p.14: the result comes when "conversion data are ready for retrieval" */
TEST(test_result_appears_when_the_conversion_ends)
{
    setup();
    model.conv_reads = 1;
    ads1015_model_set_adc(&model, 1000);

    write_reg(ADS1015_REG_CONFIG, 0x8583);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONVERSION), 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONVERSION), 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x8000);
    /* p.20: D[11:0] in 15:4, and bits 3:0 "Always reads back 0h" */
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONVERSION), 0x3E80);
}

/* p.14: "Writing a 1b to the OS bit while a conversion is ongoing has no
 * effect." */
TEST(test_os_write_during_a_conversion_has_no_effect)
{
    setup();
    model.conv_reads = 2;
    write_reg(ADS1015_REG_CONFIG, 0x8583);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x0000);

    write_reg(ADS1015_REG_CONFIG, 0x8583);
    ASSERT_EQ(model.conversion_count, 1);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x0000);
    ASSERT_EQ_HEX(read_reg(ADS1015_REG_CONFIG) & 0x8000u, 0x8000);
}

TEST(test_driver_waits_for_the_conversion)
{
    setup();
    model.conv_reads = 150;
    ads1015_model_set_adc(&model, -777);
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw = 0;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw), ADS1015_OK);
    ASSERT_EQ(raw, -777);
}

TEST(test_driver_gives_up_on_a_stuck_conversion)
{
    setup();
    model.conv_reads = -1;
    ads1015_t dev;
    ads1015_init(&dev, &model_i2c, &default_cfg);

    int16_t raw = 0;
    ASSERT_EQ(ads1015_read_raw(&dev, ADS1015_MUX_AIN0_GND, &raw),
              ADS1015_ERR_BUSY);
}

TEST_MAIN("ads1015 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_invalid_pga);
    RUN(test_read_raw_positive);
    RUN(test_read_raw_negative);
    RUN(test_read_raw_max);
    RUN(test_read_raw_min);
    RUN(test_read_voltage_positive);
    RUN(test_read_triggers_conversion);
    RUN(test_set_pga);
    RUN(test_set_rate);
    RUN(test_conversion_register_readonly);
    RUN(test_multiple_reads_consistent);
    RUN(test_different_adc_values);
    RUN(test_reset_values);
    RUN(test_threshold_reserved_bits_are_read_only);
    RUN(test_writing_os_0_has_no_effect);
    RUN(test_os_reads_0_during_a_conversion);
    RUN(test_result_appears_when_the_conversion_ends);
    RUN(test_os_write_during_a_conversion_has_no_effect);
    RUN(test_driver_waits_for_the_conversion);
    RUN(test_driver_gives_up_on_a_stuck_conversion);
TEST_MAIN_END()
