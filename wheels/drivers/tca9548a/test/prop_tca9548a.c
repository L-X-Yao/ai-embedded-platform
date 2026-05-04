// SPDX-License-Identifier: MIT
/*
 * prop_tca9548a.c — property-based tests for TCA9548A I2C mux
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "tca9548a.h"

static const tca9548a_i2c_t prop_i2c = {
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

    tca9548a_t dev;
    uint8_t addr = (uint8_t)(TCA9548A_ADDR_BASE + (ARBI_UINT8(seed) % 8));
    tca9548a_init(&dev, &prop_i2c, addr);
}

PROPERTY(select_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    tca9548a_t dev;
    tca9548a_init(&dev, &prop_i2c, TCA9548A_ADDR_BASE);

    uint8_t ch = ARBI_UINT8(seed) % 8;
    tca9548a_select(&dev, ch);

    uint8_t mask = ARBI_UINT8(seed);
    tca9548a_select_multi(&dev, mask);

    uint8_t readback;
    tca9548a_get_channel(&dev, &readback);
    tca9548a_disable_all(&dev);
}

TEST_MAIN("prop_tca9548a")
    RUN(init_no_crash);
    RUN(select_no_crash);
TEST_MAIN_END()
