// SPDX-License-Identifier: MIT
/*
 * prop_mcp23017.c — property-based tests for MCP23017 GPIO expander
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "mcp23017.h"

static const mcp23017_i2c_t prop_i2c = {
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

    mcp23017_t dev;
    uint8_t addr = (uint8_t)(0x20 + (ARBI_UINT8(seed) % 8));
    mcp23017_init(&dev, &prop_i2c, addr);
}

PROPERTY(read_write_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    mcp23017_t dev;
    mcp23017_init(&dev, &prop_i2c, 0x20);

    mcp23017_port_t port = (mcp23017_port_t)(ARBI_UINT8(seed) & 1);
    mcp23017_set_direction(&dev, port, ARBI_UINT8(seed));
    mcp23017_write_port(&dev, port, ARBI_UINT8(seed));

    uint8_t val;
    mcp23017_read_port(&dev, port, &val);

    bool level;
    uint8_t pin = ARBI_UINT8(seed) % 8;
    mcp23017_read_pin(&dev, port, pin, &level);
}

TEST_MAIN("prop_mcp23017")
    RUN(init_no_crash);
    RUN(read_write_no_crash);
TEST_MAIN_END()
