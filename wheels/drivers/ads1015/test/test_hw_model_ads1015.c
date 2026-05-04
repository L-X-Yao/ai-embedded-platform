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
TEST_MAIN_END()
