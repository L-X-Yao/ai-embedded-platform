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
 * Multi-Write command: 0x40 | (channel << 1), then [VREF PD1 PD0 Gx D11:D8], [D7:D0]
 */

#include "mcp4728.h"
#include "contract.h"

/* Multi-Write command byte: 0 1 0 0 0 DAC1 DAC0 UDAC (DS22187E Figure 5-8).
 * UDAC = 0 updates VOUT after the 4th byte's ACK. */
#define CMD_MULTI_WRITE  0x40u

#define ALL_CHANNELS     0x0Fu   /* one bit per channel in vref_internal, gain_x2 */

/* 3rd byte of a Multi-Write: VREF PD1 PD0 Gx D11-D8 (Figure 5-8). VREF and Gx
 * come from the configuration. PD1 PD0 stay 00, normal mode. */
static uint8_t config_byte(const mcp4728_t *dev, uint8_t channel, uint16_t value)
{
    uint8_t vref = (uint8_t)((dev->vref_internal >> channel) & 1u);
    uint8_t gain = (uint8_t)((dev->gain_x2 >> channel) & 1u);

    return (uint8_t)((vref << 7u) | (gain << 4u) | ((value >> 8) & 0x0Fu));
}

mcp4728_err_t mcp4728_init(mcp4728_t *dev, const mcp4728_i2c_t *i2c,
                           const mcp4728_cfg_t *cfg)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    REQUIRE(cfg != NULL);
    if (!dev || !i2c || !cfg) return MCP4728_ERR_PARAM;
    if (!i2c->write_regs) return MCP4728_ERR_PARAM;
    if (cfg->vref_internal > ALL_CHANNELS || cfg->gain_x2 > ALL_CHANNELS)
        return MCP4728_ERR_PARAM;

    /* DS22187E section 4.3.1: with VREF = VDD "the Gain of 2 option is
     * disabled". The device would ignore Gx and give half the voltage asked for. */
    if ((cfg->gain_x2 & (uint8_t)~cfg->vref_internal) != 0u)
        return MCP4728_ERR_PARAM;

    dev->i2c  = i2c;
    dev->addr = cfg->addr;
    dev->vref_internal = cfg->vref_internal;
    dev->gain_x2       = cfg->gain_x2;

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

    uint8_t cmd = (uint8_t)(CMD_MULTI_WRITE | ((uint8_t)(channel << 1u)));
    uint8_t buf[2];
    buf[0] = config_byte(dev, channel, value);
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

    /* Bytes 2-4 repeat per channel (Figure 5-8). reg carries channel A's
     * command byte, so buf starts at its 3rd byte. */
    uint8_t  buf[(MCP4728_NUM_CHANNELS * 3u) - 1u];
    uint16_t len = 0;

    for (uint8_t ch = 0; ch < MCP4728_NUM_CHANNELS; ch++) {
        if (ch > 0) {
            buf[len++] = (uint8_t)(CMD_MULTI_WRITE | ((uint8_t)(ch << 1u)));
        }
        buf[len++] = config_byte(dev, ch, values[ch]);
        buf[len++] = (uint8_t)(values[ch] & 0xFFu);
    }

    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->addr, CMD_MULTI_WRITE,
                              buf, len))
        return MCP4728_ERR_I2C;

    return MCP4728_OK;
}
