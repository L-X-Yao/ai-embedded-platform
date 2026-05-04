// SPDX-License-Identifier: MIT
/*
 * prop_ads1015.c — property-based tests for ADS1015 12-bit ADC
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ads1015.h"

static const ads1015_i2c_t prop_i2c = {
    .write_reg  = mock_i2c_write_reg,
    .read_regs  = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ads1015_t dev;
    ads1015_cfg_t cfg = {
        .addr = ADS1015_ADDR_GND,
        .pga  = (ads1015_pga_t)(ARBI_UINT8(seed) % 6),
        .rate = (ads1015_rate_t)(ARBI_UINT8(seed) % 7),
    };
    ads1015_init(&dev, &prop_i2c, &cfg);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Set OS bit (bit 15 of config reg at offset 1) to indicate conversion ready */
    mock_regs[0x01] = 0x80;

    ads1015_t dev;
    ads1015_cfg_t cfg = {
        .addr = ADS1015_ADDR_GND,
        .pga  = ADS1015_PGA_2048,
        .rate = ADS1015_RATE_1600,
    };
    ads1015_init(&dev, &prop_i2c, &cfg);

    ads1015_mux_t ch = (ads1015_mux_t)(ARBI_UINT8(seed) % 8);
    float mv;
    ads1015_read_single(&dev, ch, &mv);

    int16_t raw;
    ads1015_read_raw(&dev, ch, &raw);
}

TEST_MAIN("prop_ads1015")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
