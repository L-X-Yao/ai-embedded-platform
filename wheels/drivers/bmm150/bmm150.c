// SPDX-License-Identifier: MIT
/*
 * bmm150 — Bosch BMM150 3-axis geomagnetic sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "bmm150.h"
#include <string.h>

#define REG_CHIP_ID    0x40u
#define REG_DATA_X_LSB 0x42u
#define REG_PWR_CTRL   0x4Bu
#define REG_OP_MODE    0x4Cu

/* Soft reset bits in the power control register; bit 0 is the power control */
#define PWR_SOFT_RESET 0x82u

/* An axis that overflowed reads as this marker instead of a measurement */
#define XY_OVERFLOW    (-4096)
#define Z_OVERFLOW     (-16384)

/* Linear scaling (no trim compensation) */
#define XY_UT_PER_LSB  0.3f
#define Z_UT_PER_LSB   0.15f

/* The steps of init that talk to the chip */
static bmm150_err_t bring_up(const bmm150_i2c_t *i2c, const bmm150_cfg_t *cfg)
{
    /* Power on (set power control bit) */
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_PWR_CTRL, 0x01))
        return BMM150_ERR_I2C;
    i2c->delay_ms(i2c->ctx, 3);

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, cfg->addr, REG_CHIP_ID, &id, 1))
        return BMM150_ERR_I2C;
    if (id != BMM150_CHIP_ID)
        return BMM150_ERR_ID;

    /* Normal mode, default repetitions */
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_OP_MODE, 0x00))
        return BMM150_ERR_I2C;

    return BMM150_OK;
}

bmm150_err_t bmm150_init(bmm150_t *dev, const bmm150_i2c_t *i2c,
                          const bmm150_cfg_t *cfg)
{
    if (!dev)
        return BMM150_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return BMM150_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs || !i2c->delay_ms)
        return BMM150_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    bmm150_err_t err = bring_up(i2c, cfg);
    if (err != BMM150_OK)
        dev->i2c = NULL;
    return err;
}

bmm150_err_t bmm150_read(bmm150_t *dev, bmm150_data_t *out)
{
    if (!dev || !dev->i2c || !out)
        return BMM150_ERR_PARAM;

    uint8_t buf[6];
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr,
                              REG_DATA_X_LSB, buf, 6))
        return BMM150_ERR_I2C;

    /* X/Y: 13-bit signed, left-justified with 3 LSB status bits */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]) >> 3;
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]) >> 3;
    /* Z: 15-bit signed, left-justified with 1 LSB status bit */
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]) >> 1;

    if (raw_x == XY_OVERFLOW || raw_y == XY_OVERFLOW || raw_z == Z_OVERFLOW)
        return BMM150_ERR_OVERFLOW;

    out->x_ut = (float)raw_x * XY_UT_PER_LSB;
    out->y_ut = (float)raw_y * XY_UT_PER_LSB;
    out->z_ut = (float)raw_z * Z_UT_PER_LSB;

    return BMM150_OK;
}

bmm150_err_t bmm150_reset(bmm150_t *dev)
{
    if (!dev || !dev->i2c)
        return BMM150_ERR_PARAM;
    /* Keep the power bit: writing it as 0 would suspend the chip */
    uint8_t pwr = 0;
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, REG_PWR_CTRL, &pwr, 1))
        return BMM150_ERR_I2C;
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, REG_PWR_CTRL,
                             (uint8_t)(pwr | PWR_SOFT_RESET)))
        return BMM150_ERR_I2C;
    if (dev->i2c->delay_ms)
        dev->i2c->delay_ms(dev->i2c->ctx, 3);

    /* Soft reset leaves the chip in sleep mode, so measurements have to restart */
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, REG_OP_MODE, 0x00))
        return BMM150_ERR_I2C;
    return BMM150_OK;
}
