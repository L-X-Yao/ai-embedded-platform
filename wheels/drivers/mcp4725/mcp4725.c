// SPDX-License-Identifier: MIT
/*
 * mcp4725 — MCP4725 12-bit I2C DAC driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected, raw write/read)
 * Instances: multi
 *
 * Protocol reference: Microchip DS22039D Section 6.
 */

#include "mcp4725.h"
#include "contract.h"

/* Command byte bit positions (datasheet Figure 6-2):
 * Byte 0: [C2(7)][C1(6)][C0(5)][x(4)][x(3)][PD1(2)][PD0(1)][x(0)]
 */
#define CMD_WRITE_DAC    0x40u   /* C2:C1:C0 = 010 */
#define CMD_WRITE_EEPROM 0x60u   /* C2:C1:C0 = 011 */

mcp4725_err_t mcp4725_init(mcp4725_t *dev, const mcp4725_i2c_t *i2c,
                           uint8_t addr)
{
    REQUIRE(dev != NULL);
    REQUIRE(i2c != NULL);
    if (!dev || !i2c) return MCP4725_ERR_PARAM;
    dev->i2c = i2c;
    dev->addr = addr;

    uint8_t buf[5];
    if (!i2c->read(i2c->ctx, addr, buf, 5))
        return MCP4725_ERR_I2C;

    return MCP4725_OK;
}

mcp4725_err_t mcp4725_write_fast(mcp4725_t *dev, uint16_t value)
{
    REQUIRE(dev != NULL);
    REQUIRE(value <= MCP4725_MAX_VALUE);
    if (!dev) return MCP4725_ERR_PARAM;
    if (value > MCP4725_MAX_VALUE) return MCP4725_ERR_PARAM;

    /* Fast mode: [00][PD=00][D11:D8] [D7:D0] */
    uint8_t buf[2];
    buf[0] = (uint8_t)((value >> 8) & 0x0Fu);
    buf[1] = (uint8_t)(value & 0xFFu);

    if (!dev->i2c->write(dev->i2c->ctx, dev->addr, buf, 2))
        return MCP4725_ERR_I2C;

    return MCP4725_OK;
}

mcp4725_err_t mcp4725_write_dac(mcp4725_t *dev, uint16_t value,
                                mcp4725_pd_t pd)
{
    REQUIRE(dev != NULL);
    REQUIRE(value <= MCP4725_MAX_VALUE);
    if (!dev) return MCP4725_ERR_PARAM;
    if (value > MCP4725_MAX_VALUE) return MCP4725_ERR_PARAM;
    if ((unsigned)pd > 3u) return MCP4725_ERR_PARAM;

    /* [010xx][PD1:PD0][x] [D11:D4] [D3:D0][xxxx] */
    uint8_t buf[3];
    buf[0] = (uint8_t)(CMD_WRITE_DAC | (((uint8_t)pd & 0x03u) << 1));
    buf[1] = (uint8_t)(value >> 4);
    buf[2] = (uint8_t)((value & 0x0Fu) << 4);

    if (!dev->i2c->write(dev->i2c->ctx, dev->addr, buf, 3))
        return MCP4725_ERR_I2C;

    return MCP4725_OK;
}

mcp4725_err_t mcp4725_write_eeprom(mcp4725_t *dev, uint16_t value,
                                   mcp4725_pd_t pd)
{
    REQUIRE(dev != NULL);
    REQUIRE(value <= MCP4725_MAX_VALUE);
    if (!dev) return MCP4725_ERR_PARAM;
    if (value > MCP4725_MAX_VALUE) return MCP4725_ERR_PARAM;
    if ((unsigned)pd > 3u) return MCP4725_ERR_PARAM;

    /* [011xx][PD1:PD0][x] [D11:D4] [D3:D0][xxxx] */
    uint8_t buf[3];
    buf[0] = (uint8_t)(CMD_WRITE_EEPROM | (((uint8_t)pd & 0x03u) << 1));
    buf[1] = (uint8_t)(value >> 4);
    buf[2] = (uint8_t)((value & 0x0Fu) << 4);

    if (!dev->i2c->write(dev->i2c->ctx, dev->addr, buf, 3))
        return MCP4725_ERR_I2C;

    return MCP4725_OK;
}

mcp4725_err_t mcp4725_read_status(mcp4725_t *dev, mcp4725_status_t *out)
{
    REQUIRE(dev != NULL);
    REQUIRE(out != NULL);
    if (!dev || !out) return MCP4725_ERR_PARAM;

    /* 5-byte read:
     * [0]: [RDY/BSY(7)][POR(6)][C1(5):C0(4)][PD1(3):PD0(2)][D11(1):D10(0)]
     * [1]: [D9:D2]
     * [2]: [D1:D0][xxxxxx]
     * [3]: [x(7)][PD1e(6):PD0e(5)][x(4)][D11e(3):D8e(0)]
     * [4]: [D7e:D0e]
     */
    uint8_t buf[5];
    if (!dev->i2c->read(dev->i2c->ctx, dev->addr, buf, 5))
        return MCP4725_ERR_I2C;

    out->eeprom_busy = !(buf[0] & 0x80u);
    out->pd_mode     = (buf[0] >> 2) & 0x03u;
    out->dac_value   = (uint16_t)(((uint16_t)(buf[0] & 0x03u) << 10) |
                                  ((uint16_t)buf[1] << 2) |
                                  ((uint16_t)buf[2] >> 6));
    out->eeprom_pd   = (buf[3] >> 5) & 0x03u;
    out->eeprom_value = (uint16_t)(((uint16_t)(buf[3] & 0x0Fu) << 8) |
                                   (uint16_t)buf[4]);

    return MCP4725_OK;
}
