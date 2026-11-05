// SPDX-License-Identifier: MIT
/*
 * pcf8563 — NXP PCF8563 I2C real-time clock driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (NXP PCF8563 datasheet):
 *   0x00-0x01: Control/status
 *   0x02-0x08: Time (sec/min/hr/day/weekday/mon/year) in BCD
 *   0x09-0x0C: Alarm (min/hr/day/weekday)
 *   0x0D:      CLKOUT control
 *   0x0E-0x0F: Timer control + timer value
 */

#include "pcf8563.h"
#include "contract.h"

/* --- register map --- */
#define REG_CONTROL1     0x00u
#define REG_CONTROL2     0x01u
#define REG_SECONDS      0x02u
#define REG_MINUTES      0x03u
#define REG_HOURS        0x04u
#define REG_DAYS         0x05u
#define REG_WEEKDAYS     0x06u
#define REG_MONTHS       0x07u
#define REG_YEARS        0x08u
#define REG_ALARM_MIN    0x09u
#define REG_ALARM_HOUR   0x0Au
#define REG_ALARM_DAY    0x0Bu
#define REG_ALARM_WDAY   0x0Cu
#define REG_CLKOUT       0x0Du
#define REG_TIMER_CTRL   0x0Eu
#define REG_TIMER_VAL    0x0Fu

/* --- bit masks --- */
#define VL_BIT           0x80u   /* voltage-low in seconds register */
#define CLKOUT_FE_BIT    0x80u   /* CLKOUT enable (active high) */
#define TIMER_TE_BIT     0x80u   /* timer enable */
#define ALARM_DISABLE    0x80u   /* alarm disable bit (AE_x) */
#define CTRL1_TEST1_STOP 0xA0u   /* TEST1 and STOP in Control_status_1 */

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
static bool write_reg(const pcf8563_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, PCF8563_DEV_ADDR, reg, val);
}

static bool read_regs(const pcf8563_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, PCF8563_DEV_ADDR, reg, buf, len);
}

/* --- time validation --- */
static bool time_valid(const pcf8563_time_t *t)
{
    return t->second <= 59
        && t->minute <= 59
        && t->hour   <= 23
        && t->day    >= 1 && t->day    <= 31
        && t->month  >= 1 && t->month  <= 12
        && t->year   <= 99
        && t->weekday <= 6;
}

/* --- public API --- */

