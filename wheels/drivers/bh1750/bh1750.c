// SPDX-License-Identifier: MIT
/*
 * bh1750 — ROHM BH1750FVI ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "bh1750.h"
#include "contract.h"
#include <string.h>

#define CMD_POWER_ON  0x01
#define CMD_POWER_OFF 0x00
#define CMD_RESET     0x07

/* Maximum measurement times: tHR and tLR (manual p.2) */
#define T_H_RES_MAX_MS 180
#define T_L_RES_MAX_MS 24

static bh1750_err_t send_cmd(const bh1750_t *dev, uint8_t cmd)
{
    if (!dev->i2c->write(dev->i2c->ctx, dev->cfg.addr, &cmd, 1))
        return BH1750_ERR_I2C;
    return BH1750_OK;
}

/*
 * Start a measurement and wait out its maximum time, because until it ends the
 * Data Register still holds 0 or the old value. With no delay_ms that wait falls
 * to the caller.
 */
static bh1750_err_t start_measurement(const bh1750_t *dev)
{
    bh1750_err_t err = send_cmd(dev, (uint8_t)dev->cfg.mode);
    if (err != BH1750_OK)
        return err;

    if (dev->i2c->delay_ms) {
        bool l_res = dev->cfg.mode == BH1750_CONT_L_RES ||
                     dev->cfg.mode == BH1750_ONCE_L_RES;
        dev->i2c->delay_ms(dev->i2c->ctx, l_res ? T_L_RES_MAX_MS : T_H_RES_MAX_MS);
    }
    return BH1750_OK;
}

static bool one_time_mode(bh1750_mode_t mode)
{
    return mode == BH1750_ONCE_H_RES || mode == BH1750_ONCE_H_RES2 ||
           mode == BH1750_ONCE_L_RES;
}

/* The steps of init that talk to the chip */
static bh1750_err_t bring_up(const bh1750_t *dev)
{
    bh1750_err_t err = send_cmd(dev, CMD_POWER_ON);
    if (err != BH1750_OK)
        return err;

    return start_measurement(dev);
}

bh1750_err_t bh1750_init(bh1750_t *dev, const bh1750_i2c_t *i2c,
                         const bh1750_cfg_t *cfg)
{
    if (!dev)
        return BH1750_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return BH1750_ERR_PARAM;
    if (!i2c->write || !i2c->read)
        return BH1750_ERR_PARAM;

    switch (cfg->mode) {
    case BH1750_CONT_H_RES: case BH1750_CONT_H_RES2: case BH1750_CONT_L_RES:
    case BH1750_ONCE_H_RES: case BH1750_ONCE_H_RES2: case BH1750_ONCE_L_RES:
        break;
    default:
        return BH1750_ERR_PARAM;
    }

    /* bh1750_read_lux measures and reads in one call in these modes */
    if (one_time_mode(cfg->mode) && !i2c->delay_ms)
        return BH1750_ERR_PARAM;

    dev->i2c = i2c;
    dev->cfg = *cfg;

    bh1750_err_t err = bring_up(dev);
    if (err != BH1750_OK)
        dev->i2c = NULL;
    return err;
}

bh1750_err_t bh1750_read_lux(const bh1750_t *dev, float *lux)
{
    if (!dev || !dev->i2c || !lux)
        return BH1750_ERR_PARAM;

    /* The chip powers down after a one-time measurement, so the data never
     * changes again unless the instruction is resent (manual p.7) */
    if (one_time_mode(dev->cfg.mode)) {
        bh1750_err_t err = start_measurement(dev);
        if (err != BH1750_OK)
            return err;
    }

    uint8_t buf[2] = {0};
    if (!dev->i2c->read(dev->i2c->ctx, dev->cfg.addr, buf, 2))
        return BH1750_ERR_I2C;

    uint16_t raw = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);

    if (dev->cfg.mode == BH1750_CONT_H_RES2 ||
        dev->cfg.mode == BH1750_ONCE_H_RES2)
        *lux = (float)raw / 2.4f;
    else
        *lux = (float)raw / 1.2f;

    ENSURE(*lux == *lux);  /* not NaN */
    ENSURE(*lux >= 0.0f);
    return BH1750_OK;
}

bh1750_err_t bh1750_power_on(const bh1750_t *dev)
{
    if (!dev || !dev->i2c) return BH1750_ERR_PARAM;

    bh1750_err_t err = send_cmd(dev, CMD_POWER_ON);
    if (err != BH1750_OK)
        return err;

    /* Power On alone is "Waiting for measurement command" (manual p.5), so a
     * continuous mode needs its command again. A one-time mode sends that in
     * bh1750_read_lux. */
    if (one_time_mode(dev->cfg.mode))
        return BH1750_OK;

    return start_measurement(dev);
}

bh1750_err_t bh1750_power_off(const bh1750_t *dev)
{
    if (!dev || !dev->i2c) return BH1750_ERR_PARAM;
    return send_cmd(dev, CMD_POWER_OFF);
}

bh1750_err_t bh1750_reset(const bh1750_t *dev)
{
    if (!dev || !dev->i2c) return BH1750_ERR_PARAM;
    return send_cmd(dev, CMD_RESET);
}
