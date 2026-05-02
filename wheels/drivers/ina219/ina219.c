// SPDX-License-Identifier: MIT
/*
 * ina219 — TI INA219 I2C current/voltage/power monitor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Register map (all 16-bit big-endian):
 *   0x00 Config, 0x01 Shunt Voltage, 0x02 Bus Voltage,
 *   0x03 Power, 0x04 Current, 0x05 Calibration
 */

#include "ina219.h"
#include <string.h>
#include <math.h>

/* ---- register addresses ---- */
#define REG_CONFIG       0x00u
#define REG_SHUNT_V      0x01u
#define REG_BUS_V        0x02u
#define REG_POWER        0x03u
#define REG_CURRENT      0x04u
#define REG_CALIBRATION  0x05u

/* ---- config register bits ---- */
#define CFG_RST          (1u << 15)
/* BRNG (bus voltage range) at bit 13 */
/* PGA gain at bits 12:11 */
/* BADC (bus ADC resolution) at bits 10:7   — we use 12-bit = 0b0011 */
/* SADC (shunt ADC resolution) at bits 6:3  — we use 12-bit = 0b0011 */
/* MODE at bits 2:0                         — continuous shunt+bus = 0b111 */
#define CFG_BADC_12BIT   (0x03u << 7)
#define CFG_SADC_12BIT   (0x03u << 3)
#define CFG_MODE_CONT    0x07u

/* ---- internal helpers ---- */

static ina219_err_t write_reg16(const ina219_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    if (!dev->i2c->write_regs(dev->i2c->ctx, dev->dev_addr, reg, buf, 2))
        return INA219_ERR_I2C;
    return INA219_OK;
}

static ina219_err_t read_reg16(const ina219_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, dev->dev_addr, reg, buf, 2))
        return INA219_ERR_I2C;
    *val = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return INA219_OK;
}

/* ---- public API ---- */

ina219_err_t ina219_init(ina219_t *dev, const ina219_i2c_t *i2c,
                         uint8_t dev_addr, float shunt_ohms,
                         ina219_bus_range_t range, ina219_gain_t gain)
{
    if (!dev || !i2c)
        return INA219_ERR_PARAM;
    if (!i2c->write_regs || !i2c->read_regs)
        return INA219_ERR_PARAM;
    if (shunt_ohms <= 0.0f)
        return INA219_ERR_PARAM;
    if ((unsigned)range > 1 || (unsigned)gain > 3)
        return INA219_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c       = i2c;
    dev->dev_addr  = dev_addr;
    dev->shunt_ohms = shunt_ohms;

    /* Determine max current from gain setting */
    static const float gain_mv[] = { 40.0f, 80.0f, 160.0f, 320.0f };
    dev->max_current_a = (gain_mv[gain] / 1000.0f) / shunt_ohms;

    /* current_lsb = max_current / 2^15 */
    dev->current_lsb = dev->max_current_a / 32768.0f;

    /* Build config register */
    uint16_t config = 0;
    config |= ((uint16_t)range & 0x01u) << 13;
    config |= ((uint16_t)gain  & 0x03u) << 11;
    config |= CFG_BADC_12BIT;
    config |= CFG_SADC_12BIT;
    config |= CFG_MODE_CONT;

    ina219_err_t err = write_reg16(dev, REG_CONFIG, config);
    if (err != INA219_OK)
        return err;

    /* Calibration register = trunc(0.04096 / (current_lsb * shunt_ohms)) */
    float cal_f = 0.04096f / (dev->current_lsb * shunt_ohms);
    uint16_t cal = (uint16_t)cal_f;

    err = write_reg16(dev, REG_CALIBRATION, cal);
    if (err != INA219_OK)
        return err;

    return INA219_OK;
}

ina219_err_t ina219_read_bus_voltage(const ina219_t *dev, float *voltage_v)
{
    if (!dev || !voltage_v)
        return INA219_ERR_PARAM;

    uint16_t raw;
    ina219_err_t err = read_reg16(dev, REG_BUS_V, &raw);
    if (err != INA219_OK)
        return err;

    /* Bus voltage: bits [15:3], LSB = 4 mV */
    int16_t shifted = (int16_t)(raw >> 3);
    *voltage_v = (float)shifted * 0.004f;
    return INA219_OK;
}

ina219_err_t ina219_read_shunt_voltage(const ina219_t *dev, float *voltage_mv)
{
    if (!dev || !voltage_mv)
        return INA219_ERR_PARAM;

    uint16_t raw;
    ina219_err_t err = read_reg16(dev, REG_SHUNT_V, &raw);
    if (err != INA219_OK)
        return err;

    /* Shunt voltage register is signed 16-bit, LSB = 10 uV = 0.01 mV */
    int16_t signed_raw = (int16_t)raw;
    *voltage_mv = (float)signed_raw * 0.01f;
    return INA219_OK;
}

ina219_err_t ina219_read_current(const ina219_t *dev, float *current_ma)
{
    if (!dev || !current_ma)
        return INA219_ERR_PARAM;

    uint16_t raw;
    ina219_err_t err = read_reg16(dev, REG_CURRENT, &raw);
    if (err != INA219_OK)
        return err;

    /* Current register is signed 16-bit, LSB = current_lsb (A) */
    int16_t signed_raw = (int16_t)raw;
    *current_ma = (float)signed_raw * dev->current_lsb * 1000.0f;
    return INA219_OK;
}

ina219_err_t ina219_read_power(const ina219_t *dev, float *power_mw)
{
    if (!dev || !power_mw)
        return INA219_ERR_PARAM;

    uint16_t raw;
    ina219_err_t err = read_reg16(dev, REG_POWER, &raw);
    if (err != INA219_OK)
        return err;

    /* Power register LSB = 20 * current_lsb (W), convert to mW */
    *power_mw = (float)raw * 20.0f * dev->current_lsb * 1000.0f;
    return INA219_OK;
}

ina219_err_t ina219_reset(ina219_t *dev)
{
    if (!dev)
        return INA219_ERR_PARAM;

    return write_reg16(dev, REG_CONFIG, CFG_RST);
}
