// SPDX-License-Identifier: MIT
/*
 * tmp117 — TI TMP117 high-accuracy digital temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "tmp117.h"

#define REG_TEMP_RESULT 0x00u
#define REG_CONFIG      0x01u
#define REG_DEVICE_ID   0x0Fu

#define TMP117_RESOLUTION 0.0078125f  /* °C per LSB (1/128) */

static bool read_reg16(const tmp117_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2))
        return false;
    *val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return true;
}

static bool write_reg16(const tmp117_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    return dev->i2c->write_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2);
}

tmp117_err_t tmp117_init(tmp117_t *dev, const tmp117_i2c_t *i2c,
                         const tmp117_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return TMP117_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return TMP117_ERR_PARAM;
    if ((unsigned)cfg->avg > 3)
        return TMP117_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    uint16_t id = 0;
    if (!read_reg16(dev, REG_DEVICE_ID, &id))
        return TMP117_ERR_I2C;
    if (id != TMP117_DEVICE_ID)
        return TMP117_ERR_ID;

    uint16_t config = (uint16_t)((unsigned)cfg->avg << 5);
    if (!write_reg16(dev, REG_CONFIG, config))
        return TMP117_ERR_I2C;

    return TMP117_OK;
}

tmp117_err_t tmp117_read(tmp117_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return TMP117_ERR_PARAM;

    uint16_t raw = 0;
    if (!read_reg16(dev, REG_TEMP_RESULT, &raw))
        return TMP117_ERR_I2C;

    *temp_c = (float)(int16_t)raw * TMP117_RESOLUTION;
    return TMP117_OK;
}

tmp117_err_t tmp117_reset(tmp117_t *dev)
{
    if (!dev)
        return TMP117_ERR_PARAM;
    if (!dev->i2c)
        return TMP117_ERR_PARAM;
    if (!write_reg16(dev, REG_CONFIG, 0x0002))
        return TMP117_ERR_I2C;
    if (dev->i2c->delay_ms)
        dev->i2c->delay_ms(dev->i2c->ctx, 2);
    return TMP117_OK;
}
