// SPDX-License-Identifier: MIT
/*
 * prop_bme680.c — property-based tests for BME680 environmental sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "bme680.h"

static const bme680_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xD0] = BME680_CHIP_ID;
    /* Clear measuring bit so read does not loop */
    mock_regs[0x1D] = 0x00;

    bme680_t dev;
    bme680_init(&dev, &prop_i2c, BME680_ADDR_LOW);

    bme680_data_t data;
    bme680_read(&dev, &data);
}

PROPERTY(output_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xD0] = BME680_CHIP_ID;
    mock_regs[0x1D] = 0x00;

    bme680_t dev;
    bme680_err_t err = bme680_init(&dev, &prop_i2c, BME680_ADDR_LOW);
    if (err != BME680_OK) return;

    bme680_data_t data;
    err = bme680_read(&dev, &data);
    if (err != BME680_OK) return;

    PROP_ASSERT(data.temperature_c == data.temperature_c);
    PROP_ASSERT(data.humidity_pct == data.humidity_pct);
    PROP_ASSERT(data.pressure_pa == data.pressure_pa);
}

TEST_MAIN("prop_bme680")
    RUN(read_no_crash);
    RUN(output_finite);
TEST_MAIN_END()
