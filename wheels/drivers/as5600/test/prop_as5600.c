// SPDX-License-Identifier: MIT
/*
 * prop_as5600.c — property-based tests for AS5600 magnetic position sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "as5600.h"

static const as5600_i2c_t prop_i2c = {
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

    as5600_t dev;
    as5600_init(&dev, &prop_i2c);

    uint16_t raw;
    as5600_read_raw_angle(&dev, &raw);
    float deg;
    as5600_read_degrees(&dev, &deg);
}

PROPERTY(degrees_bounded, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    as5600_t dev;
    as5600_err_t err = as5600_init(&dev, &prop_i2c);
    if (err != AS5600_OK) return;

    float deg;
    err = as5600_read_degrees(&dev, &deg);
    if (err != AS5600_OK) return;

    PROP_ASSERT(deg == deg); /* not NaN */
    PROP_ASSERT(deg >= 0.0f && deg <= 360.0f);
}

TEST_MAIN("prop_as5600")
    RUN(read_no_crash);
    RUN(degrees_bounded);
TEST_MAIN_END()
