/*
 * hw_model_ds3231.h -- DS3231 hardware register model from datasheet
 *
 * Models DS3231 register behavior:
 * - Time registers 0x00-0x06 in BCD (seconds/minutes/hours/day/date/month/year)
 * - Control at 0x0E, Status at 0x0F
 * - Temperature at 0x11-0x12 (MSB signed + 2-bit fraction)
 * - Hours register bit 6 selects 12/24-hour mode
 * - All time registers are BCD-encoded
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
    hw_model_set_writable_mask(&m->hw, DS3231_REG_MONTH,   0x1F);
    /* Year: full 8-bit BCD writable */

    /* Temperature registers (read-only, written by oscillator) */
    hw_model_set_readonly(&m->hw, DS3231_REG_TEMP_MSB);
    hw_model_set_readonly(&m->hw, DS3231_REG_TEMP_LSB);

    /* Status register: bit 7 (OSF) writable-clear, bit 3 (EN32kHz) writable */
    hw_model_set_writable_mask(&m->hw, DS3231_REG_STATUS, 0x88);

    /* Default time: 2025-01-15 Wednesday 12:30:45 */
    hw_model_set_reset_value(&m->hw, DS3231_REG_SECONDS, ds3231_bin2bcd(45));
    hw_model_set_reset_value(&m->hw, DS3231_REG_MINUTES, ds3231_bin2bcd(30));
    hw_model_set_reset_value(&m->hw, DS3231_REG_HOURS,   ds3231_bin2bcd(12));
    hw_model_set_reset_value(&m->hw, DS3231_REG_DAY,     3);  /* Wednesday */
    hw_model_set_reset_value(&m->hw, DS3231_REG_DATE,    ds3231_bin2bcd(15));
    hw_model_set_reset_value(&m->hw, DS3231_REG_MONTH,   ds3231_bin2bcd(1));
    hw_model_set_reset_value(&m->hw, DS3231_REG_YEAR,    ds3231_bin2bcd(25));

    /* Default temperature: 23.25 C (MSB=23, LSB fraction=01 in bits 7:6) */
    m->hw.reset_values[DS3231_REG_TEMP_MSB] = 23;
    m->hw.reset_values[DS3231_REG_TEMP_LSB] = 0x40;  /* 0b01_000000 = 0.25C */

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
    if (pm) val |= 0x20u; /* bit 5 = PM */
    val |= ds3231_bin2bcd(hour12) & 0x1Fu;
    m->hw.regs[DS3231_REG_HOURS] = val;
}

#endif /* HW_MODEL_DS3231_H */
