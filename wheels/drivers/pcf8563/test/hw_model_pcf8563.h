// SPDX-License-Identifier: MIT
/*
 * hw_model_pcf8563.h — datasheet-accurate register model for PCF8563
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with the reset values of Rev. 10 Table 27. The time is undefined after
 * reset, so each test plants what it needs with hw_model_inject_time().
 *
 * The HAL has no delay, so a test moves time with hw_model_tick(). The tick
 * obeys STOP, carries through the calendar and compares the alarm.
 */
#ifndef HW_MODEL_PCF8563_H
#define HW_MODEL_PCF8563_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 16   /* PCF8563 has 16 registers (0x00-0x0F) */

/* Rev. 10 Table 4: "Bit positions labelled with N should always be written
 * with logic 0" */
#define HW_CTRL1_N_BITS 0x57u
#define HW_CTRL2_N_BITS 0xE0u

/* Table 5: AF and TF in Control_status_2 */
#define HW_CTRL2_FLAGS  0x0Cu
#define HW_CTRL2_AF     0x08u

/* Table 4: N bits "could be either logic 0 or logic 1" when read, and x bits
 * "are not relevant". They read 1 here, so a driver that expects 0 or forgets
 * a mask fails. */
static const uint8_t hw_read_as_1[HW_REG_COUNT] = {
    HW_CTRL1_N_BITS, HW_CTRL2_N_BITS,
    0x00u, 0x80u, 0xC0u, 0xC0u, 0xF8u, 0x60u, 0x00u,  /* 02h-08h: time */
    0x00u, 0x40u, 0x40u, 0x78u,                       /* 09h-0Ch: alarm */
    0x7Cu, 0x7Cu, 0x00u,                              /* 0Dh-0Fh */
};

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;
static int     hw_time_writes_running;  /* 02h-08h written with STOP = 0 */
static int     hw_n_bits_written;       /* writes with an N bit at 1 */
static bool    hw_alarm_matching;       /* enabled alarm fields match now */

static void hw_model_reset(void)
{
    /* Table 27. Registers marked x "are undefined at power-up", and read 1
     * here. That leaves VL = 1 and AE_x = 1 (alarm disabled), and time
     * registers that hold no valid BCD. */
    memset(hw_regs, 0xFF, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_time_writes_running = 0;
    hw_n_bits_written = 0;
    hw_alarm_matching = false;

    hw_regs[0x00] = 0x08;  /* Control_status_1: TEST1=0, STOP=0, TESTC=1 */
    hw_regs[0x01] = 0x00;  /* Control_status_2: TI_TP=0, AF=0, TF=0, AIE=0, TIE=0 */
    hw_regs[0x0D] = 0x80;  /* CLKOUT_control: FE=1, FD=00 (32.768 kHz) */
    hw_regs[0x0E] = 0x03;  /* Timer_control: TE=0, TD=11 (1/60 Hz) */
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    if (reg >= HW_REG_COUNT) return true;  /* ignore out of range */
    if (reg >= 0x02u && reg <= 0x08u && !(hw_regs[0x00] & 0x20u)) {
        hw_time_writes_running++;
    }
    if ((reg == 0x00u && (val & HW_CTRL1_N_BITS)) ||
        (reg == 0x01u && (val & HW_CTRL2_N_BITS))) {
        hw_n_bits_written++;
    }
    /* p.9: "To prevent one flag being overwritten while clearing another, a
     * logic AND is performed during a write access." */
    if (reg == 0x01u) {
        uint8_t flags = (uint8_t)(hw_regs[0x01] & val & HW_CTRL2_FLAGS);
        val = (uint8_t)((val & (uint8_t)~HW_CTRL2_FLAGS) | flags);
    }
    hw_regs[reg] = val;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t idx = (uint16_t)(reg + i);
        buf[i] = (idx < HW_REG_COUNT)
                 ? (uint8_t)(hw_regs[idx] | hw_read_as_1[idx]) : 0x00;
    }
    return true;
}

static uint8_t hw_from_bcd(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0Fu));
}

static uint8_t hw_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

/* p.11: "a 29th day to February if the year counter contains a value which
 * is exactly divisible by 4, including the year 00" */
static uint8_t hw_month_days(uint8_t month, uint8_t year)
{
    static const uint8_t days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month == 2u && (year % 4u) == 0u) return 29;
    return (month >= 1u && month <= 12u) ? days[month - 1u] : 31;
}

