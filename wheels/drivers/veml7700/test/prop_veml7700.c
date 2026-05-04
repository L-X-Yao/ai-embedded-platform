// SPDX-License-Identifier: MIT
/*
 * prop_veml7700.c — property-based tests for VEML7700 ambient light sensor
 *
 * VEML7700 uses write_regs/read_regs I2C (16-bit register values).
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "veml7700.h"

static const veml7700_i2c_t prop_i2c = {
    .write_regs = mock_i2c_write_regs,
    .read_regs  = mock_i2c_read_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    veml7700_t dev;
    veml7700_init(&dev, &prop_i2c,
                  (veml7700_gain_t)(ARBI_UINT8(seed) % 4),
                  VEML7700_IT_100MS);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    veml7700_t dev;
    veml7700_init(&dev, &prop_i2c, VEML7700_GAIN_1, VEML7700_IT_100MS);

    uint16_t als;
    veml7700_read_als(&dev, &als);

    float lux;
    veml7700_err_t err = veml7700_read_lux(&dev, &lux);
    if (err != VEML7700_OK) return;

    PROP_ASSERT(lux == lux);
    PROP_ASSERT(lux >= 0.0f);
}

TEST_MAIN("prop_veml7700")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
