// SPDX-License-Identifier: MIT
/*
 * tsl2591 — AMS TSL2591 high-dynamic-range ambient light sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "tsl2591.h"
#include "contract.h"
#include <string.h>

/* ---------- ENABLE register bits ---------- */
#define ENABLE_PON   0x01u   /* power on */
#define ENABLE_AEN   0x02u   /* ALS enable */

/* ---------- STATUS register bits ---------- */
#define STATUS_AVALID 0x01u  /* an integration cycle finished since AEN */

/* ---------- internal helpers ---------- */

static tsl2591_err_t write_reg(const tsl2591_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t cmd = (uint8_t)(TSL2591_CMD_BIT | reg);
    if (!dev->i2c->write_reg(dev->i2c->ctx, TSL2591_DEV_ADDR, cmd, val))
        return TSL2591_ERR_I2C;
    return TSL2591_OK;
}

static tsl2591_err_t read_regs(const tsl2591_t *dev, uint8_t reg,
                               uint8_t *buf, uint16_t len)
{
    uint8_t cmd = (uint8_t)(TSL2591_CMD_BIT | reg);
    if (!dev->i2c->read_regs(dev->i2c->ctx, TSL2591_DEV_ADDR, cmd, buf, len))
        return TSL2591_ERR_I2C;
    return TSL2591_OK;
}

/* The four data bytes, once there is a result. Data sheet p.16, AVALID:
 * "Indicates that the ADC channels have completed an integration cycle since
 * the AEN bit was asserted." */
static tsl2591_err_t read_channels(const tsl2591_t *dev, uint8_t *buf)
{
    uint8_t status = 0;
    tsl2591_err_t err = read_regs(dev, TSL2591_REG_STATUS, &status, 1);
    if (err != TSL2591_OK)
        return err;
    if (!(status & STATUS_AVALID))
        return TSL2591_ERR_NOT_READY;

    return read_regs(dev, TSL2591_REG_C0DATAL, buf, 4);
}

static bool gain_valid(tsl2591_gain_t g)
{
    return g == TSL2591_GAIN_LOW || g == TSL2591_GAIN_MEDIUM ||
           g == TSL2591_GAIN_HIGH || g == TSL2591_GAIN_MAX;
}

static bool itime_valid(tsl2591_itime_t t)
{
    return (unsigned)t <= (unsigned)TSL2591_ITIME_600MS;
}

static float gain_factor(tsl2591_gain_t g)
{
    switch (g) {
    case TSL2591_GAIN_LOW:    return 1.0f;
    case TSL2591_GAIN_MEDIUM: return 25.0f;
    case TSL2591_GAIN_HIGH:   return 428.0f;
    case TSL2591_GAIN_MAX:    return 9876.0f;
    default:                  return 1.0f;
    }
}

static float itime_factor_ms(tsl2591_itime_t t)
{
    /* Each step adds 100ms: 100, 200, 300, 400, 500, 600 */
    return 100.0f * ((float)(uint8_t)t + 1.0f);
}

static tsl2591_err_t write_config(const tsl2591_t *dev, tsl2591_gain_t gain,
                                  tsl2591_itime_t itime)
{
    uint8_t cfg = (uint8_t)((uint8_t)gain | (uint8_t)itime);
    return write_reg(dev, TSL2591_REG_CONFIG, cfg);
}

/* The steps of init that talk to the chip */
static tsl2591_err_t bring_up(const tsl2591_t *dev)
{
    /* verify chip ID */
    uint8_t id = 0;
    tsl2591_err_t err = read_regs(dev, TSL2591_REG_ID, &id, 1);
    if (err != TSL2591_OK)
        return err;
    if (id != TSL2591_DEVICE_ID)
        return TSL2591_ERR_ID;

    /* enable sensor: PON + AEN */
    err = write_reg(dev, TSL2591_REG_ENABLE, ENABLE_PON | ENABLE_AEN);
    if (err != TSL2591_OK)
        return err;

    /* configure gain + integration time */
    return write_config(dev, dev->gain, dev->itime);
}

/* ---------- public API ---------- */

