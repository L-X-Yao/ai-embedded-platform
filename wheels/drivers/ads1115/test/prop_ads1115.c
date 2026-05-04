// SPDX-License-Identifier: MIT
/*
 * prop_ads1115.c — property-based tests for ADS1115 ADC driver
 *
 * ADS1115 uses write_regs (not write_reg) for the 16-bit config register.
 * We use mock_i2c.h for read_regs and provide a custom write_regs.
 *
 * Properties:
 *   1. init+read never crashes with arbitrary register values
 *   2. raw value always in int16_t range [-32768, 32767]
 *   3. voltage output bounded by PGA setting
 *   4. channel switch never crashes
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ads1115.h"

/* ADS1115 needs write_regs which mock_i2c.h provides as mock_i2c_write_regs */
static const ads1115_i2c_t prop_i2c = {
    .write_reg  = mock_i2c_write_reg,
    .read_regs  = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

/* ADS1115 register addresses */
#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Set conversion-ready bit (bit 15 of config = byte[REG_CONFIG] bit 7) */
    mock_regs[REG_CONFIG] = (uint8_t)(mock_regs[REG_CONFIG] | 0x80);

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128,
    };
    ads1115_init(&dev, &prop_i2c, &cfg);

    int16_t raw;
    ads1115_read_raw(&dev, &raw);
    /* Must not crash */
}

PROPERTY(raw_in_int16_range, 5000)
{
    mock_i2c_reset();
    /* Plant random conversion result */
    mock_regs[REG_CONVERSION]     = ARBI_UINT8(seed);
    mock_regs[REG_CONVERSION + 1] = ARBI_UINT8(seed);
    /* Conversion ready */
    mock_regs[REG_CONFIG] = 0x80;

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128,
    };
    ads1115_err_t err = ads1115_init(&dev, &prop_i2c, &cfg);
    if (err != ADS1115_OK) return;

    int16_t raw = 0;
    err = ads1115_read_raw(&dev, &raw);
    if (err != ADS1115_OK) return;

    /* Verify read succeeded and raw was actually written (not left at init) */
    uint8_t expected_hi = mock_regs[REG_CONVERSION];
    uint8_t expected_lo = mock_regs[REG_CONVERSION + 1];
    int16_t expected = (int16_t)((uint16_t)expected_hi << 8 | expected_lo);
    PROP_ASSERT_EQ(raw, expected);
}

PROPERTY(voltage_bounded_by_pga, 5000)
{
    /* PGA full-scale ranges in mV */
    static const float pga_max_mv[] = {
        6144.0f, 4096.0f, 2048.0f, 1024.0f, 512.0f, 256.0f
    };

    mock_i2c_reset();
    mock_regs[REG_CONVERSION]     = ARBI_UINT8(seed);
    mock_regs[REG_CONVERSION + 1] = ARBI_UINT8(seed);
    mock_regs[REG_CONFIG] = 0x80;

    int pga_idx = (int)(ARBI_UINT8(seed) % 6);
    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = (ads1115_pga_t)pga_idx,
        .rate = ADS1115_RATE_128,
    };
    ads1115_err_t err = ads1115_init(&dev, &prop_i2c, &cfg);
    if (err != ADS1115_OK) return;

    float voltage_mv = 0.0f;
    err = ads1115_read_voltage(&dev, &voltage_mv);
    if (err != ADS1115_OK) return;

    PROP_ASSERT(voltage_mv == voltage_mv); /* not NaN */
    float limit = pga_max_mv[pga_idx] + 1.0f;
    PROP_ASSERT(voltage_mv >= -limit && voltage_mv <= limit);
}

PROPERTY(channel_switch_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    mock_regs[REG_CONFIG] = 0x80;

    ads1115_t dev;
    ads1115_cfg_t cfg = {
        .addr = ADS1115_ADDR_GND,
        .mux  = ADS1115_MUX_AIN0_GND,
        .pga  = ADS1115_PGA_2048,
        .rate = ADS1115_RATE_128,
    };
    ads1115_err_t err = ads1115_init(&dev, &prop_i2c, &cfg);
    if (err != ADS1115_OK) return;

    /* Switch to random channel and read */
    ads1115_mux_t mux = (ads1115_mux_t)(ARBI_UINT8(seed) % 8);
    ads1115_set_channel(&dev, mux);

    mock_regs[REG_CONVERSION]     = ARBI_UINT8(seed);
    mock_regs[REG_CONVERSION + 1] = ARBI_UINT8(seed);

    int16_t raw;
    ads1115_read_raw(&dev, &raw);
    /* Must not crash */
}

TEST_MAIN("prop_ads1115")
    RUN(read_no_crash);
    RUN(raw_in_int16_range);
    RUN(voltage_bounded_by_pga);
    RUN(channel_switch_no_crash);
TEST_MAIN_END()
