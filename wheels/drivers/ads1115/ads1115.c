// SPDX-License-Identifier: MIT
/*
 * ads1115 — TI ADS1115 16-bit 4-channel ADC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */
#include "ads1115.h"
#include "contract.h"

#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01

static const float pga_scale_mv[] = {
    6144.0f / 32768.0f,
    4096.0f / 32768.0f,
    2048.0f / 32768.0f,
    1024.0f / 32768.0f,
    512.0f  / 32768.0f,
    256.0f  / 32768.0f,
};

static uint16_t build_config(const ads1115_cfg_t *cfg, bool start)
{
    uint16_t config = 0;
    if (start)
        config |= (1u << 15);
    config |= ((uint16_t)cfg->mux & 0x07) << 12;
    config |= ((uint16_t)cfg->pga & 0x07) << 9;
    config |= (1u << 8);
    config |= ((uint16_t)cfg->rate & 0x07) << 5;
    config |= 0x0003;
    return config;
}

static ads1115_err_t write_config(ads1115_t *dev, uint16_t config)
{
    uint8_t buf[2] = {(uint8_t)(config >> 8), (uint8_t)(config & 0xFF)};
    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONFIG, buf, 2))
        return ADS1115_ERR_I2C;
    return ADS1115_OK;
}

ads1115_err_t ads1115_init(ads1115_t *dev, const ads1115_i2c_t *i2c,
                           const ads1115_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return ADS1115_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return ADS1115_ERR_PARAM;
    if ((unsigned)cfg->mux > 7 || (unsigned)cfg->pga > 5 || (unsigned)cfg->rate > 7)
        return ADS1115_ERR_PARAM;
    dev->i2c = i2c;
    dev->cfg = *cfg;
    return ADS1115_OK;
}

ads1115_err_t ads1115_read_raw(ads1115_t *dev, int16_t *raw)
{
    if (!dev || !raw)
        return ADS1115_ERR_PARAM;

    uint16_t config = build_config(&dev->cfg, true);
    ads1115_err_t err = write_config(dev, config);
    if (err != ADS1115_OK)
        return err;

    /* Poll for conversion complete (bit 15 of config register = 1) */
    uint8_t cfg_buf[2];
    for (int retry = 0; retry < 200; retry++) {
        if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONFIG, cfg_buf, 2))
            return ADS1115_ERR_I2C;
        if (cfg_buf[0] & 0x80) break;
        if (dev->i2c->delay_ms)
            dev->i2c->delay_ms(dev->i2c->ctx, 1);
        if (retry == 199) return ADS1115_ERR_BUSY;
    }

    uint8_t buf[2] = {0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONVERSION, buf, 2))
        return ADS1115_ERR_I2C;

    *raw = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return ADS1115_OK;
}

ads1115_err_t ads1115_read_voltage(ads1115_t *dev, float *voltage_mv)
{
    if (!dev || !voltage_mv)
        return ADS1115_ERR_PARAM;

    int16_t raw;
    ads1115_err_t err = ads1115_read_raw(dev, &raw);
    if (err != ADS1115_OK)
        return err;

    int pga_idx = (int)dev->cfg.pga;
    if (pga_idx < 0) pga_idx = 0;
    if (pga_idx > 5) pga_idx = 5;
    *voltage_mv = (float)raw * pga_scale_mv[pga_idx];
    ENSURE(*voltage_mv == *voltage_mv);  /* not NaN */
    return ADS1115_OK;
}

ads1115_err_t ads1115_set_channel(ads1115_t *dev, ads1115_mux_t mux)
{
    if (!dev)
        return ADS1115_ERR_PARAM;
    dev->cfg.mux = mux;
    return ADS1115_OK;
}
