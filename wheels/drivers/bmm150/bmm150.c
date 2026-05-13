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

#define REG_CHIP_ID    0x40u
#define REG_DATA_X_LSB 0x42u
#define REG_PWR_CTRL   0x4Bu
#define REG_OP_MODE    0x4Cu

/* Linear scaling (no trim compensation) */
#define XY_UT_PER_LSB  0.3f
#define Z_UT_PER_LSB   0.15f

bmm150_err_t bmm150_init(bmm150_t *dev, const bmm150_i2c_t *i2c,
                          const bmm150_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return BMM150_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return BMM150_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Power on (set power control bit) */
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_PWR_CTRL, 0x01))
        return BMM150_ERR_I2C;
    if (i2c->delay_ms)
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

bmm150_err_t bmm150_read(bmm150_t *dev, bmm150_data_t *out)
{
    if (!dev || !out)
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

    out->x_ut = (float)raw_x * XY_UT_PER_LSB;
    out->y_ut = (float)raw_y * XY_UT_PER_LSB;
    out->z_ut = (float)raw_z * Z_UT_PER_LSB;

    return BMM150_OK;
}

bmm150_err_t bmm150_reset(bmm150_t *dev)
{
    if (!dev)
        return BMM150_ERR_PARAM;
    /* Soft reset: set bit 7 and power control bit 0 */
    if (!dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, REG_PWR_CTRL, 0x82))
        return BMM150_ERR_I2C;
    if (dev->i2c->delay_ms)
        dev->i2c->delay_ms(dev->i2c->ctx, 3);
    return BMM150_OK;
}
