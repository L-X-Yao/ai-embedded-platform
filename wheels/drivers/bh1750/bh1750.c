/*
 * bh1750 — ROHM BH1750FVI ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */
#include "bh1750.h"

#define CMD_POWER_ON  0x01
#define CMD_POWER_OFF 0x00
#define CMD_RESET     0x07

static bh1750_err_t send_cmd(bh1750_t *dev, uint8_t cmd)
{
    if (!dev->i2c.write(dev->i2c.ctx, dev->cfg.addr, &cmd, 1))
        return BH1750_ERR_I2C;
    return BH1750_OK;
}

bh1750_err_t bh1750_init(bh1750_t *dev, const bh1750_i2c_t *i2c,
                         const bh1750_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return BH1750_ERR_PARAM;
    if (!i2c->write || !i2c->read)
        return BH1750_ERR_PARAM;
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    bh1750_err_t err = send_cmd(dev, CMD_POWER_ON);
    if (err != BH1750_OK)
        return err;

    uint8_t mode = (uint8_t)dev->cfg.mode;
    return send_cmd(dev, mode);
}

bh1750_err_t bh1750_read_lux(bh1750_t *dev, float *lux)
{
    if (!dev || !lux)
        return BH1750_ERR_PARAM;

    uint8_t buf[2] = {0};
    if (!dev->i2c.read(dev->i2c.ctx, dev->cfg.addr, buf, 2))
        return BH1750_ERR_I2C;

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];

    if (dev->cfg.mode == BH1750_CONT_H_RES2 ||
        dev->cfg.mode == BH1750_ONCE_H_RES2)
        *lux = (float)raw / 2.4f;
    else
        *lux = (float)raw / 1.2f;

    return BH1750_OK;
}

bh1750_err_t bh1750_power_on(bh1750_t *dev)
{
    if (!dev) return BH1750_ERR_PARAM;
    return send_cmd(dev, CMD_POWER_ON);
}

bh1750_err_t bh1750_power_off(bh1750_t *dev)
{
    if (!dev) return BH1750_ERR_PARAM;
    return send_cmd(dev, CMD_POWER_OFF);
}

bh1750_err_t bh1750_reset(bh1750_t *dev)
{
    if (!dev) return BH1750_ERR_PARAM;
    return send_cmd(dev, CMD_RESET);
}