/* One more second through Figure 8: seconds, minutes, hours, then weekday and
 * days, months and years. VL stays, C toggles when the years wrap (p.12). */
static void hw_count_one_second(void)
{
    uint8_t sec = hw_from_bcd(hw_regs[0x02] & 0x7Fu);
    uint8_t vl  = (uint8_t)(hw_regs[0x02] & 0x80u);

    if (++sec < 60u) {
        hw_regs[0x02] = (uint8_t)(vl | hw_to_bcd(sec));
        return;
    }
    hw_regs[0x02] = vl;

    uint8_t min = hw_from_bcd(hw_regs[0x03] & 0x7Fu);
    if (++min < 60u) {
        hw_regs[0x03] = hw_to_bcd(min);
        return;
    }
    hw_regs[0x03] = 0x00u;

    uint8_t hour = hw_from_bcd(hw_regs[0x04] & 0x3Fu);
    if (++hour < 24u) {
        hw_regs[0x04] = hw_to_bcd(hour);
        return;
    }
    hw_regs[0x04] = 0x00u;

    hw_regs[0x06] = (uint8_t)(((hw_regs[0x06] & 0x07u) + 1u) % 7u);

    uint8_t day   = hw_from_bcd(hw_regs[0x05] & 0x3Fu);
    uint8_t month = hw_from_bcd(hw_regs[0x07] & 0x1Fu);
    uint8_t year  = hw_from_bcd(hw_regs[0x08]);
    uint8_t c     = (uint8_t)(hw_regs[0x07] & 0x80u);

    if (++day <= hw_month_days(month, year)) {
        hw_regs[0x05] = hw_to_bcd(day);
        return;
    }
    hw_regs[0x05] = 0x01u;
    if (++month <= 12u) {
        hw_regs[0x07] = (uint8_t)(c | hw_to_bcd(month));
        return;
    }
    if (++year > 99u) {
        year = 0u;
        c ^= 0x80u;
    }
    hw_regs[0x07] = (uint8_t)(c | 0x01u);
    hw_regs[0x08] = hw_to_bcd(year);
}

/* p.16: fields with AE_x = 0 are compared, and those with AE_x = 1 "are
 * ignored". With every field ignored nothing can match. */
static bool hw_alarm_fields_match(void)
{
    static const struct {
        uint8_t alarm;
        uint8_t time;
        uint8_t mask;
    } fields[4] = {
        { 0x09u, 0x03u, 0x7Fu }, { 0x0Au, 0x04u, 0x3Fu },
        { 0x0Bu, 0x05u, 0x3Fu }, { 0x0Cu, 0x06u, 0x07u },
    };
    bool any = false;

    for (unsigned i = 0; i < 4u; i++) {
        uint8_t alarm = hw_regs[fields[i].alarm];

        if (alarm & 0x80u) continue;
        any = true;
        if ((alarm ^ hw_regs[fields[i].time]) & fields[i].mask) return false;
    }
    return any;
}

/* Let time pass. p.20: with STOP "no 1 Hz ticks will be generated". */
__attribute__((unused))
static void hw_model_tick(uint32_t seconds)
{
    for (; seconds > 0u; seconds--) {
        if (hw_regs[0x00] & 0x20u) return;
        hw_count_one_second();

        /* p.16: AF is set "When all enabled comparisons first match" */
        bool match = hw_alarm_fields_match();
        if (match && !hw_alarm_matching) hw_regs[0x01] |= HW_CTRL2_AF;
        hw_alarm_matching = match;
    }
}

/* Inject a time into the register model (BCD-encodes internally) */
__attribute__((unused))
static void hw_model_inject_time(uint8_t year, uint8_t month, uint8_t day,
                                  uint8_t weekday, uint8_t hour,
                                  uint8_t minute, uint8_t second)
{
    hw_regs[0x02] = (uint8_t)(((second / 10) << 4) | (second % 10));
    hw_regs[0x03] = (uint8_t)(((minute / 10) << 4) | (minute % 10));
    hw_regs[0x04] = (uint8_t)(((hour / 10) << 4) | (hour % 10));
    hw_regs[0x05] = (uint8_t)(((day / 10) << 4) | (day % 10));
    hw_regs[0x06] = weekday;
    hw_regs[0x07] = (uint8_t)(((month / 10) << 4) | (month % 10));
    hw_regs[0x08] = (uint8_t)(((year / 10) << 4) | (year % 10));
}

#endif /* HW_MODEL_PCF8563_H */
