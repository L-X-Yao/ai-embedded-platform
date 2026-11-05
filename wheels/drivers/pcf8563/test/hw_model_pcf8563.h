// SPDX-License-Identifier: MIT
/*
 * hw_model_pcf8563.h — datasheet-accurate register model for PCF8563
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults per NXP PCF8563 datasheet.
 */
#ifndef HW_MODEL_PCF8563_H
#define HW_MODEL_PCF8563_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 16   /* PCF8563 has 16 registers (0x00-0x0F) */

/* Rev. 10 Table 4: N bits "could be either logic 0 or logic 1" when read.
 * Control_status_1 has them at bits 6, 4 and 2-0 (Table 5). They read 1
 * here, so a driver that expects 0 fails. */
#define HW_CTRL1_N_BITS 0x57u

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;
static int     hw_time_writes_running;  /* 02h-08h written with STOP = 0 */

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_time_writes_running = 0;

    /* POR defaults from datasheet:
     * Control1/2 = 0x00
     * Time registers = 0x00 (undefined, but assume cleared)
     * Alarm registers = 0x00 (disabled by default, AE bits clear)
     * CLKOUT = 0x00 (disabled)
     * Timer = 0x00 (disabled)
     */
    hw_regs[0x00] = 0x00;  /* Control_status_1: TEST1=0, STOP=0, TESTC=0 */
    hw_regs[0x01] = 0x00;  /* Control_status_2: TI/TP=0, AF=0, TF=0, AIE=0, TIE=0 */
    /* Set a sensible default time for testing: 2026-03-20 Fri 14:30:00 */
    hw_regs[0x02] = 0x00;  /* seconds: 0 = 0x00 BCD */
    hw_regs[0x03] = 0x30;  /* minutes: 30 = 0x30 BCD */
    hw_regs[0x04] = 0x14;  /* hours: 14 = 0x14 BCD */
    hw_regs[0x05] = 0x20;  /* days: 20 = 0x20 BCD */
    hw_regs[0x06] = 0x05;  /* weekday: 5 = Friday */
    hw_regs[0x07] = 0x03;  /* month: 3 = 0x03 BCD */
    hw_regs[0x08] = 0x26;  /* year: 26 = 0x26 BCD */
    /* Alarm registers disabled */
    hw_regs[0x09] = 0x80;  /* minute alarm: AE=1 (disabled) */
    hw_regs[0x0A] = 0x80;  /* hour alarm: AE=1 (disabled) */
    hw_regs[0x0B] = 0x80;  /* day alarm: AE=1 (disabled) */
    hw_regs[0x0C] = 0x80;  /* weekday alarm: AE=1 (disabled) */
    hw_regs[0x0D] = 0x00;  /* CLKOUT: disabled */
    hw_regs[0x0E] = 0x00;  /* Timer control: disabled */
    hw_regs[0x0F] = 0x00;  /* Timer value: 0 */
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
        buf[i] = (idx < HW_REG_COUNT) ? hw_regs[idx] : 0x00;
        if (idx == 0x00u) buf[i] |= HW_CTRL1_N_BITS;
    }
    return true;
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
