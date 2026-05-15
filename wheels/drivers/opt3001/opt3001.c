// SPDX-License-Identifier: MIT
/*
 * opt3001 — TI OPT3001 I2C digital ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "opt3001.h"
#include "contract.h"

/* Register addresses */
#define REG_RESULT          0x00
#define REG_CONFIG          0x01
#define REG_MANUFACTURER_ID 0x7E
#define REG_DEVICE_ID       0x7F

/* Expected ID values */
#define MANUFACTURER_ID     0x5449
#define DEVICE_ID           0x3001

/* Config register bit fields */
#define CFG_MODE_SHIFT      9
#define CFG_MODE_MASK       (0x03u << CFG_MODE_SHIFT)
#define CFG_MODE_SHUTDOWN   (0x00u << CFG_MODE_SHIFT)
#define CFG_MODE_SINGLE     (0x01u << CFG_MODE_SHIFT)
#define CFG_MODE_CONTINUOUS (0x03u << CFG_MODE_SHIFT)
#define CFG_CRF_BIT         (1u << 7)   /* Conversion ready flag */

/* Default config: auto full-scale range, 100 ms conversion, shutdown mode */
#define CFG_DEFAULT         0xC010u

static opt3001_err_t read_reg16(const opt3001_t *dev, uint8_t reg,
                                uint16_t *val)
{
    uint8_t buf[2];
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, 2))
        return OPT3001_ERR_I2C;
    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return OPT3001_OK;
}

static opt3001_err_t write_reg16(const opt3001_t *dev, uint8_t reg,
                                 uint16_t val)
{
    uint8_t buf[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->addr, reg, buf, 2))
        return OPT3001_ERR_I2C;
    return OPT3001_OK;
}

opt3001_err_t opt3001_init(opt3001_t *dev, const opt3001_i2c_t *i2c,
                           uint8_t addr)
{
    if (!dev || !i2c)
        return OPT3001_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return OPT3001_ERR_PARAM;
    if (addr < 0x44 || addr > 0x47)
        return OPT3001_ERR_PARAM;

    dev->i2c = i2c;
    dev->addr = addr;

    /* Verify manufacturer ID */
    uint16_t id;
    opt3001_err_t err = read_reg16(dev, REG_MANUFACTURER_ID, &id);
    if (err != OPT3001_OK)
        return err;
    if (id != MANUFACTURER_ID)
        return OPT3001_ERR_ID;

    /* Verify device ID */
    err = read_reg16(dev, REG_DEVICE_ID, &id);
    if (err != OPT3001_OK)
        return err;
    if (id != DEVICE_ID)
        return OPT3001_ERR_ID;

    /* Write default config: auto range, 100 ms, shutdown mode */
    return write_reg16(dev, REG_CONFIG, CFG_DEFAULT);
}

opt3001_err_t opt3001_read_lux(opt3001_t *dev, float *lux)
{
    if (!dev || !lux)
        return OPT3001_ERR_PARAM;

    uint16_t raw;
    opt3001_err_t err = read_reg16(dev, REG_RESULT, &raw);
    if (err != OPT3001_OK)
        return err;

    uint8_t exponent = (uint8_t)((raw >> 12) & 0x0F);
    uint16_t mantissa = raw & 0x0FFF;

    /* lux = 0.01 * 2^E * R */
    *lux = 0.01f * (float)(1u << exponent) * (float)mantissa;
    ENSURE(*lux == *lux);  /* not NaN */
    return OPT3001_OK;
}

opt3001_err_t opt3001_start_single(opt3001_t *dev)
{
    if (!dev)
        return OPT3001_ERR_PARAM;

    uint16_t cfg;
    opt3001_err_t err = read_reg16(dev, REG_CONFIG, &cfg);
    if (err != OPT3001_OK)
        return err;

    cfg = (uint16_t)((cfg & (uint16_t)~CFG_MODE_MASK) | CFG_MODE_SINGLE);
    return write_reg16(dev, REG_CONFIG, cfg);
}

opt3001_err_t opt3001_set_continuous(opt3001_t *dev)
{
    if (!dev)
        return OPT3001_ERR_PARAM;

    uint16_t cfg;
    opt3001_err_t err = read_reg16(dev, REG_CONFIG, &cfg);
    if (err != OPT3001_OK)
        return err;

    cfg = (uint16_t)((cfg & (uint16_t)~CFG_MODE_MASK) | CFG_MODE_CONTINUOUS);
    return write_reg16(dev, REG_CONFIG, cfg);
}

bool opt3001_data_ready(opt3001_t *dev)
{
    if (!dev)
        return false;

    uint16_t cfg;
    if (read_reg16(dev, REG_CONFIG, &cfg) != OPT3001_OK)
        return false;

    return (cfg & CFG_CRF_BIT) != 0;
}
