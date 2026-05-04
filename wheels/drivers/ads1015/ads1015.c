// SPDX-License-Identifier: MIT
/*
 * ads1015 — TI ADS1015 12-bit 4-channel ADC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register layout identical to ADS1115: config at 0x01, conversion at
 * 0x00.  12-bit result is left-justified in the 16-bit conversion
 * register (bits [15:4]).  We right-shift by 4 to recover the signed
 * 12-bit code (-2048 .. +2047).
 */
#include "ads1015.h"
#include "contract.h"

#define REG_CONVERSION 0x00u
#define REG_CONFIG     0x01u

/* millivolts per LSB for each PGA setting (FSR / 2048) */
static const float pga_scale_mv[] = {
    6144.0f / 2048.0f,   /* PGA_6144: 3.0 mV/LSB */
    4096.0f / 2048.0f,   /* PGA_4096: 2.0 mV/LSB */
    2048.0f / 2048.0f,   /* PGA_2048: 1.0 mV/LSB */
    1024.0f / 2048.0f,   /* PGA_1024: 0.5 mV/LSB */
     512.0f / 2048.0f,   /* PGA_0512: 0.25 mV/LSB */
     256.0f / 2048.0f,   /* PGA_0256: 0.125 mV/LSB */
};

/*
 * Build 16-bit config word.
 * Bit layout (MSB first):
 *   [15]    OS      — start single-shot
 *   [14:12] MUX     — channel / differential pair
 *   [11:9]  PGA     — gain
 *   [8]     MODE    — 1 = single-shot
 *   [7:5]   DR      — data rate
 *   [4:0]   comparator defaults (0x03 = disable)
 */
static uint16_t build_config(const ads1015_cfg_t *cfg, ads1015_mux_t mux,
                             bool start)
{
    uint16_t c = 0;
    if (start)
        c |= (1u << 15);
    c |= ((uint16_t)mux & 0x07u) << 12;
    c |= ((uint16_t)cfg->pga & 0x07u) << 9;
    c |= (1u << 8);                        /* single-shot mode */
    c |= ((uint16_t)cfg->rate & 0x07u) << 5;
    c |= 0x0003u;                           /* disable comparator */
    return c;
}

static ads1015_err_t write_config(ads1015_t *dev, uint16_t config)
{
    uint8_t buf[2] = {(uint8_t)(config >> 8), (uint8_t)(config & 0xFFu)};
    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONFIG,
                             buf, 2))
        return ADS1015_ERR_I2C;
    return ADS1015_OK;
}

static ads1015_err_t poll_ready(ads1015_t *dev)
{
    uint8_t cfg_buf[2];
    for (int retry = 0; retry < 200; retry++) {
        if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONFIG,
                                cfg_buf, 2))
            return ADS1015_ERR_I2C;
        if (cfg_buf[0] & 0x80u)
            return ADS1015_OK;
    }
    return ADS1015_ERR_BUSY;
}

/* ---- public API ---- */

ads1015_err_t ads1015_init(ads1015_t *dev, const ads1015_i2c_t *i2c,
                           const ads1015_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return ADS1015_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return ADS1015_ERR_PARAM;
    if ((unsigned)cfg->pga > 5u || (unsigned)cfg->rate > 6u)
        return ADS1015_ERR_PARAM;
    dev->i2c = i2c;
    dev->cfg = *cfg;
    return ADS1015_OK;
}

ads1015_err_t ads1015_read_raw(ads1015_t *dev, ads1015_mux_t channel,
                               int16_t *raw)
{
    if (!dev || !raw)
        return ADS1015_ERR_PARAM;
    if ((unsigned)channel > 7u)
        return ADS1015_ERR_PARAM;

    uint16_t config = build_config(&dev->cfg, channel, true);
    ads1015_err_t err = write_config(dev, config);
    if (err != ADS1015_OK)
        return err;

    err = poll_ready(dev);
    if (err != ADS1015_OK)
        return err;

    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, REG_CONVERSION,
                            buf, 2))
        return ADS1015_ERR_I2C;

    /* 12-bit result is left-justified: shift right 4 to sign-extend */
    *raw = (int16_t)((uint16_t)buf[0] << 8 | buf[1]) >> 4;
    return ADS1015_OK;
}

ads1015_err_t ads1015_read_single(ads1015_t *dev, ads1015_mux_t channel,
                                  float *voltage_mv)
{
    if (!dev || !voltage_mv)
        return ADS1015_ERR_PARAM;

    int16_t raw;
    ads1015_err_t err = ads1015_read_raw(dev, channel, &raw);
    if (err != ADS1015_OK)
        return err;

    unsigned idx = (unsigned)dev->cfg.pga;
    if (idx > 5u) idx = 5u;
    *voltage_mv = (float)raw * pga_scale_mv[idx];
    ENSURE(*voltage_mv == *voltage_mv);  /* not NaN */
    return ADS1015_OK;
}

ads1015_err_t ads1015_set_pga(ads1015_t *dev, ads1015_pga_t pga)
{
    if (!dev)
        return ADS1015_ERR_PARAM;
    if ((unsigned)pga > 5u)
        return ADS1015_ERR_PARAM;
    dev->cfg.pga = pga;
    return ADS1015_OK;
}

ads1015_err_t ads1015_set_rate(ads1015_t *dev, ads1015_rate_t rate)
{
    if (!dev)
        return ADS1015_ERR_PARAM;
    if ((unsigned)rate > 6u)
        return ADS1015_ERR_PARAM;
    dev->cfg.rate = rate;
    return ADS1015_OK;
}
