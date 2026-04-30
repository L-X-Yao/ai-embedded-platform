/*
 * hmc5883l — Honeywell HMC5883L 3-axis digital magnetometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */
#include "hmc5883l.h"

#define REG_CONFIG_A 0x00
#define REG_CONFIG_B 0x01
#define REG_MODE     0x02
#define REG_DATA_X_H 0x03
#define REG_ID_A     0x0A

static const float gain_scale[] = {
    1.0f / 1370.0f,
    1.0f / 1090.0f,
    1.0f / 820.0f,
    1.0f / 660.0f,
    1.0f / 440.0f,
    1.0f / 390.0f,
    1.0f / 330.0f,
    1.0f / 230.0f,
};

hmc5883l_err_t hmc5883l_init(hmc5883l_t *dev, const hmc5883l_i2c_t *i2c,
                             const hmc5883l_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return HMC5883L_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return HMC5883L_ERR_PARAM;
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    int gain_idx = (int)cfg->gain;
    if (gain_idx < 0) gain_idx = 0;
    if (gain_idx > 7) gain_idx = 7;
    dev->scale = gain_scale[gain_idx];

    uint8_t id[3];
    if (!dev->i2c.read_regs(dev->i2c.ctx, HMC5883L_ADDR, REG_ID_A, id, 3))
        return HMC5883L_ERR_I2C;
    if (id[0] != 'H' || id[1] != '4' || id[2] != '3')
        return HMC5883L_ERR_ID;

    uint8_t cfg_a = (uint8_t)(((uint8_t)cfg->rate & 0x07) << 2);
    if (!dev->i2c.write_reg(dev->i2c.ctx, HMC5883L_ADDR, REG_CONFIG_A, cfg_a))
        return HMC5883L_ERR_I2C;

    uint8_t cfg_b = (uint8_t)(((uint8_t)cfg->gain & 0x07) << 5);
    if (!dev->i2c.write_reg(dev->i2c.ctx, HMC5883L_ADDR, REG_CONFIG_B, cfg_b))
        return HMC5883L_ERR_I2C;

    if (!dev->i2c.write_reg(dev->i2c.ctx, HMC5883L_ADDR, REG_MODE, 0x00))
        return HMC5883L_ERR_I2C;

    return HMC5883L_OK;
}

hmc5883l_err_t hmc5883l_read_raw(hmc5883l_t *dev, hmc5883l_raw_t *raw)
{
    if (!dev || !raw)
        return HMC5883L_ERR_PARAM;

    uint8_t buf[6];
    if (!dev->i2c.read_regs(dev->i2c.ctx, HMC5883L_ADDR, REG_DATA_X_H, buf, 6))
        return HMC5883L_ERR_I2C;

    raw->x = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    raw->z = (int16_t)((uint16_t)buf[2] << 8 | buf[3]);
    raw->y = (int16_t)((uint16_t)buf[4] << 8 | buf[5]);
    return HMC5883L_OK;
}

hmc5883l_err_t hmc5883l_read_gauss(hmc5883l_t *dev, hmc5883l_data_t *out)
{
    if (!dev || !out)
        return HMC5883L_ERR_PARAM;

    hmc5883l_raw_t raw;
    hmc5883l_err_t err = hmc5883l_read_raw(dev, &raw);
    if (err != HMC5883L_OK)
        return err;

    out->x = (float)raw.x * dev->scale;
    out->y = (float)raw.y * dev->scale;
    out->z = (float)raw.z * dev->scale;
    return HMC5883L_OK;
}
