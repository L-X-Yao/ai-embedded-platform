// SPDX-License-Identifier: MIT
/*
 * at24cxx --- I2C EEPROM driver for AT24CXX_C01 through AT24CXX_C256
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "at24cxx.h"
#include "contract.h"

/* ---- helpers ---- */

/** Return true if model uses 16-bit memory addressing. */
static bool needs_addr16(at24cxx_model_t model)
{
    return (int)model >= (int)AT24CXX_C32;
}

/** Page size lookup by model. */
static uint8_t page_size_for(at24cxx_model_t model)
{
    switch (model) {
    case AT24CXX_C01:  return 8;
    case AT24CXX_C02:  return 8;
    case AT24CXX_C04:  return 16;
    case AT24CXX_C08:  return 16;
    case AT24CXX_C16:  return 16;
    case AT24CXX_C32:  return 32;
    case AT24CXX_C64:  return 32;
    case AT24CXX_C128: return 64;
    case AT24CXX_C256: return 64;
    default:       return 8;
    }
}

/**
 * For AT24CXX_C04/08/16 the upper address bits are encoded in the I2C
 * device address (using the A0-A2 pin positions).  Return the
 * effective device address for a given memory address.
 */
static uint8_t effective_dev_addr(const at24cxx_t *dev, uint16_t mem_addr)
{
    if (needs_addr16(dev->model)) {
        return dev->dev_addr;
    }
    /* AT24CXX_C04: bit 8 of address goes into dev_addr bit 0
     * AT24CXX_C08: bits 8-9 go into dev_addr bits 0-1
     * AT24CXX_C16: bits 8-10 go into dev_addr bits 0-2 */
    uint8_t page_bits = 0;
    if ((int)dev->model > 256)  page_bits = (uint8_t)(mem_addr >> 8);
    return (uint8_t)(dev->dev_addr | (page_bits & 0x07u));
}

/** Write one page (or partial page) of data. */
static at24cxx_err_t write_page(const at24cxx_t *dev, uint16_t addr,
                                const uint8_t *data, uint16_t len)
{
    const at24cxx_i2c_t *i2c = dev->i2c;
    uint8_t daddr = effective_dev_addr(dev, addr);

    if (needs_addr16(dev->model)) {
        if (!i2c->write_regs) return AT24CXX_ERR_I2C;
        if (!i2c->write_regs(i2c->ctx, daddr, addr, data, len))
            return AT24CXX_ERR_I2C;
    } else {
        /* 8-bit addressing: batch via write_regs8 if available,
         * otherwise fall back to byte-by-byte via write_reg */
        uint8_t reg = (uint8_t)(addr & 0xFFu);
        if (i2c->write_regs8) {
            if (!i2c->write_regs8(i2c->ctx, daddr, reg, data, len))
                return AT24CXX_ERR_I2C;
        } else {
            for (uint16_t i = 0; i < len; i++) {
                /* Every byte is a write of its own, with its own write cycle */
                if (i > 0u)
                    i2c->delay_ms(i2c->ctx, AT24CXX_WRITE_CYCLE_MS);
                if (!i2c->write_reg(i2c->ctx, daddr, (uint8_t)(reg + i),
                                    data[i]))
                    return AT24CXX_ERR_I2C;
            }
        }
    }

    /* Write-cycle delay */
    i2c->delay_ms(i2c->ctx, AT24CXX_WRITE_CYCLE_MS);
    return AT24CXX_OK;
}

/* ---- public API ---- */

at24cxx_err_t at24cxx_init(at24cxx_t *dev, const at24cxx_i2c_t *i2c,
                           uint8_t dev_addr, at24cxx_model_t model)
{
    if (!dev || !i2c) return AT24CXX_ERR_PARAM;

    /* The chip answers nothing until a write cycle ends, and nothing else waits */
    if (!i2c->delay_ms) return AT24CXX_ERR_PARAM;

    /* Validate model */
    switch (model) {
    case AT24CXX_C01: case AT24CXX_C02: case AT24CXX_C04: case AT24CXX_C08:
    case AT24CXX_C16: case AT24CXX_C32: case AT24CXX_C64: case AT24CXX_C128:
    case AT24CXX_C256:
        break;
    default:
        return AT24CXX_ERR_PARAM;
    }

    /* 16-bit chips need write_regs and read_regs16 */
    if (needs_addr16(model)) {
        if (!i2c->write_regs || !i2c->read_regs16)
            return AT24CXX_ERR_PARAM;
    } else {
        if (!i2c->write_reg || !i2c->read_regs)
            return AT24CXX_ERR_PARAM;
    }

    dev->i2c       = i2c;
    dev->dev_addr  = dev_addr;
    dev->model     = model;
    dev->page_size = page_size_for(model);
    INVARIANT(dev->i2c != NULL);
    return AT24CXX_OK;
}

at24cxx_err_t at24cxx_read(const at24cxx_t *dev, uint16_t addr,
                           uint8_t *buf, uint16_t len)
{
    if (!dev || !dev->i2c || !buf) return AT24CXX_ERR_PARAM;
    if (len == 0) return AT24CXX_OK;
    if ((uint32_t)addr + len > (uint32_t)dev->model) return AT24CXX_ERR_ADDR;

    const at24cxx_i2c_t *i2c = dev->i2c;

    if (needs_addr16(dev->model)) {
        if (!i2c->read_regs16(i2c->ctx, dev->dev_addr, addr, buf, len))
            return AT24CXX_ERR_I2C;
    } else {
        uint8_t daddr = effective_dev_addr(dev, addr);
        uint8_t reg   = (uint8_t)(addr & 0xFFu);
        if (!i2c->read_regs(i2c->ctx, daddr, reg, buf, len))
            return AT24CXX_ERR_I2C;
    }
    return AT24CXX_OK;
}

at24cxx_err_t at24cxx_write(const at24cxx_t *dev, uint16_t addr,
                            const uint8_t *data, uint16_t len)
{
    if (!dev || !dev->i2c || !data) return AT24CXX_ERR_PARAM;
    if (len == 0) return AT24CXX_OK;
    if ((uint32_t)addr + len > (uint32_t)dev->model) return AT24CXX_ERR_ADDR;

    uint16_t offset = 0;
    while (offset < len) {
        /* Bytes remaining in current page */
        uint16_t page_off  = (uint16_t)((addr + offset) % dev->page_size);
        uint16_t page_room = (uint16_t)(dev->page_size - page_off);
        uint16_t chunk     = (uint16_t)(len - offset);
        if (chunk > page_room) chunk = page_room;

        at24cxx_err_t err = write_page(dev, (uint16_t)(addr + offset),
                                       &data[offset], chunk);
        if (err != AT24CXX_OK) return err;
        offset = (uint16_t)(offset + chunk);
    }
    return AT24CXX_OK;
}

at24cxx_err_t at24cxx_write_byte(const at24cxx_t *dev, uint16_t addr,
                                 uint8_t val)
{
    return at24cxx_write(dev, addr, &val, 1);
}
