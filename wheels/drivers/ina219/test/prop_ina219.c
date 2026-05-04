// SPDX-License-Identifier: MIT
/*
 * prop_ina219.c — property-based tests for INA219 current/voltage monitor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ina219.h"

static const ina219_i2c_t prop_i2c = {
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

    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                (ina219_bus_range_t)(ARBI_UINT8(seed) & 1),
                (ina219_gain_t)(ARBI_UINT8(seed) % 4));
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ina219_t dev;
    ina219_init(&dev, &prop_i2c, 0x40, 0.1f,
                INA219_RANGE_32V, INA219_GAIN_320MV);

    float voltage, shunt, current, power;
    ina219_read_bus_voltage(&dev, &voltage);
    ina219_read_shunt_voltage(&dev, &shunt);
    ina219_read_current(&dev, &current);
    ina219_read_power(&dev, &power);
}

TEST_MAIN("prop_ina219")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
