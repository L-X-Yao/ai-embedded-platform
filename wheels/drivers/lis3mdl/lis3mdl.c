// SPDX-License-Identifier: MIT
/*
 * lis3mdl — ST LIS3MDL 3-axis digital magnetometer driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "lis3mdl.h"
#include "contract.h"
#include <string.h>

/* ---------- register map ---------- */

#define REG_WHO_AM_I   0x0Fu
#define REG_CTRL_REG1  0x20u
#define REG_CTRL_REG2  0x21u
#define REG_CTRL_REG3  0x22u
#define REG_OUT_X_L    0x28u   /* data block: 0x28..0x2D (XL,XH,YL,YH,ZL,ZH) */

#define WHO_AM_I_VAL   0x3Du

/* LIS3MDL auto-increment: set bit 7 of sub-address register */
#define AUTO_INC(reg)  ((uint8_t)((reg) | 0x80u))

/* CTRL_REG1 defaults: XY medium performance, 10 Hz ODR, no temp sensor */
#define CTRL_REG1_DEFAULT 0x30u  /* OM=01 (medium), DO=100 (10 Hz) */

/* CTRL_REG3: continuous conversion mode = 0x00 */
#define CTRL_REG3_CONTINUOUS 0x00u

/* ---------- sensitivity table (LSB per gauss) ---------- */

static const float sensitivity_lsb_per_gauss[] = {
    6842.0f,  /* +/-4 gauss  */
    3421.0f,  /* +/-8 gauss  */
    2281.0f,  /* +/-12 gauss */
    1711.0f,  /* +/-16 gauss */
};

/* ---------- API implementation ---------- */

/* The steps of init that talk to the chip */
static lis3mdl_err_t bring_up(const lis3mdl_i2c_t *i2c,
                              const lis3mdl_cfg_t *cfg)
{
    /* Read and verify WHO_AM_I */
    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, cfg->addr, REG_WHO_AM_I, &id, 1))
        return LIS3MDL_ERR_I2C;
    if (id != WHO_AM_I_VAL)
        return LIS3MDL_ERR_ID;

    /* CTRL_REG1: XY medium performance, 10 Hz ODR */
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_CTRL_REG1, CTRL_REG1_DEFAULT))
        return LIS3MDL_ERR_I2C;

    /* CTRL_REG2: full-scale range selection (bits [6:5]) */
    uint8_t ctrl2 = (uint8_t)(((uint8_t)cfg->range & 0x03u) << 5);
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_CTRL_REG2, ctrl2))
        return LIS3MDL_ERR_I2C;

    /* CTRL_REG3: continuous conversion mode */
    if (!i2c->write_reg(i2c->ctx, cfg->addr, REG_CTRL_REG3, CTRL_REG3_CONTINUOUS))
        return LIS3MDL_ERR_I2C;

    return LIS3MDL_OK;
}

lis3mdl_err_t lis3mdl_init(lis3mdl_t *dev, const lis3mdl_i2c_t *i2c,
                           const lis3mdl_cfg_t *cfg)
{
    if (!dev)
        return LIS3MDL_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it, whose chip was never verified */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return LIS3MDL_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return LIS3MDL_ERR_PARAM;
    if ((unsigned)cfg->range > 3u)
        return LIS3MDL_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;
    dev->sensitivity = sensitivity_lsb_per_gauss[(unsigned)cfg->range];

    lis3mdl_err_t err = bring_up(i2c, cfg);
    if (err != LIS3MDL_OK)
        dev->i2c = NULL;
    return err;
}

lis3mdl_err_t lis3mdl_read_raw(const lis3mdl_t *dev, lis3mdl_raw_t *raw)
{
    if (!dev || !raw)
        return LIS3MDL_ERR_PARAM;
    if (!dev->i2c || !dev->i2c->read_regs)
        return LIS3MDL_ERR_PARAM;

    uint8_t buf[6];
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr,
                             AUTO_INC(REG_OUT_X_L), buf, 6))
        return LIS3MDL_ERR_I2C;

    /* Little-endian: XL,XH,YL,YH,ZL,ZH */
    raw->x = (int16_t)(uint16_t)((uint16_t)buf[1] << 8 | (uint16_t)buf[0]);
    raw->y = (int16_t)(uint16_t)((uint16_t)buf[3] << 8 | (uint16_t)buf[2]);
    raw->z = (int16_t)(uint16_t)((uint16_t)buf[5] << 8 | (uint16_t)buf[4]);
    return LIS3MDL_OK;
}

lis3mdl_err_t lis3mdl_read(lis3mdl_t *dev, lis3mdl_data_t *out)
{
    if (!dev || !out)
        return LIS3MDL_ERR_PARAM;

    lis3mdl_raw_t raw;
    lis3mdl_err_t err = lis3mdl_read_raw(dev, &raw);
    if (err != LIS3MDL_OK)
        return err;

    /* Convert: value_ut = (raw / sensitivity_lsb_per_gauss) * 100.0f */
    out->x_ut = (float)raw.x / dev->sensitivity * 100.0f;
    out->y_ut = (float)raw.y / dev->sensitivity * 100.0f;
    out->z_ut = (float)raw.z / dev->sensitivity * 100.0f;

    ENSURE(out->x_ut == out->x_ut);  /* not NaN */
    ENSURE(out->y_ut == out->y_ut);
    ENSURE(out->z_ut == out->z_ut);

    return LIS3MDL_OK;
}
