// SPDX-License-Identifier: MIT
/*
 * ina226 — TI INA226 precision I2C current/power monitor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (all 16-bit big-endian):
 *   0x00 Config, 0x01 Shunt Voltage, 0x02 Bus Voltage,
 *   0x03 Power, 0x04 Current, 0x05 Calibration,
 *   0xFE Manufacturer ID, 0xFF Die ID
 */

#include "ina226.h"
#include "contract.h"
#include <string.h>

/* ---- register addresses ---- */
#define REG_CONFIG       0x00u
#define REG_SHUNT_V      0x01u
#define REG_BUS_V        0x02u
#define REG_POWER        0x03u
#define REG_CURRENT      0x04u
#define REG_CALIBRATION  0x05u
#define REG_MASK_ENABLE  0x06u
#define REG_ALERT_LIMIT  0x07u
#define REG_MFR_ID       0xFEu
#define REG_DIE_ID       0xFFu

/* ---- config register bits ---- */
#define CFG_RST          (1u << 15)
/* Averaging mode: bits 11:9 */
/* Bus voltage conv time: bits 8:6 */
/* Shunt voltage conv time: bits 5:3 */
/* Mode: bits 2:0 — continuous shunt + bus = 0b111 */
#define CFG_MODE_CONT_SHUNT_BUS 0x07u

/* ---- INA226 calibration ---- */
/* CAL = 0.00512 / (current_lsb * r_shunt) */
/* Shunt voltage LSB = 2.5 uV */
/* Bus voltage LSB = 1.25 mV */
#define SHUNT_LSB_UV     2.5f
#define BUS_LSB_MV       1.25f

/* ---- internal helpers ---- */

static ina226_err_t write_reg16(const ina226_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFFu) };
    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2))
        return INA226_ERR_I2C;
    return INA226_OK;
}

static ina226_err_t read_reg16(const ina226_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2))
        return INA226_ERR_I2C;
    *val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return INA226_OK;
}

/* ---- public API ---- */

ina226_err_t ina226_init(ina226_t *dev, const ina226_i2c_t *i2c,
                         const ina226_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return INA226_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return INA226_ERR_PARAM;
    if (cfg->shunt_resistance_mohm == 0 || cfg->max_current_ma == 0)
        return INA226_ERR_PARAM;
    if ((unsigned)cfg->averaging > 7 ||
        (unsigned)cfg->bus_conv_time > 7 ||
        (unsigned)cfg->shunt_conv_time > 7)
        return INA226_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Verify manufacturer ID */
    uint16_t mfr_id = 0;
    ina226_err_t err = read_reg16(dev, REG_MFR_ID, &mfr_id);
    if (err != INA226_OK)
        return err;
    if (mfr_id != INA226_MFR_ID)
        return INA226_ERR_ID;

    /* Verify die ID */
    uint16_t die_id = 0;
    err = read_reg16(dev, REG_DIE_ID, &die_id);
    if (err != INA226_OK)
        return err;
    if (die_id != INA226_DIE_ID)
        return INA226_ERR_ID;

    /* Calculate current LSB: max_current_ma / 1000.0 / 2^15 */
    float max_current_a = (float)cfg->max_current_ma / 1000.0f;
    dev->current_lsb = max_current_a / 32768.0f;

    /* Build config register */
    uint16_t config = 0;
    config |= ((uint16_t)cfg->averaging     & 0x07u) << 9;
    config |= ((uint16_t)cfg->bus_conv_time  & 0x07u) << 6;
    config |= ((uint16_t)cfg->shunt_conv_time & 0x07u) << 3;
    config |= CFG_MODE_CONT_SHUNT_BUS;

    err = write_reg16(dev, REG_CONFIG, config);
    if (err != INA226_OK)
        return err;

    /* Calibration register: CAL = 0.00512 / (current_lsb * r_shunt_ohms) */
    float r_shunt_ohms = (float)cfg->shunt_resistance_mohm / 1000.0f;
    float cal_f = 0.00512f / (dev->current_lsb * r_shunt_ohms);
    uint16_t cal = (cal_f >= 65535.0f) ? 65535u : (uint16_t)cal_f;

    err = write_reg16(dev, REG_CALIBRATION, cal);
    if (err != INA226_OK)
        return err;

    INVARIANT(dev->i2c != NULL);
    return INA226_OK;
}

ina226_err_t ina226_read(ina226_t *dev, ina226_data_t *out)
{
    if (!dev || !out)
        return INA226_ERR_PARAM;

    uint16_t raw;

    /* Shunt voltage: signed 16-bit, LSB = 2.5 uV */
    ina226_err_t err = read_reg16(dev, REG_SHUNT_V, &raw);
    if (err != INA226_OK)
        return err;
    out->shunt_voltage_uv = (int32_t)((int16_t)raw) * 25 / 10;

    /* Bus voltage: unsigned 16-bit, LSB = 1.25 mV */
    err = read_reg16(dev, REG_BUS_V, &raw);
    if (err != INA226_OK)
        return err;
    out->bus_voltage_mv = (uint32_t)raw * 125u / 100u;

    /* Current: signed 16-bit, LSB = current_lsb (A) */
    err = read_reg16(dev, REG_CURRENT, &raw);
    if (err != INA226_OK)
        return err;
    out->current_ma = (int32_t)((float)((int16_t)raw) * dev->current_lsb * 1000.0f);

    /* Power: unsigned 16-bit, LSB = 25 * current_lsb (W) */
    err = read_reg16(dev, REG_POWER, &raw);
    if (err != INA226_OK)
        return err;
    out->power_mw = (uint32_t)((float)raw * 25.0f * dev->current_lsb * 1000.0f);

    return INA226_OK;
}

ina226_err_t ina226_reset(ina226_t *dev)
{
    if (!dev)
        return INA226_ERR_PARAM;
    return write_reg16(dev, REG_CONFIG, CFG_RST);
}
