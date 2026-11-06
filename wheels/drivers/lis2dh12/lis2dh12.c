// SPDX-License-Identifier: MIT
/*
 * lis2dh12 — ST LIS2DH12 3-axis accelerometer
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "lis2dh12.h"
#include <string.h>

#define REG_WHO_AM_I  0x0Fu
#define REG_CTRL_REG1 0x20u
#define REG_CTRL_REG4 0x23u
#define REG_OUT_X_L   0x28u

/* Normal mode (10-bit) sensitivity: mg per digit (datasheet Table 4) */
static const float sensitivity_table[] = {
    4.0f,   /* ±2g  */
    8.0f,   /* ±4g  */
    16.0f,  /* ±8g  */
    48.0f,  /* ±16g */
};

/* The steps of init that talk to the chip */
static lis2dh12_err_t bring_up(const lis2dh12_i2c_t *i2c,
                               const lis2dh12_cfg_t *cfg)
{
    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, cfg->addr, REG_WHO_AM_I, &id, 1))
        return LIS2DH12_ERR_I2C;
    if (id != LIS2DH12_WHO_AM_I)
        return LIS2DH12_ERR_ID;

    /* CTRL_REG1: ODR | normal mode | XYZ enable */
    uint8_t ctrl1 = (uint8_t)(((unsigned)cfg->odr << 4) | 0x07);
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_CTRL_REG1, ctrl1))
        return LIS2DH12_ERR_I2C;

    /* CTRL_REG4: BDU=1 | FS | normal mode (HR=0) */
    uint8_t ctrl4 = (uint8_t)(0x80 | ((unsigned)cfg->range << 4));
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_CTRL_REG4, ctrl4))
        return LIS2DH12_ERR_I2C;

    return LIS2DH12_OK;
}

lis2dh12_err_t lis2dh12_init(lis2dh12_t *dev, const lis2dh12_i2c_t *i2c,
                              const lis2dh12_cfg_t *cfg)
{
    if (!dev)
        return LIS2DH12_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other call refuses a
     * device without it, whose chip was never verified */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return LIS2DH12_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return LIS2DH12_ERR_PARAM;
    if ((unsigned)cfg->range > 3 || (unsigned)cfg->odr > 7)
        return LIS2DH12_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->sensitivity = sensitivity_table[(unsigned)cfg->range];

    lis2dh12_err_t err = bring_up(i2c, cfg);
    if (err != LIS2DH12_OK)
        dev->i2c = NULL;
    return err;
}

lis2dh12_err_t lis2dh12_read(lis2dh12_t *dev, lis2dh12_data_t *out)
{
    if (!dev || !dev->i2c || !out)
        return LIS2DH12_ERR_PARAM;

    uint8_t buf[6];
    /* Auto-increment bit (0x80) for multi-byte read */
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr,
                              REG_OUT_X_L | 0x80, buf, 6))
        return LIS2DH12_ERR_I2C;

    /* 10-bit left-justified: shift right 6 to get signed 10-bit value */
    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]) >> 6;
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]) >> 6;
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]) >> 6;

    out->x_mg = (float)raw_x * dev->sensitivity;
    out->y_mg = (float)raw_y * dev->sensitivity;
    out->z_mg = (float)raw_z * dev->sensitivity;

    return LIS2DH12_OK;
}
