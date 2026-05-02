/*
 * lis3dh — ST LIS3DH I2C 3-axis accelerometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "lis3dh.h"
#include <string.h>

/* ---------- register map ---------- */
#define REG_WHO_AM_I    0x0Fu
#define REG_CTRL_REG1   0x20u
#define REG_CTRL_REG4   0x23u
#define REG_OUT_X_L     0x28u

/* CTRL_REG1 bits */
#define CTRL1_XEN       0x01u
#define CTRL1_YEN       0x02u
#define CTRL1_ZEN       0x04u
#define CTRL1_AXES_ALL  (CTRL1_XEN | CTRL1_YEN | CTRL1_ZEN)

/* CTRL_REG4 bits */
#define CTRL4_BDU       0x80u   /* block data update */

/* Multi-byte read: set MSB of sub-address for auto-increment */
#define AUTO_INC        0x80u

/*
 * Scale divisors for converting raw 16-bit left-justified to G.
 * LIS3DH is 12-bit; data sits in bits [15:4] of the 16-bit register.
 * Datasheet sensitivity (normal 12-bit mode):
 *   +/-2g  : 1 mg/digit  -> raw16 / (16 * 1000)      = raw16 / 16000
 *   +/-4g  : 2 mg/digit  -> raw16 / (16 * 1000 / 2)   = raw16 / 8000
 *   +/-8g  : 4 mg/digit  -> raw16 / (16 * 1000 / 4)   = raw16 / 4000
 *   +/-16g : 12 mg/digit -> raw16 / (16 * 1000 / 12)   = raw16 / 1333.33
 */
static const float range_scale[] = {
    16000.0f,       /* 2g  */
     8000.0f,       /* 4g  */
     4000.0f,       /* 8g  */
     1333.333f,     /* 16g */
};

/* ---------- I2C helpers ---------- */

static bool write_reg(const lis3dh_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c.write_reg(dev->i2c.ctx, dev->cfg.addr, reg, val);
}

static bool read_regs(const lis3dh_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c.read_regs(dev->i2c.ctx, dev->cfg.addr,
                              reg | AUTO_INC, buf, len);
}

/* ---------- public API ---------- */

lis3dh_err_t lis3dh_init(lis3dh_t *dev, const lis3dh_i2c_t *i2c,
                         const lis3dh_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return LIS3DH_ERR_PARAM;
    if ((unsigned)cfg->range > 3 || (unsigned)cfg->odr > 9)
        return LIS3DH_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    /* Verify chip identity */
    uint8_t id = 0;
    if (!dev->i2c.read_regs(dev->i2c.ctx, dev->cfg.addr,
                            REG_WHO_AM_I, &id, 1))
        return LIS3DH_ERR_I2C;
    if (id != LIS3DH_WHO_AM_I)
        return LIS3DH_ERR_ID;

    /* CTRL_REG1: ODR + enable all axes */
    uint8_t ctrl1 = (uint8_t)((unsigned)cfg->odr << 4) | CTRL1_AXES_ALL;
    if (!write_reg(dev, REG_CTRL_REG1, ctrl1))
        return LIS3DH_ERR_I2C;

    /* CTRL_REG4: BDU + full-scale range */
    uint8_t ctrl4 = CTRL4_BDU | (uint8_t)((unsigned)cfg->range << 4);
    if (!write_reg(dev, REG_CTRL_REG4, ctrl4))
        return LIS3DH_ERR_I2C;

    dev->scale = range_scale[cfg->range & 0x03];
    return LIS3DH_OK;
}

lis3dh_err_t lis3dh_read_accel(const lis3dh_t *dev, lis3dh_accel_t *out)
{
    if (!dev || !out)
        return LIS3DH_ERR_PARAM;

    uint8_t buf[6];
    if (!read_regs(dev, REG_OUT_X_L, buf, 6))
        return LIS3DH_ERR_I2C;

    int16_t rx = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t ry = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t rz = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->x = (float)rx / dev->scale;
    out->y = (float)ry / dev->scale;
    out->z = (float)rz / dev->scale;
    return LIS3DH_OK;
}

lis3dh_err_t lis3dh_read_raw(const lis3dh_t *dev, lis3dh_raw_t *out)
{
    if (!dev || !out)
        return LIS3DH_ERR_PARAM;

    uint8_t buf[6];
    if (!read_regs(dev, REG_OUT_X_L, buf, 6))
        return LIS3DH_ERR_I2C;

    out->x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    out->y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    out->z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
    return LIS3DH_OK;
}

lis3dh_err_t lis3dh_set_range(lis3dh_t *dev, lis3dh_range_t range)
{
    if (!dev || (unsigned)range > 3)
        return LIS3DH_ERR_PARAM;

    uint8_t ctrl4 = CTRL4_BDU | (uint8_t)((unsigned)range << 4);
    if (!write_reg(dev, REG_CTRL_REG4, ctrl4))
        return LIS3DH_ERR_I2C;

    dev->cfg.range = range;
    dev->scale = range_scale[range & 0x03];
    return LIS3DH_OK;
}

lis3dh_err_t lis3dh_set_odr(lis3dh_t *dev, lis3dh_odr_t odr)
{
    if (!dev || (unsigned)odr > 9)
        return LIS3DH_ERR_PARAM;

    uint8_t ctrl1 = (uint8_t)((unsigned)odr << 4) | CTRL1_AXES_ALL;
    if (!write_reg(dev, REG_CTRL_REG1, ctrl1))
        return LIS3DH_ERR_I2C;

    dev->cfg.odr = odr;
    return LIS3DH_OK;
}
