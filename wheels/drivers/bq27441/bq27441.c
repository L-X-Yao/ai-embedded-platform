// SPDX-License-Identifier: MIT
/*
 * bq27441 — TI BQ27441-G1 I2C smart fuel gauge driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (all 16-bit little-endian):
 *   0x00 Control, 0x02 Temperature, 0x04 Voltage, 0x06 Flags,
 *   0x0C RemainingCapacity, 0x0E FullChargeCapacity,
 *   0x10 AverageCurrent, 0x18 AveragePower, 0x1C StateOfCharge,
 *   0x3C DesignCapacity (extended data, config mode only)
 *
 * Control sub-commands are written as 16-bit LE to reg 0x00,
 * then read back from reg 0x00.
 */

#include "bq27441.h"
#include <string.h>

/* ---- standard register addresses ---- */
#define REG_CONTROL          0x00u
#define REG_TEMPERATURE      0x02u
#define REG_VOLTAGE          0x04u
#define REG_FLAGS            0x06u
#define REG_REMAINING_CAP    0x0Cu
#define REG_FULL_CHARGE_CAP  0x0Eu
#define REG_AVG_CURRENT      0x10u
#define REG_AVG_POWER        0x18u
#define REG_SOC              0x1Cu

/* ---- extended data (config mode) ---- */
#define REG_DESIGN_CAPACITY  0x3Cu

/* ---- control sub-commands ---- */
#define CTRL_DEVICE_TYPE     0x0001u
#define CTRL_SEALED          0x0020u
#define CTRL_SET_CFGUPDATE   0x0013u
#define CTRL_SOFT_RESET      0x0042u

/* ---- unseal keys (BQ27441-G1 default) ---- */
#define UNSEAL_KEY_A         0x8000u
#define UNSEAL_KEY_B         0x8000u

/* ---- flags register bits ---- */
#define FLAG_CFGUPMODE       (1u << 4)

/* ---- timing ---- */
#define CFGUPDATE_POLL_MS    10u
#define CFGUPDATE_MAX_POLLS  100u  /* 1 second total */

/* ---- internal helpers ---- */

static bq27441_err_t write_reg16(const bq27441_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val & 0xFFu), (uint8_t)(val >> 8) };
    if (!dev->i2c->write_regs(dev->i2c->ctx, BQ27441_DEV_ADDR, reg, buf, 2))
        return BQ27441_ERR_I2C;
    return BQ27441_OK;
}

static bq27441_err_t read_reg16(const bq27441_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    if (!dev->i2c->read_regs(dev->i2c->ctx, BQ27441_DEV_ADDR, reg, buf, 2))
        return BQ27441_ERR_I2C;
    *val = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    return BQ27441_OK;
}

static bq27441_err_t write_control(const bq27441_t *dev, uint16_t subcmd)
{
    return write_reg16(dev, REG_CONTROL, subcmd);
}

static bq27441_err_t read_control(const bq27441_t *dev, uint16_t *val)
{
    return read_reg16(dev, REG_CONTROL, val);
}

static bq27441_err_t read_flags(const bq27441_t *dev, uint16_t *flags)
{
    return read_reg16(dev, REG_FLAGS, flags);
}

static void do_delay(const bq27441_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

/* ---- public API ---- */

bq27441_err_t bq27441_init(bq27441_t *dev, const bq27441_i2c_t *i2c)
{
    if (!dev || !i2c)
        return BQ27441_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_regs)
        return BQ27441_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* Read device type: write DEVICE_TYPE sub-command, then read back */
    bq27441_err_t err = write_control(dev, CTRL_DEVICE_TYPE);
    if (err != BQ27441_OK)
        return err;

    uint16_t dev_type = 0;
    err = read_control(dev, &dev_type);
    if (err != BQ27441_OK)
        return err;

    if (dev_type != BQ27441_DEVICE_TYPE)
        return BQ27441_ERR_ID;

    return BQ27441_OK;
}

bq27441_err_t bq27441_read(bq27441_t *dev, bq27441_data_t *out)
{
    if (!dev || !out)
        return BQ27441_ERR_PARAM;

    bq27441_err_t err;
    uint16_t raw;

    /* Voltage (mV) */
    err = read_reg16(dev, REG_VOLTAGE, &raw);
    if (err != BQ27441_OK) return err;
    out->voltage_mv = raw;

    /* Average current (mA, signed) */
    err = read_reg16(dev, REG_AVG_CURRENT, &raw);
    if (err != BQ27441_OK) return err;
    out->current_ma = (int16_t)raw;

    /* State of charge (%) */
    err = read_reg16(dev, REG_SOC, &raw);
    if (err != BQ27441_OK) return err;
    out->soc_pct = raw;

    /* Temperature: BQ27441 reports in 0.1 K, convert to 0.1 degC */
    err = read_reg16(dev, REG_TEMPERATURE, &raw);
    if (err != BQ27441_OK) return err;
    out->temperature_01c = (int16_t)((int32_t)raw - 2731);

    /* Remaining capacity (mAh) */
    err = read_reg16(dev, REG_REMAINING_CAP, &raw);
    if (err != BQ27441_OK) return err;
    out->remaining_mah = raw;

    /* Full charge capacity (mAh) */
    err = read_reg16(dev, REG_FULL_CHARGE_CAP, &raw);
    if (err != BQ27441_OK) return err;
    out->full_charge_mah = raw;

    /* Average power (mW, signed) */
    err = read_reg16(dev, REG_AVG_POWER, &raw);
    if (err != BQ27441_OK) return err;
    out->avg_power_mw = (int16_t)raw;

    return BQ27441_OK;
}