tsl2591_err_t tsl2591_init(tsl2591_t *dev, const tsl2591_i2c_t *i2c,
                           tsl2591_gain_t gain, tsl2591_itime_t itime)
{
    if (!dev)
        return TSL2591_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c)
        return TSL2591_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return TSL2591_ERR_PARAM;
    if (!gain_valid(gain) || !itime_valid(itime))
        return TSL2591_ERR_PARAM;

    dev->i2c   = i2c;
    dev->gain  = gain;
    dev->itime = itime;

    tsl2591_err_t err = bring_up(dev);
    if (err != TSL2591_OK)
        dev->i2c = NULL;
    return err;
}

tsl2591_err_t tsl2591_read(tsl2591_t *dev, tsl2591_data_t *out)
{
    if (!dev || !dev->i2c || !out)
        return TSL2591_ERR_PARAM;

    /* read all 4 channel data bytes (C0DATAL..C1DATAH) */
    uint8_t buf[4] = {0};
    tsl2591_err_t err = read_channels(dev, buf);
    if (err != TSL2591_OK)
        return err;

    out->ch0_raw  = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    out->ch1_raw  = (uint16_t)((uint16_t)buf[3] << 8 | buf[2]);
    out->infrared = out->ch1_raw;

    if (out->ch0_raw >= out->ch1_raw)
        out->visible = (uint32_t)out->ch0_raw - (uint32_t)out->ch1_raw;
    else
        out->visible = 0;

    /* lux calculation per TSL2591 application note */
    float atime_ms = itime_factor_ms(dev->itime);
    float again    = gain_factor(dev->gain);
    float cpl      = (atime_ms * again) / 408.0f;

    if (!(cpl > 0.0f)) {
        /* guard against zero/NaN denominator */
        out->lux = 0.0f;
        return TSL2591_OK;
    }

    float lux = ((float)out->ch0_raw - (float)out->ch1_raw) / cpl;

    /* clamp negative to zero */
    if (!(lux >= 0.0f))
        lux = 0.0f;

    out->lux = lux;

    ENSURE(out->lux == out->lux);   /* not NaN */
    ENSURE(out->lux >= 0.0f);
    return TSL2591_OK;
}

tsl2591_err_t tsl2591_read_raw(tsl2591_t *dev, uint16_t *ch0, uint16_t *ch1)
{
    if (!dev || !dev->i2c || !ch0 || !ch1)
        return TSL2591_ERR_PARAM;

    uint8_t buf[4] = {0};
    tsl2591_err_t err = read_channels(dev, buf);
    if (err != TSL2591_OK)
        return err;

    *ch0 = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    *ch1 = (uint16_t)((uint16_t)buf[3] << 8 | buf[2]);
    return TSL2591_OK;
}

tsl2591_err_t tsl2591_set_gain(tsl2591_t *dev, tsl2591_gain_t gain)
{
    if (!dev || !dev->i2c)
        return TSL2591_ERR_PARAM;
    if (!gain_valid(gain))
        return TSL2591_ERR_PARAM;

    /* Stored only once written: tsl2591_read scales by what the chip really has */
    tsl2591_err_t err = write_config(dev, gain, dev->itime);
    if (err == TSL2591_OK)
        dev->gain = gain;
    return err;
}

tsl2591_err_t tsl2591_set_integration_time(tsl2591_t *dev, tsl2591_itime_t itime)
{
    if (!dev || !dev->i2c)
        return TSL2591_ERR_PARAM;
    if (!itime_valid(itime))
        return TSL2591_ERR_PARAM;

    tsl2591_err_t err = write_config(dev, dev->gain, itime);
    if (err == TSL2591_OK)
        dev->itime = itime;
    return err;
}

tsl2591_err_t tsl2591_enable(tsl2591_t *dev)
{
    if (!dev || !dev->i2c)
        return TSL2591_ERR_PARAM;

    return write_reg(dev, TSL2591_REG_ENABLE, ENABLE_PON | ENABLE_AEN);
}

tsl2591_err_t tsl2591_disable(tsl2591_t *dev)
{
    if (!dev || !dev->i2c)
        return TSL2591_ERR_PARAM;

    return write_reg(dev, TSL2591_REG_ENABLE, 0x00);
}

tsl2591_err_t tsl2591_get_status(tsl2591_t *dev, uint8_t *status)
{
    if (!dev || !dev->i2c || !status)
        return TSL2591_ERR_PARAM;

    return read_regs(dev, TSL2591_REG_STATUS, status, 1);
}
