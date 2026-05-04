// SPDX-License-Identifier: MIT
/*
 * prop_tcs34725.c — property-based tests for TCS34725 color sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "tcs34725.h"

static const tcs34725_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x12] = 0x44; /* chip ID */

    tcs34725_t dev;
    tcs34725_init(&dev, &prop_i2c,
                  TCS34725_INTEG_24MS, TCS34725_GAIN_1X);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x12] = 0x44;

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &prop_i2c,
        (tcs34725_integ_t)(ARBI_UINT8(seed)),
        (tcs34725_gain_t)(ARBI_UINT8(seed) % 4));
    if (err != TCS34725_OK) return;

    tcs34725_rgbc_t rgbc;
    tcs34725_read_rgbc(&dev, &rgbc);

    float lux;
    tcs34725_get_lux(&rgbc, &lux);
    uint16_t cct;
    tcs34725_get_color_temp(&rgbc, &cct);
}

TEST_MAIN("prop_tcs34725")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
