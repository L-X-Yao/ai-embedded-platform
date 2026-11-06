// SPDX-License-Identifier: MIT
/*
 * prop_ads1115.c — property-based tests for ADS1115 ADC driver
 *
 * Runs on hw_model_ads1115.h, which keeps the 16-bit registers apart. The
 * byte-wide mock_i2c.h could not: pointer 1 is the Config register, and
 * byte 1 is also the low byte of the Conversion register.
 *
 * Properties:
 *   1. init+read succeeds with arbitrary register contents
 *   2. raw is the 16-bit two's complement result of the conversion
 *   3. voltage is raw times the LSB of p.15 and stays in the FSR
 *   4. a channel switch reaches MUX[2:0] of the Config register
 */

#include "test_harness.h"
#include "property_test.h"
#include "hw_model_ads1115.h"
#include "ads1115.h"
#include <math.h>

static ads1115_model_t model;

static const ads1115_i2c_t prop_i2c = {
    .read_regs  = ads1115_model_read_regs,
    .write_regs = ads1115_model_write_regs,
    .delay_ms   = ads1115_model_delay_ms,
    .ctx        = &model,
};

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    ads1115_model_init(&model);
    for (uint8_t reg = 0; reg < 4; reg++) {
        model.hw.regs[ADS_SLOT(reg)]     = ARBI_UINT8(seed);
        model.hw.regs[ADS_SLOT(reg) + 1] = ARBI_UINT8(seed);
    }
    int16_t planted = (int16_t)ARBI_UINT16(seed);
    ads1115_model_set_adc(&model, planted);

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128,
    };
    PROP_ASSERT_EQ(ads1115_init(&dev, &prop_i2c, &cfg), ADS1115_OK);

    int16_t raw = 0;
    PROP_ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    PROP_ASSERT_EQ(raw, planted);
}

PROPERTY(raw_in_int16_range, 5000)
{
    ads1115_model_init(&model);
    int16_t planted = (int16_t)ARBI_UINT16(seed);
    ads1115_model_set_adc(&model, planted);
    model.slow_percent = ARBI_UINT8(seed) % 11u;

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = (ads1115_rate_t)(ARBI_UINT8(seed) % 8u),
    };
    PROP_ASSERT_EQ(ads1115_init(&dev, &prop_i2c, &cfg), ADS1115_OK);

    int16_t raw = 0;
    PROP_ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);
    PROP_ASSERT_EQ(raw, planted);
}

PROPERTY(voltage_bounded_by_pga, 5000)
{
    /* p.15 Table 7-1: FSR in mV and LSB in uV, one row per PGA code */
    static const float fsr_mv[] = {
        6144.0f, 4096.0f, 2048.0f, 1024.0f, 512.0f, 256.0f
    };
    static const float lsb_uv[] = {
        187.5f, 125.0f, 62.5f, 31.25f, 15.625f, 7.8125f
    };

    ads1115_model_init(&model);
    int16_t planted = (int16_t)ARBI_UINT16(seed);
    ads1115_model_set_adc(&model, planted);

    int pga_idx = (int)(ARBI_UINT8(seed) % 6);
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = (ads1115_pga_t)pga_idx,
        .rate = ADS1115_RATE_128,
    };
    PROP_ASSERT_EQ(ads1115_init(&dev, &prop_i2c, &cfg), ADS1115_OK);

    float voltage_mv = 0.0f;
    PROP_ASSERT_EQ(ads1115_read_voltage(&dev, &voltage_mv), ADS1115_OK);

    PROP_ASSERT(voltage_mv == voltage_mv); /* not NaN */
    PROP_ASSERT(fabsf(voltage_mv - (float)planted * lsb_uv[pga_idx] / 1000.0f) < 0.01f);
    PROP_ASSERT(voltage_mv >= -fsr_mv[pga_idx] && voltage_mv < fsr_mv[pga_idx]);
}

PROPERTY(channel_switch_no_crash, 5000)
{
    ads1115_model_init(&model);

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128,
    };
    PROP_ASSERT_EQ(ads1115_init(&dev, &prop_i2c, &cfg), ADS1115_OK);

    /* Switch to random channel and read */
    uint8_t mux = (uint8_t)(ARBI_UINT8(seed) % 8u);
    PROP_ASSERT_EQ(ads1115_set_channel(&dev, (ads1115_mux_t)mux), ADS1115_OK);

    int16_t raw = 0;
    PROP_ASSERT_EQ(ads1115_read_raw(&dev, &raw), ADS1115_OK);

    /* p.25 MUX[2:0] is bits 14:12 of the Config register */
    PROP_ASSERT_EQ((model.hw.regs[ADS_SLOT(ADS1115_REG_CONFIG)] >> 4) & 7, mux);
}

TEST_MAIN("prop_ads1115")
    RUN(read_no_crash);
    RUN(raw_in_int16_range);
    RUN(voltage_bounded_by_pga);
    RUN(channel_switch_no_crash);
TEST_MAIN_END()
