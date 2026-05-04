/*
 * test_hw_model_ads1115.c — tests using hardware register model
 *
 * Validates ADS1115 driver against a register model.
 * The model enforces conversion register read-only, config register
 * OS bit triggering conversion, and 16-bit register layout.
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
