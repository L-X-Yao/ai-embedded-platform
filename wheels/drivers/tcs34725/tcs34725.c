// SPDX-License-Identifier: MIT
/*
 * tcs34725 — AMS TCS34725 I2C RGB color sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "tcs34725.h"
#include "contract.h"
#include <string.h>

/* --- Register addresses (raw, without command bit) --- */
#define REG_ENABLE  0x00u
#define REG_ATIME   0x01u
#define REG_CONTROL 0x0Fu
#define REG_ID      0x12u
#define REG_STATUS  0x13u
#define REG_CDATAL  0x14u   /* RGBC data: C(14-15) R(16-17) G(18-19) B(1A-1B) */

/* Command register (p.14): bit 7 must be set for register access. TYPE in
 * bits 6:5 sets the protocol: 00b repeats one register on every byte,
 * 01b auto-increments, which a multi-byte read needs. */
#define CMD(reg)          (uint8_t)(0x80u | (reg))
#define CMD_AUTO_INC(reg) (uint8_t)(0xA0u | (reg))

/* Enable register bits */
#define ENABLE_PON  0x01u   /* power on */
#define ENABLE_AEN  0x02u   /* RGBC enable */

/* Status register bits */
#define STATUS_AVALID 0x01u  /* an integration cycle has finished */

/* p.15: at least 2.4 ms from PON to the start of an RGBC, in whole ms */
#define PON_WAIT_MS 3u

#define CHIP_ID_44  0x44u
#define CHIP_ID_4D  0x4Du

/* DN40 Table 1 (p.13), TCS3472 row: open air, GA = 1 */
#define DN40_CT_COEF    3810.0f
#define DN40_CT_OFFSET  1391.0f

static tcs34725_err_t write_reg(const tcs34725_t *dev,
                                uint8_t reg, uint8_t val)
{
    if (!dev->i2c->write_reg(dev->i2c->ctx, TCS34725_ADDR, CMD(reg), val))
        return TCS34725_ERR_I2C;
    return TCS34725_OK;
}

static tcs34725_err_t read_regs(const tcs34725_t *dev,
                                uint8_t cmd, uint8_t *buf, uint16_t len)
{
    if (!dev->i2c->read_regs(dev->i2c->ctx, TCS34725_ADDR, cmd, buf, len))
        return TCS34725_ERR_I2C;
    return TCS34725_OK;
}

/* Power on, then start the RGBC once the PON interval has passed */
static tcs34725_err_t power_on(const tcs34725_t *dev)
{
    tcs34725_err_t err = write_reg(dev, REG_ENABLE, ENABLE_PON);
    if (err != TCS34725_OK)
        return err;
    dev->i2c->delay_ms(dev->i2c->ctx, PON_WAIT_MS);
    return write_reg(dev, REG_ENABLE, ENABLE_PON | ENABLE_AEN);
}

/* The steps of init that talk to the chip */
static tcs34725_err_t bring_up(const tcs34725_t *dev)
{
    /* Verify chip ID */
    uint8_t id = 0;
    tcs34725_err_t err = read_regs(dev, CMD(REG_ID), &id, 1);
    if (err != TCS34725_OK)
        return err;
    if (id != CHIP_ID_44 && id != CHIP_ID_4D)
        return TCS34725_ERR_ID;

    /* Set integration time */
    err = write_reg(dev, REG_ATIME, (uint8_t)dev->integ);
    if (err != TCS34725_OK)
        return err;

    /* Set gain */
    err = write_reg(dev, REG_CONTROL, (uint8_t)dev->gain);
    if (err != TCS34725_OK)
        return err;

    /* Enable: power on + RGBC */
    return power_on(dev);
}

tcs34725_err_t tcs34725_init(tcs34725_t *dev,
                             const tcs34725_i2c_t *i2c,
                             tcs34725_integ_t integ,
                             tcs34725_gain_t gain)
{
    if (!dev)
        return TCS34725_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c)
        return TCS34725_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs || !i2c->delay_ms)
        return TCS34725_ERR_PARAM;
    if ((unsigned)gain > TCS34725_GAIN_60X)
        return TCS34725_ERR_PARAM;

    dev->i2c   = i2c;
    dev->integ = integ;
    dev->gain  = gain;

    tcs34725_err_t err = bring_up(dev);
    if (err != TCS34725_OK)
        dev->i2c = NULL;
    return err;
}

tcs34725_err_t tcs34725_read_rgbc(tcs34725_t *dev,
                                  tcs34725_rgbc_t *out)
{
    if (!dev || !dev->i2c || !out)
        return TCS34725_ERR_PARAM;

    /* p.19, AVALID: "Indicates that the RGBC channels have completed an
     * integration cycle." */
    uint8_t status = 0;
    tcs34725_err_t err = read_regs(dev, CMD(REG_STATUS), &status, 1);
    if (err != TCS34725_OK)
        return err;
    if (!(status & STATUS_AVALID))
        return TCS34725_ERR_NOT_READY;

    uint8_t buf[8];
    err = read_regs(dev, CMD_AUTO_INC(REG_CDATAL), buf, 8);
    if (err != TCS34725_OK)
        return err;

    /* 16-bit little-endian: low byte first */
    out->c = (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
    out->r = (uint16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
    out->g = (uint16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));
    out->b = (uint16_t)((uint16_t)buf[6] | ((uint16_t)buf[7] << 8));
    return TCS34725_OK;
}

tcs34725_err_t tcs34725_set_integration_time(tcs34725_t *dev,
                                             tcs34725_integ_t integ)
{
    if (!dev || !dev->i2c)
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
    if (!dev || !dev->i2c)
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
    if (!dev || !dev->i2c)
        return TCS34725_ERR_PARAM;
    return power_on(dev);
}

tcs34725_err_t tcs34725_disable(tcs34725_t *dev)
{
    if (!dev || !dev->i2c)
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

    /* DN40 p.4: no IR channel, so IR = (R + G + B - C) / 2 comes off each
     * channel. Every term fits a float exactly. */
    float ir = ((float)rgbc->r + (float)rgbc->g +
                (float)rgbc->b - (float)rgbc->c) / 2.0f;
    float r_ir = (float)rgbc->r - ir;
    float b_ir = (float)rgbc->b - ir;

    /* Nothing left of R: the ratio below has no value */
    if (r_ir <= 0.0f)
        return TCS34725_ERR_PARAM;

    /* DN40 p.6: CT = CT_Coef * B' / R' + CT_Offset */
    float cct = DN40_CT_COEF * b_ir / r_ir + DN40_CT_OFFSET;

    if (cct < 0.0f)
        cct = 0.0f;
    *cct_k = (cct >= 65535.0f) ? 65535u : (uint16_t)cct;
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
    ENSURE(*lux == *lux);  /* not NaN */
    ENSURE(*lux >= 0.0f);
    return TCS34725_OK;
}
