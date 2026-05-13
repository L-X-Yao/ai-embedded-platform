// SPDX-License-Identifier: MIT
/*
 * mcp4728 — Microchip MCP4728 4-channel 12-bit DAC
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Protocol reference: Microchip DS20002278 Section 5.
 * Fast write command: 0x40 | (channel << 1), then [upper_nibble | D11:D8], [D7:D0]
 */

#include "mcp4728.h"
#include "contract.h"

/* Fast write command byte: 010 0 CC 0  where CC = channel (0-3) */
#define CMD_FAST_WRITE  0x40u

mcp4728_err_t mcp4728_init(mcp4728_t *dev, const mcp4728_i2c_t *i2c,
                           const mcp4728_cfg_t *cfg)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    REQUIRE(cfg != NULL);
    if (!dev || !i2c || !cfg) return MCP4728_ERR_PARAM;
    if (!i2c->write_regs) return MCP4728_ERR_PARAM;

    dev->i2c  = i2c;
    dev->addr = cfg->addr;

    return MCP4728_OK;
}

mcp4728_err_t mcp4728_write_channel(mcp4728_t *dev, uint8_t channel,
                                    uint16_t value)
{
    REQUIRE(dev != NULL);
    REQUIRE(channel < MCP4728_NUM_CHANNELS);
    REQUIRE(value <= MCP4728_MAX_VALUE);
    if (!dev || !dev->i2c) return MCP4728_ERR_PARAM;
    if (channel >= MCP4728_NUM_CHANNELS) return MCP4728_ERR_PARAM;
    if (value > MCP4728_MAX_VALUE) return MCP4728_ERR_PARAM;

    uint8_t cmd = (uint8_t)(CMD_FAST_WRITE | ((uint8_t)(channel << 1u)));
    uint8_t buf[2];
    buf[0] = (uint8_t)((value >> 8) & 0x0Fu);
    buf[1] = (uint8_t)(value & 0xFFu);

    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->addr, cmd, buf, 2))
        return MCP4728_ERR_I2C;

    return MCP4728_OK;
}

mcp4728_err_t mcp4728_write_all(mcp4728_t *dev, const uint16_t values[4])
{
    REQUIRE(dev != NULL);
    REQUIRE(values != NULL);
    if (!dev || !dev->i2c || !values) return MCP4728_ERR_PARAM;

    for (uint8_t ch = 0; ch < MCP4728_NUM_CHANNELS; ch++) {
        if (values[ch] > MCP4728_MAX_VALUE) return MCP4728_ERR_PARAM;
    }

    for (uint8_t ch = 0; ch < MCP4728_NUM_CHANNELS; ch++) {
        uint8_t cmd = (uint8_t)(CMD_FAST_WRITE | ((uint8_t)(ch << 1u)));
        uint8_t buf[2];
        buf[0] = (uint8_t)((values[ch] >> 8) & 0x0Fu);
        buf[1] = (uint8_t)(values[ch] & 0xFFu);

        if (!dev->i2c->write_regs(dev->i2c->ctx, dev->addr, cmd, buf, 2))
            return MCP4728_ERR_I2C;
    }

    return MCP4728_OK;
}
