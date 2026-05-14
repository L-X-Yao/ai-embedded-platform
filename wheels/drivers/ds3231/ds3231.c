// SPDX-License-Identifier: MIT
/*
 * ds3231 — DS3231 high-precision I2C RTC driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * DS3231 registers 0x00-0x06: time in BCD (sec/min/hr/day/date/mon/yr).
 * Registers 0x11-0x12: temperature (MSB + upper 2 bits of LSB, 0.25C).
 * Fixed I2C address 0x68.
 */

#include "ds3231.h"
#include "contract.h"

/* --- register map --- */
#define REG_SECONDS   0x00u
#define REG_MINUTES   0x01u
#define REG_HOURS     0x02u
#define REG_DAY       0x03u
#define REG_DATE      0x04u
#define REG_MONTH     0x05u
#define REG_YEAR      0x06u
#define REG_CONTROL   0x0Eu
#define REG_TEMP_MSB  0x11u
#define REG_TEMP_LSB  0x12u

/* --- BCD helpers --- */
static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0Fu));
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

/* --- I2C wrappers --- */
static bool write_reg(const ds3231_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, DS3231_ADDR, reg, val);
}

static bool read_regs(const ds3231_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, DS3231_ADDR, reg, buf, len);
}

/* --- time validation --- */
static bool time_valid(const ds3231_time_t *t)
{
    return t->second <= 59
        && t->minute <= 59
        && t->hour   <= 23
        && t->day    >= 1 && t->day    <= 31
        && t->month  >= 1 && t->month  <= 12
        && t->year   <= 99
        && t->weekday >= 1 && t->weekday <= 7;
}

/* --- public API --- */

ds3231_err_t ds3231_init(ds3231_t *dev, const ds3231_i2c_t *i2c)
{
    if (!dev || !i2c)
    {
        return DS3231_ERR_PARAM;
    }
    dev->i2c = i2c;

    /* Force 24-hour mode: clear bit 6 of hours register */
    uint8_t hr = 0;
    if (!read_regs(dev, REG_HOURS, &hr, 1))
    {
        return DS3231_ERR_I2C;
    }
    if (hr & 0x40u)
    {
        /* Convert 12-hour to 24-hour and clear bit 6 */
        uint8_t h12 = bcd_to_bin(hr & 0x1Fu);
        bool pm = (hr & 0x20u) != 0;
        if (pm && h12 != 12)
            h12 = (uint8_t)(h12 + 12);
        else if (!pm && h12 == 12)
            h12 = 0;
        if (!write_reg(dev, REG_HOURS, bin_to_bcd(h12)))
        {
            return DS3231_ERR_I2C;
        }
    }

    return DS3231_OK;
}

ds3231_err_t ds3231_get_time(const ds3231_t *dev, ds3231_time_t *time)
{
    if (!dev || !time || !dev->i2c)
    {
        return DS3231_ERR_PARAM;
    }

    uint8_t buf[7];
    if (!read_regs(dev, REG_SECONDS, buf, 7))
    {
        return DS3231_ERR_I2C;
    }

    time->second  = bcd_to_bin(buf[0] & 0x7Fu);
    time->minute  = bcd_to_bin(buf[1] & 0x7Fu);
    time->hour    = bcd_to_bin(buf[2] & 0x3Fu); /* 24-hour mask */
    time->weekday = buf[3] & 0x07u;
    time->day     = bcd_to_bin(buf[4] & 0x3Fu);
    time->month   = bcd_to_bin(buf[5] & 0x1Fu);
    time->year    = bcd_to_bin(buf[6]);

    return DS3231_OK;
}

ds3231_err_t ds3231_set_time(const ds3231_t *dev, const ds3231_time_t *time)
{
    if (!dev || !time || !dev->i2c)
    {
        return DS3231_ERR_PARAM;
    }
    if (!time_valid(time))
    {
        return DS3231_ERR_PARAM;
    }

    if (!write_reg(dev, REG_SECONDS, bin_to_bcd(time->second)))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_MINUTES, bin_to_bcd(time->minute)))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_HOURS,   bin_to_bcd(time->hour)))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_DAY,     time->weekday))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_DATE,    bin_to_bcd(time->day)))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_MONTH,   bin_to_bcd(time->month)))
        return DS3231_ERR_I2C;
    if (!write_reg(dev, REG_YEAR,    bin_to_bcd(time->year)))
        return DS3231_ERR_I2C;

    return DS3231_OK;
}

ds3231_err_t ds3231_get_temperature(const ds3231_t *dev, float *temp_c)
{
    if (!dev || !temp_c || !dev->i2c)
    {
        return DS3231_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP_MSB, buf, 2))
    {
        return DS3231_ERR_I2C;
    }

    /* MSB is signed integer part, upper 2 bits of LSB are 0.25C fraction */
    int8_t msb = (int8_t)buf[0];
    uint8_t frac = (buf[1] >> 6) & 0x03u;
    *temp_c = (float)msb + (float)frac * 0.25f;

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    return DS3231_OK;
}
