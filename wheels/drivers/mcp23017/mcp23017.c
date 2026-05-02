// SPDX-License-Identifier: MIT
/*
 * mcp23017 — I2C 16-bit GPIO expander driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Bank=0 register addressing (default after POR).
 * See Microchip MCP23017 datasheet DS20001952C.
 */

#include "mcp23017.h"

/* ---- Bank=0 register addresses ---- */
#define REG_IODIRA      0x00
#define REG_IODIRB      0x01
#define REG_IPOLA       0x02
#define REG_IPOLB       0x03
#define REG_GPINTENA    0x04
#define REG_GPINTENB    0x05
#define REG_DEFVALA     0x06
#define REG_DEFVALB     0x07
#define REG_INTCONA     0x08
#define REG_INTCONB     0x09
#define REG_IOCON       0x0A
#define REG_GPPUA       0x0C
#define REG_GPPUB       0x0D
#define REG_INTFA       0x0E
#define REG_INTFB       0x0F
#define REG_INTCAPA     0x10
#define REG_INTCAPB     0x11
#define REG_GPIOA       0x12
#define REG_GPIOB       0x13
#define REG_OLATA       0x14
#define REG_OLATB       0x15

/* Number of register pairs to reset during init (IODIRA..OLATB = 22 regs) */
#define REG_RESET_COUNT 22

static bool write_reg(const mcp23017_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->dev_addr, reg, val);
}

static bool read_reg(const mcp23017_t *dev, uint8_t reg, uint8_t *val)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->dev_addr, reg, val, 1);
}

static bool valid_port(mcp23017_port_t port)
{
    return port == MCP23017_PORT_A || port == MCP23017_PORT_B;
}

mcp23017_err_t mcp23017_init(mcp23017_t *dev, const mcp23017_i2c_t *i2c,
                             uint8_t dev_addr)
{
    if (!dev || !i2c || !i2c->write_reg || !i2c->read_regs)
        return MCP23017_ERR_PARAM;

    if (dev_addr < 0x20 || dev_addr > 0x27)
        return MCP23017_ERR_PARAM;

    dev->i2c      = i2c;
    dev->dev_addr = dev_addr;

    /* Reset all registers to POR defaults:
     * IODIR=0xFF (all input), all others=0x00 */
    for (uint8_t reg = 0; reg < REG_RESET_COUNT; reg++)
    {
        uint8_t val = (reg == REG_IODIRA || reg == REG_IODIRB) ? 0xFF : 0x00;
        if (!write_reg(dev, reg, val))
            return MCP23017_ERR_I2C;
    }

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_set_direction(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_IODIRA : REG_IODIRB;
    if (!write_reg(dev, reg, mask))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_set_pullup(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t mask)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_GPPUA : REG_GPPUB;
    if (!write_reg(dev, reg, mask))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_write_port(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t value)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_GPIOA : REG_GPIOB;
    if (!write_reg(dev, reg, value))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_read_port(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t *value)
{
    if (!dev || !dev->i2c || !value || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_GPIOA : REG_GPIOB;
    if (!read_reg(dev, reg, value))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_write_pin(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t pin, bool level)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return MCP23017_ERR_PARAM;
    if (pin > 7)
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_OLATA : REG_OLATB;
    uint8_t cur;
    if (!read_reg(dev, reg, &cur))
        return MCP23017_ERR_I2C;

    if (level)
        cur |= (uint8_t)(1u << pin);
    else
        cur &= (uint8_t)~(1u << pin);

    if (!write_reg(dev, reg, cur))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_read_pin(mcp23017_t *dev, mcp23017_port_t port,
                                 uint8_t pin, bool *level)
{
    if (!dev || !dev->i2c || !level || !valid_port(port))
        return MCP23017_ERR_PARAM;
    if (pin > 7)
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_GPIOA : REG_GPIOB;
    uint8_t val;
    if (!read_reg(dev, reg, &val))
        return MCP23017_ERR_I2C;

    *level = (val & (1u << pin)) != 0;
    return MCP23017_OK;
}

mcp23017_err_t mcp23017_set_interrupt(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_GPINTENA : REG_GPINTENB;
    if (!write_reg(dev, reg, mask))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}

mcp23017_err_t mcp23017_read_interrupt_flag(mcp23017_t *dev,
                                            mcp23017_port_t port,
                                            uint8_t *flags)
{
    if (!dev || !dev->i2c || !flags || !valid_port(port))
        return MCP23017_ERR_PARAM;

    uint8_t reg = (port == MCP23017_PORT_A) ? REG_INTFA : REG_INTFB;
    if (!read_reg(dev, reg, flags))
        return MCP23017_ERR_I2C;

    return MCP23017_OK;
}
