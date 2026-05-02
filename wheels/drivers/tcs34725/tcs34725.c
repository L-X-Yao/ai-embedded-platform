// SPDX-License-Identifier: MIT
/*
 * tcs34725 — AMS TCS34725 I2C RGB color sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */
#include "tcs34725.h"

/* --- Register addresses (raw, without command bit) --- */
#define REG_ENABLE  0x00u
#define REG_ATIME   0x01u
#define REG_CONTROL 0x0Fu
#define REG_ID      0x12u
#define REG_CDATAL  0x14u   /* RGBC data: C(14-15) R(16-17) G(18-19) B(1A-1B) */

/* Command register: bit 7 must be set for register access */
#define CMD(reg)    (uint8_t)(0x80u | (reg))

/* Enable register bits */
#define ENABLE_PON  0x01u   /* power on */
#define ENABLE_AEN  0x02u   /* RGBC enable */

#define CHIP_ID_44  0x44u
#define CHIP_ID_4D  0x4Du

static tcs34725_err_t write_reg(const tcs34725_t *dev,
                                uint8_t reg, uint8_t val)
{
    if (!dev->i2c.write_reg(dev->i2c.ctx, TCS34725_ADDR, CMD(reg), val))
        return TCS34725_ERR_I2C;
    return TCS34725_OK;
}

static tcs34725_err_t read_regs(const tcs34725_t *dev,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (!dev->i2c.read_regs(dev->i2c.ctx, TCS34725_ADDR, CMD(reg), buf, len))
        return TCS34725_ERR_I2C;
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_init(tcs34725_t *dev,
                             const tcs34725_i2c_t *i2c,
                             tcs34725_integ_t integ,
                             tcs34725_gain_t gain)
{
    if (!dev || !i2c)
        return TCS34725_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return TCS34725_ERR_PARAM;
    if ((unsigned)gain > TCS34725_GAIN_60X)
        return TCS34725_ERR_PARAM;

    dev->i2c   = *i2c;
    dev->integ = integ;
    dev->gain  = gain;

    /* Verify chip ID */
    uint8_t id = 0;
    tcs34725_err_t err = read_regs(dev, REG_ID, &id, 1);
    if (err != TCS34725_OK)
        return err;
    if (id != CHIP_ID_44 && id != CHIP_ID_4D)
        return TCS34725_ERR_ID;

    /* Set integration time */
    err = write_reg(dev, REG_ATIME, (uint8_t)integ);
    if (err != TCS34725_OK)
        return err;

    /* Set gain */
    err = write_reg(dev, REG_CONTROL, (uint8_t)gain);
    if (err != TCS34725_OK)
        return err;

    /* Enable: power on + RGBC */
    return write_reg(dev, REG_ENABLE, ENABLE_PON | ENABLE_AEN);
}

tcs34725_err_t tcs34725_read_rgbc(tcs34725_t *dev,
                                  tcs34725_rgbc_t *out)
{
    if (!dev || !out)
        return TCS34725_ERR_PARAM;

    uint8_t buf[8];
    tcs34725_err_t err = read_regs(dev, REG_CDATAL, buf, 8);
    if (err != TCS34725_OK)
        return err;

    /* 16-bit little-endian: low byte first */
    out->c = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    out->r = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    out->g = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    out->b = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_set_integration_time(tcs34725_t *dev,
                                             tcs34725_integ_t integ)
{
    if (!dev)
        return TCS34725_ERR_PARAM;
    tcs34725_err_t err = write_reg(dev, REG_ATIME, (uint8_t)integ);
    if (err != TCS34725_OK)
        return err;
    dev->integ = integ;
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_set_gain(tcs34725_t *dev,
                                 tcs34725_gain_t gain)
{
    if (!dev)
        return TCS34725_ERR_PARAM;
    if ((unsigned)gain > TCS34725_GAIN_60X)
        return TCS34725_ERR_PARAM;
    tcs34725_err_t err = write_reg(dev, REG_CONTROL, (uint8_t)gain);
    if (err != TCS34725_OK)
        return err;
    dev->gain = gain;
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_enable(tcs34725_t *dev)
{
    if (!dev)
        return TCS34725_ERR_PARAM;
    return write_reg(dev, REG_ENABLE, ENABLE_PON | ENABLE_AEN);
}

tcs34725_err_t tcs34725_disable(tcs34725_t *dev)
{
    if (!dev)
        return TCS34725_ERR_PARAM;
    return write_reg(dev, REG_ENABLE, 0x00);
}

tcs34725_err_t tcs34725_get_color_temp(const tcs34725_rgbc_t *rgbc,
                                       uint16_t *cct_k)
{
    if (!rgbc || !cct_k)
        return TCS34725_ERR_PARAM;
    if (rgbc->c == 0)
        return TCS34725_ERR_PARAM;

    /* DN40 application note: chromaticity then McCamy's formula */
    float r_ratio = (float)rgbc->r / (float)rgbc->c;
    float b_ratio = (float)rgbc->b / (float)rgbc->c;

    /* Simplified CCT via empirical formula from DN40 */
    float x = -0.14282f * r_ratio + 1.54924f * b_ratio - 0.95641f;
    float n = (x - 0.3320f) / (0.1858f - x);
    float cct = 449.0f * n * n * n + 3525.0f * n * n + 6823.3f * n + 5520.33f;

    if (cct < 0.0f)
        cct = 0.0f;
    *cct_k = (uint16_t)cct;
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_get_lux(const tcs34725_rgbc_t *rgbc,
                                float *lux)
{
    if (!rgbc || !lux)
        return TCS34725_ERR_PARAM;

    /* DN40 empirical lux formula: -0.32466*R + 1.57837*G + -0.73191*B */
    float l = -0.32466f * (float)rgbc->r
            +  1.57837f * (float)rgbc->g
            + -0.73191f * (float)rgbc->b;
    if (l < 0.0f)
        l = 0.0f;
    *lux = l;
    return TCS34725_OK;
}
