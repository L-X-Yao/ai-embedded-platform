// SPDX-License-Identifier: MIT
/*
 * hw_model_ds3231.h -- DS3231 hardware register model from datasheet
 *
 * Models DS3231 register behavior (Maxim 19-5170 Rev 10):
 * - Time registers 0x00-0x06 in BCD (seconds/minutes/hours/day/date/month/year),
 *   reset to 01/01/00 01 00:00:00 (p10). The clock does not run: tests plant
 *   the time they need
 * - Month bit 7 is the century bit (p12)
 * - Control at 0x0E, reset 0x1C (p13)
 * - Status at 0x0F, reset 0x88 (p14): OSF, A2F and A1F only clear, EN32kHz
 *   is read/write, BSY and bits 6:4 ignore writes
 * - Temperature at 0x11-0x12 (MSB signed + 2-bit fraction), reset 0 (p15)
 * - Hours register bit 6 selects 12/24-hour mode
 * - The address wraps from 12h to 00h (p11)
 */

#ifndef HW_MODEL_DS3231_H
#define HW_MODEL_DS3231_H

#include "hw_model.h"

/* --- DS3231 register addresses --- */
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

/* --- BCD encoding helpers --- */
static uint8_t ds3231_bin2bcd(uint8_t val)
{
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

/* --- simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
} ds3231_model_t;

/* Callback: Status write. A 0 clears OSF, A2F and A1F, a 1 changes nothing. */
static void ds3231_model_on_status(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    ds3231_model_t *m = (ds3231_model_t *)user;
    m->hw.regs[DS3231_REG_STATUS] =
        (uint8_t)((new_val & 0x0C) | (old_val & new_val & 0x83));
}

/* Replaces hw_model_i2c_read_regs for the wrap at the end of the map */
__attribute__((unused))
static bool ds3231_hw_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                      uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    uint8_t addr = reg;

    if (hw_model__should_fail(hw)) return false;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw->regs[addr];
        addr = (addr >= DS3231_REG_TEMP_LSB) ? 0u : (uint8_t)(addr + 1u);
    }
    return true;
}

static void ds3231_model_init(ds3231_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Time registers: 7-bit writable (BCD data) */
    hw_model_set_writable_mask(&m->hw, DS3231_REG_SECONDS, 0x7F);
    hw_model_set_writable_mask(&m->hw, DS3231_REG_MINUTES, 0x7F);
    hw_model_set_writable_mask(&m->hw, DS3231_REG_HOURS,   0x7F);
    hw_model_set_writable_mask(&m->hw, DS3231_REG_DAY,     0x07);
    hw_model_set_writable_mask(&m->hw, DS3231_REG_DATE,    0x3F);
    hw_model_set_writable_mask(&m->hw, DS3231_REG_MONTH,   0x9F);
    /* Year: full 8-bit BCD writable */

    /* Temperature registers (read-only, written by oscillator) */
    hw_model_set_readonly(&m->hw, DS3231_REG_TEMP_MSB);
    hw_model_set_readonly(&m->hw, DS3231_REG_TEMP_LSB);

    /* Status register: OSF, EN32kHz, A2F and A1F take writes */
    hw_model_set_writable_mask(&m->hw, DS3231_REG_STATUS, 0x8B);
    hw_model_set_on_write(&m->hw, DS3231_REG_STATUS, ds3231_model_on_status);

    /* Reset date, 01/01/00 01 00:00:00; the other time registers are 0 */
    hw_model_set_reset_value(&m->hw, DS3231_REG_DAY,   0x01);
    hw_model_set_reset_value(&m->hw, DS3231_REG_DATE,  0x01);
    hw_model_set_reset_value(&m->hw, DS3231_REG_MONTH, 0x01);

    /* INTCN, RS2 and RS1, then OSF and EN32kHz */
    hw_model_set_reset_value(&m->hw, DS3231_REG_CONTROL, 0x1C);
    hw_model_set_reset_value(&m->hw, DS3231_REG_STATUS,  0x88);

    hw_model_reset(&m->hw);
}

/* Helper: set simulated time in the model (BCD encoded) */
static void ds3231_model_set_time(ds3231_model_t *m,
                                  uint8_t year, uint8_t month, uint8_t day,
                                  uint8_t weekday,
                                  uint8_t hour, uint8_t minute, uint8_t second)
{
    m->hw.regs[DS3231_REG_SECONDS] = ds3231_bin2bcd(second);
    m->hw.regs[DS3231_REG_MINUTES] = ds3231_bin2bcd(minute);
    m->hw.regs[DS3231_REG_HOURS]   = ds3231_bin2bcd(hour);
    m->hw.regs[DS3231_REG_DAY]     = weekday;
    m->hw.regs[DS3231_REG_DATE]    = ds3231_bin2bcd(day);
    m->hw.regs[DS3231_REG_MONTH]   = ds3231_bin2bcd(month);
    m->hw.regs[DS3231_REG_YEAR]    = ds3231_bin2bcd(year);
}

/* Helper: set simulated temperature */
static void ds3231_model_set_temperature(ds3231_model_t *m, int8_t integer,
                                         uint8_t frac_quarter)
{
    m->hw.regs[DS3231_REG_TEMP_MSB] = (uint8_t)integer;
    m->hw.regs[DS3231_REG_TEMP_LSB] = (uint8_t)((frac_quarter & 0x03) << 6);
}

/* Helper: set hours register to 12-hour mode for init conversion test */
static void ds3231_model_set_12h_mode(ds3231_model_t *m, uint8_t hour12,
                                      bool pm)
{
    uint8_t val = 0x40u;  /* bit 6 = 12-hour mode */
    if (pm) val = (uint8_t)(val | 0x20u); /* bit 5 = PM */
    val = (uint8_t)(val | (ds3231_bin2bcd(hour12) & 0x1Fu));
    m->hw.regs[DS3231_REG_HOURS] = val;
}

#endif /* HW_MODEL_DS3231_H */