bq27441_err_t bq27441_read_voltage(bq27441_t *dev, uint16_t *voltage_mv)
{
    if (!dev || !voltage_mv)
        return BQ27441_ERR_PARAM;
    return read_reg16(dev, REG_VOLTAGE, voltage_mv);
}

bq27441_err_t bq27441_read_soc(bq27441_t *dev, uint16_t *soc_pct)
{
    if (!dev || !soc_pct)
        return BQ27441_ERR_PARAM;
    return read_reg16(dev, REG_SOC, soc_pct);
}

bq27441_err_t bq27441_read_current(bq27441_t *dev, int16_t *current_ma)
{
    if (!dev || !current_ma)
        return BQ27441_ERR_PARAM;

    uint16_t raw;
    bq27441_err_t err = read_reg16(dev, REG_AVG_CURRENT, &raw);
    if (err != BQ27441_OK) return err;
    *current_ma = (int16_t)raw;
    return BQ27441_OK;
}

bq27441_err_t bq27441_read_temperature(bq27441_t *dev, int16_t *temperature_01c)
{
    if (!dev || !temperature_01c)
        return BQ27441_ERR_PARAM;

    uint16_t raw;
    bq27441_err_t err = read_reg16(dev, REG_TEMPERATURE, &raw);
    if (err != BQ27441_OK) return err;
    /* BQ27441 reports 0.1 K, convert to 0.1 degC */
    *temperature_01c = (int16_t)((int32_t)raw - 2731);
    return BQ27441_OK;
}

bq27441_err_t bq27441_enter_config_mode(bq27441_t *dev)
{
    if (!dev)
        return BQ27441_ERR_PARAM;

    /* Unseal first */
    bq27441_err_t err = bq27441_unseal(dev);
    if (err != BQ27441_OK)
        return err;

    /* Send SET_CFGUPDATE sub-command */
    err = write_control(dev, CTRL_SET_CFGUPDATE);
    if (err != BQ27441_OK)
        return err;

    /* Poll FLAGS.CFGUPMODE until set */
    for (uint32_t i = 0; i < CFGUPDATE_MAX_POLLS; i++) {
        do_delay(dev, CFGUPDATE_POLL_MS);
        uint16_t flags = 0;
        err = read_flags(dev, &flags);
        if (err != BQ27441_OK)
            return err;
        if (flags & FLAG_CFGUPMODE)
            return BQ27441_OK;
    }

    return BQ27441_ERR_TIMEOUT;
}

bq27441_err_t bq27441_exit_config_mode(bq27441_t *dev)
{
    if (!dev)
        return BQ27441_ERR_PARAM;

    /* Soft reset to exit config mode */
    bq27441_err_t err = write_control(dev, CTRL_SOFT_RESET);
    if (err != BQ27441_OK)
        return err;

    /* Poll FLAGS.CFGUPMODE until cleared */
    for (uint32_t i = 0; i < CFGUPDATE_MAX_POLLS; i++) {
        do_delay(dev, CFGUPDATE_POLL_MS);
        uint16_t flags = 0;
        err = read_flags(dev, &flags);
        if (err != BQ27441_OK)
            return err;
        if (!(flags & FLAG_CFGUPMODE)) {
            /* Reseal after exiting config */
            return bq27441_seal(dev);
        }
    }

    return BQ27441_ERR_TIMEOUT;
}

bq27441_err_t bq27441_set_design_capacity(bq27441_t *dev, uint16_t mah)
{
    if (!dev)
        return BQ27441_ERR_PARAM;
    if (mah == 0)
        return BQ27441_ERR_PARAM;

    return write_reg16(dev, REG_DESIGN_CAPACITY, mah);
}

bq27441_err_t bq27441_unseal(bq27441_t *dev)
{
    if (!dev)
        return BQ27441_ERR_PARAM;

    /* Write unseal key A then key B to CONTROL */
    bq27441_err_t err = write_control(dev, UNSEAL_KEY_A);
    if (err != BQ27441_OK)
        return err;
    return write_control(dev, UNSEAL_KEY_B);
}

bq27441_err_t bq27441_seal(bq27441_t *dev)
{
    if (!dev)
        return BQ27441_ERR_PARAM;

    return write_control(dev, CTRL_SEALED);
}