pcf8563_err_t pcf8563_init(pcf8563_t *dev,
                               const pcf8563_i2c_t *i2c)
{
    if (!dev || !i2c) return PCF8563_ERR_PARAM;
    if (!i2c->read_regs || !i2c->write_reg) return PCF8563_ERR_PARAM;
    dev->i2c = i2c;

    /* Clear control registers to known state:
     * Control1 = 0x00 (normal mode, no test, no stop)
     * Control2 = 0x00 (no alarm/timer interrupts) */
    if (!write_reg(dev, REG_CONTROL1, 0x00))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_CONTROL2, 0x00))
        return PCF8563_ERR_I2C;

    /* Verify communication by reading back control1 */
    uint8_t ctrl1 = 0xFF;
    if (!read_regs(dev, REG_CONTROL1, &ctrl1, 1))
        return PCF8563_ERR_I2C;

    /* init just wrote TEST1 = STOP = 0. If either reads back 1,
     * communication failed. Bit 6 is an N bit and may read either way. */
    if (ctrl1 & CTRL1_TEST1_STOP)
        return PCF8563_ERR_ID;

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_read_time(const pcf8563_t *dev,
                                    pcf8563_time_t *time)
{
    if (!dev || !time || !dev->i2c)
        return PCF8563_ERR_PARAM;

    uint8_t buf[7];
    if (!read_regs(dev, REG_SECONDS, buf, 7))
        return PCF8563_ERR_I2C;

    time->second  = bcd_to_bin(buf[0] & 0x7Fu);  /* mask VL bit */
    time->minute  = bcd_to_bin(buf[1] & 0x7Fu);
    time->hour    = bcd_to_bin(buf[2] & 0x3Fu);
    time->day     = bcd_to_bin(buf[3] & 0x3Fu);
    time->weekday = buf[4] & 0x07u;
    time->month   = bcd_to_bin(buf[5] & 0x1Fu);
    time->year    = bcd_to_bin(buf[6]);

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_write_time(const pcf8563_t *dev,
                                     const pcf8563_time_t *time)
{
    if (!dev || !time || !dev->i2c)
        return PCF8563_ERR_PARAM;
    if (!time_valid(time))
        return PCF8563_ERR_PARAM;

    if (!write_reg(dev, REG_SECONDS,  bin_to_bcd(time->second)))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_MINUTES,  bin_to_bcd(time->minute)))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_HOURS,    bin_to_bcd(time->hour)))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_DAYS,     bin_to_bcd(time->day)))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_WEEKDAYS, time->weekday))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_MONTHS,   bin_to_bcd(time->month)))
        return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_YEARS,    bin_to_bcd(time->year)))
        return PCF8563_ERR_I2C;

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_read_alarm(const pcf8563_t *dev,
                                     pcf8563_alarm_t *alarm)
{
    if (!dev || !alarm || !dev->i2c)
        return PCF8563_ERR_PARAM;

    uint8_t buf[4];
    if (!read_regs(dev, REG_ALARM_MIN, buf, 4))
        return PCF8563_ERR_I2C;

    /* If AE bit (bit 7) is set, alarm field is disabled */
    alarm->minute  = (buf[0] & ALARM_DISABLE)
                     ? ALARM_DISABLE
                     : bcd_to_bin(buf[0] & 0x7Fu);
    alarm->hour    = (buf[1] & ALARM_DISABLE)
                     ? ALARM_DISABLE
                     : bcd_to_bin(buf[1] & 0x3Fu);
    alarm->day     = (buf[2] & ALARM_DISABLE)
                     ? ALARM_DISABLE
                     : bcd_to_bin(buf[2] & 0x3Fu);
    alarm->weekday = (buf[3] & ALARM_DISABLE)
                     ? ALARM_DISABLE
                     : (buf[3] & 0x07u);

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_write_alarm(const pcf8563_t *dev,
                                      const pcf8563_alarm_t *alarm)
{
    if (!dev || !alarm || !dev->i2c)
        return PCF8563_ERR_PARAM;

    /* Validate non-disabled fields */
    if (!(alarm->minute & ALARM_DISABLE) && alarm->minute > 59)
        return PCF8563_ERR_PARAM;
    if (!(alarm->hour & ALARM_DISABLE) && alarm->hour > 23)
        return PCF8563_ERR_PARAM;
    if (!(alarm->day & ALARM_DISABLE) && (alarm->day < 1 || alarm->day > 31))
        return PCF8563_ERR_PARAM;
    if (!(alarm->weekday & ALARM_DISABLE) && alarm->weekday > 6)
        return PCF8563_ERR_PARAM;

    uint8_t min_reg  = (alarm->minute & ALARM_DISABLE)
                       ? ALARM_DISABLE
                       : bin_to_bcd(alarm->minute);
    uint8_t hour_reg = (alarm->hour & ALARM_DISABLE)
                       ? ALARM_DISABLE
                       : bin_to_bcd(alarm->hour);
    uint8_t day_reg  = (alarm->day & ALARM_DISABLE)
                       ? ALARM_DISABLE
                       : bin_to_bcd(alarm->day);
    uint8_t wday_reg = (alarm->weekday & ALARM_DISABLE)
                       ? ALARM_DISABLE
                       : alarm->weekday;

    if (!write_reg(dev, REG_ALARM_MIN,  min_reg))  return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_ALARM_HOUR, hour_reg)) return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_ALARM_DAY,  day_reg))  return PCF8563_ERR_I2C;
    if (!write_reg(dev, REG_ALARM_WDAY, wday_reg)) return PCF8563_ERR_I2C;

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_enable_timer(const pcf8563_t *dev,
                                       pcf8563_timer_clk_t clk,
                                       uint8_t countdown)
{
    if (!dev || !dev->i2c)
        return PCF8563_ERR_PARAM;
    if ((unsigned)clk > 3)
        return PCF8563_ERR_PARAM;

    /* Write timer value first, then enable */
    if (!write_reg(dev, REG_TIMER_VAL, countdown))
        return PCF8563_ERR_I2C;
    /* Timer control: TE=1 (bit 7) | TD[1:0] = clk source */
    if (!write_reg(dev, REG_TIMER_CTRL,
                   (uint8_t)(TIMER_TE_BIT | ((uint8_t)clk & 0x03u))))
        return PCF8563_ERR_I2C;

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_disable_timer(const pcf8563_t *dev)
{
    if (!dev || !dev->i2c)
        return PCF8563_ERR_PARAM;

    /* TE = 0, and TD = 1/60 Hz, the setting the manual wants when unused */
    if (!write_reg(dev, REG_TIMER_CTRL, (uint8_t)PCF8563_TIMER_1_60HZ))
        return PCF8563_ERR_I2C;

    return PCF8563_OK;
}

pcf8563_err_t pcf8563_set_clkout(const pcf8563_t *dev,
                                     bool enable,
                                     pcf8563_clkout_t freq)
{
    if (!dev || !dev->i2c)
        return PCF8563_ERR_PARAM;
    if ((unsigned)freq > 3)
        return PCF8563_ERR_PARAM;

    uint8_t val;
    if (enable) {
        val = (uint8_t)(CLKOUT_FE_BIT | ((uint8_t)freq & 0x03u));
    } else {
        val = 0x00;
    }

    if (!write_reg(dev, REG_CLKOUT, val))
        return PCF8563_ERR_I2C;

    return PCF8563_OK;
}
