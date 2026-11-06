// SPDX-License-Identifier: MIT
/*
 * ina3221 — TI INA3221 triple-channel current and power monitor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (all 16-bit big-endian):
 *   0x01 CH1 shunt, 0x02 CH1 bus,
 *   0x03 CH2 shunt, 0x04 CH2 bus,
 *   0x05 CH3 shunt, 0x06 CH3 bus,
 *   0xFE Manufacturer ID (0x5449 = "TI"),
 *   0xFF Die ID (0x3220)
 */

#include "ina3221.h"
#include "contract.h"
#include <string.h>

/* ---- register addresses ---- */
#define REG_CH1_SHUNT_V  0x01u
#define REG_CH1_BUS_V    0x02u
#define REG_CH2_SHUNT_V  0x03u
#define REG_CH2_BUS_V    0x04u
#define REG_CH3_SHUNT_V  0x05u
#define REG_CH3_BUS_V    0x06u
#define REG_MANUFACTURER 0xFEu
#define REG_DIE_ID       0xFFu

/* ---- expected identification ---- */
#define MANUFACTURER_ID  0x5449u  /* "TI" */
#define DIE_ID           0x3220u

/* ---- conversion constants ---- */
#define SHUNT_UV_PER_LSB 40.0f   /* 40 uV/LSB */
#define BUS_MV_PER_LSB    8.0f   /* 8 mV/LSB  */

/* ---- register offset tables ---- */
static const uint8_t shunt_regs[INA3221_NUM_CHANNELS] = {
    REG_CH1_SHUNT_V, REG_CH2_SHUNT_V, REG_CH3_SHUNT_V
};
static const uint8_t bus_regs[INA3221_NUM_CHANNELS] = {
    REG_CH1_BUS_V, REG_CH2_BUS_V, REG_CH3_BUS_V
};

/* ---- internal helpers ---- */

static ina3221_err_t read_reg16(const ina3221_t *dev, uint8_t reg,
                                uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2))
        return INA3221_ERR_I2C;
    *val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return INA3221_OK;
}

/* Decode shunt voltage register: 13-bit signed (bits[15:3]), 40 uV/LSB */
static float decode_shunt(uint16_t raw)
{
    int16_t signed_raw = (int16_t)raw;
    signed_raw >>= 3;  /* arithmetic right shift preserves sign */
    return (float)signed_raw * SHUNT_UV_PER_LSB;
}

/* Decode bus voltage register: 13-bit signed (bits[15:3]), 8 mV/LSB */
static float decode_bus(uint16_t raw)
{
    int16_t signed_raw = (int16_t)raw;
    signed_raw >>= 3;
    return (float)signed_raw * BUS_MV_PER_LSB;
}

/* The steps of init that talk to the chip */
static ina3221_err_t bring_up(const ina3221_t *dev)
{
    /* Verify manufacturer ID */
    uint16_t mfr_id;
    ina3221_err_t err = read_reg16(dev, REG_MANUFACTURER, &mfr_id);
    if (err != INA3221_OK)
        return err;
    if (mfr_id != MANUFACTURER_ID)
        return INA3221_ERR_ID;

    /* Verify die ID */
    uint16_t die_id;
    err = read_reg16(dev, REG_DIE_ID, &die_id);
    if (err != INA3221_OK)
        return err;
    if (die_id != DIE_ID)
        return INA3221_ERR_ID;

    return INA3221_OK;
}

/* ---- public API ---- */

ina3221_err_t ina3221_init(ina3221_t *dev,
                           const ina3221_i2c_t *i2c,
                           const ina3221_cfg_t *cfg)
{
    if (!dev)
        return INA3221_ERR_PARAM;

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it, whose chip was never verified */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
        return INA3221_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return INA3221_ERR_PARAM;

    /* Validate shunt resistors: must all be positive */
    for (int ch = 0; ch < INA3221_NUM_CHANNELS; ch++) {
        if (!(cfg->shunt_resistor_ohm[ch] > 0.0f))
            return INA3221_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;

    ina3221_err_t err = bring_up(dev);
    if (err != INA3221_OK)
        dev->i2c = NULL;
    return err;
}

ina3221_err_t ina3221_read_channel(const ina3221_t *dev,
                                   uint8_t channel,
                                   float *shunt_uv,
                                   float *bus_mv)
{
    if (!dev || !dev->i2c || !shunt_uv || !bus_mv)
        return INA3221_ERR_PARAM;
    if (channel >= INA3221_NUM_CHANNELS)
        return INA3221_ERR_PARAM;

    uint16_t raw;
    ina3221_err_t err;

    err = read_reg16(dev, shunt_regs[channel], &raw);
    if (err != INA3221_OK)
        return err;
    *shunt_uv = decode_shunt(raw);

    err = read_reg16(dev, bus_regs[channel], &raw);
    if (err != INA3221_OK)
        return err;
    *bus_mv = decode_bus(raw);

    ENSURE(*shunt_uv == *shunt_uv);  /* not NaN */
    ENSURE(*bus_mv == *bus_mv);

    return INA3221_OK;
}

ina3221_err_t ina3221_read_all(const ina3221_t *dev,
                               ina3221_data_t *out)
{
    if (!dev || !dev->i2c || !out)
        return INA3221_ERR_PARAM;

    memset(out, 0, sizeof(*out));

    for (uint8_t ch = 0; ch < INA3221_NUM_CHANNELS; ch++) {
        ina3221_err_t err = ina3221_read_channel(dev, ch,
                                                 &out->shunt_uv[ch],
                                                 &out->bus_mv[ch]);
        if (err != INA3221_OK)
            return err;

        /* current (mA) = shunt_uv / (shunt_resistor_ohm * 1000) */
        float r_ohm = dev->cfg.shunt_resistor_ohm[ch];
        out->current_ma[ch] = out->shunt_uv[ch] / (r_ohm * 1000.0f);
        ENSURE(out->current_ma[ch] == out->current_ma[ch]);
    }

    return INA3221_OK;
}
