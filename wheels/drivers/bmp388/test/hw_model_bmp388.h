// SPDX-License-Identifier: MIT
/*
 * hw_model_bmp388.h — datasheet-accurate register model for BMP388
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults. Use in test_hw_model_bmp388.c
 * for round-trip verification against the real driver.
 *
 * A forced measurement runs only what press_en and temp_en ask for. Time passes
 * through hw_model_delay_ms, and a little with each read over the bus. Pressure
 * lands first, then temperature, each with its drdy flag, after the time of the
 * manual's formula. Reading a data register clears that flag, and at the end the
 * sensor is back in sleep mode.
 *
 * A soft reset restores the Table 24 defaults and leaves the NVM alone. It ends
 * with por_detected set. cmd_rdy is not modelled: Table 24 gives STATUS as 0x00
 * and the driver does not use it.
 */
#ifndef HW_MODEL_BMP388_H
#define HW_MODEL_BMP388_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256
#define HW_PWR_CTRL_LOG_MAX 8

/* The manual gives no time for a soft reset, so this is its 2 ms start-up time
 * (tstartup, p.8), which is the nearest thing it has */
#define HW_RESET_US 2000u

/* One register read at 400 kHz is about 40 clocks */
#define HW_BUS_READ_US 100u

static uint8_t  hw_regs[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;
static uint32_t hw_adc_press;        /* the next pressure measurement */
static uint32_t hw_adc_temp;         /* the next temperature measurement */
static int      hw_press_measured;
static int      hw_temp_measured;
static uint32_t hw_now_us;
static uint32_t hw_press_done_us;
static uint32_t hw_temp_done_us;
static bool     hw_press_busy;
static bool     hw_temp_busy;
static uint32_t hw_reset_done_us;
static bool     hw_resetting;
static bool     hw_reset_stuck;      /* the reset never finishes */
static uint32_t hw_reset_us;         /* how long a reset takes */
static uint8_t  hw_pwr_ctrl_log[HW_PWR_CTRL_LOG_MAX];  /* every byte written */
static int      hw_pwr_ctrl_len;

/* Table 24, p.30 and p.31: the default of every register below the NVM */
static void hw_model_defaults(void)
{
    memset(&hw_regs[0x01], 0, 0x1Fu);
    hw_regs[0x06] = 0x80;  /* DATA_2 */
    hw_regs[0x09] = 0x80;  /* DATA_5 */
    hw_regs[0x15] = 0x01;  /* FIFO_WTM_0 */
    hw_regs[0x17] = 0x02;  /* FIFO_CONFIG_1 */
    hw_regs[0x18] = 0x02;  /* FIFO_CONFIG_2 */
    hw_regs[0x19] = 0x02;  /* INT_CTRL */
    hw_regs[0x1C] = 0x02;  /* OSR */
}

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_adc_press = 0;
    hw_adc_temp  = 0;
    hw_press_measured = 0;
    hw_temp_measured  = 0;
    hw_now_us = 0;
    hw_press_busy = false;
    hw_temp_busy = false;
    hw_resetting = false;
    hw_reset_stuck = false;
    hw_reset_us = HW_RESET_US;
    hw_pwr_ctrl_len = 0;

    /* POR defaults from BMP388 datasheet, Table 24. STATUS starts at 0x00. */
    hw_regs[0x00] = 0x50;  /* CHIP_ID */
    hw_model_defaults();
    hw_regs[0x10] = 0x01;  /* EVENT: por_detected */

    /* Calibration NVM (same realistic values as test_bmp388.c) */
    /* NVM_PAR_T1 = 26000 LE */
    hw_regs[0x31] = 0x90; hw_regs[0x32] = 0x65;
    /* NVM_PAR_T2 = 18000 LE */
    hw_regs[0x33] = 0x50; hw_regs[0x34] = 0x46;
    /* NVM_PAR_T3 = -10 */
    hw_regs[0x35] = 0xF6;
    /* NVM_PAR_P1 = -4000 LE */
    hw_regs[0x36] = 0x60; hw_regs[0x37] = 0xF0;
    /* NVM_PAR_P2 = -6000 LE */
    hw_regs[0x38] = 0x90; hw_regs[0x39] = 0xE8;
    /* NVM_PAR_P3 = 10 */
    hw_regs[0x3A] = 0x0A;
    /* NVM_PAR_P4 = 0 */
    hw_regs[0x3B] = 0x00;
    /* NVM_PAR_P5 = 25000 LE */
    hw_regs[0x3C] = 0xA8; hw_regs[0x3D] = 0x61;
    /* NVM_PAR_P6 = 28000 LE */
    hw_regs[0x3E] = 0x60; hw_regs[0x3F] = 0x6D;
    /* NVM_PAR_P7 = 15 */
    hw_regs[0x40] = 0x0F;
    /* NVM_PAR_P8 = -10 */
    hw_regs[0x41] = 0xF6;
    /* NVM_PAR_P9 = 1000 LE */
    hw_regs[0x42] = 0xE8; hw_regs[0x43] = 0x03;
    /* NVM_PAR_P10 = 10 */
    hw_regs[0x44] = 0x0A;
    /* NVM_PAR_P11 = 0 */
    hw_regs[0x45] = 0x00;
}

static void hw_model_land_press(void)
{
    hw_regs[0x04] = (uint8_t)(hw_adc_press & 0xFF);
    hw_regs[0x05] = (uint8_t)((hw_adc_press >> 8) & 0xFF);
    hw_regs[0x06] = (uint8_t)((hw_adc_press >> 16) & 0xFF);
    hw_regs[0x03] |= 0x20;  /* drdy_press */
}

static void hw_model_land_temp(void)
{
    hw_regs[0x07] = (uint8_t)(hw_adc_temp & 0xFF);
    hw_regs[0x08] = (uint8_t)((hw_adc_temp >> 8) & 0xFF);
    hw_regs[0x09] = (uint8_t)((hw_adc_temp >> 16) & 0xFF);
    hw_regs[0x03] |= 0x40;  /* drdy_temp */
}

/* Manual p.13, Figure 5: pressure, then temperature, "skip if press_en = 0", "skip
 * if temp_en = 0". p.27: Tconv = 234 us + press_en * (392 us + 2^osr_p * 2000 us)
 * + temp_en * (313 us + 2^osr_t * 2000 us). A skipped measurement leaves its data
 * registers and its flag alone. */
static void hw_model_measure(uint8_t pwr_ctrl)
{
    uint32_t at = hw_now_us + 234u;

    if (pwr_ctrl & 0x01)
    {
        hw_press_measured++;
        at += 392u + (2000u << (hw_regs[0x1C] & 0x07u));
        hw_press_done_us = at;
        hw_press_busy = true;
    }
    if (pwr_ctrl & 0x02)
    {
        hw_temp_measured++;
        at += 313u + (2000u << ((hw_regs[0x1C] >> 3) & 0x07u));
        hw_temp_done_us = at;
        hw_temp_busy = true;
    }
}

static void hw_model_advance_us(uint32_t us)
{
    bool measuring = hw_press_busy || hw_temp_busy;

    hw_now_us += us;
    if (hw_resetting && !hw_reset_stuck && hw_now_us >= hw_reset_done_us)
    {
        hw_resetting = false;
        hw_regs[0x10] |= 0x01;  /* por_detected */
    }
    if (hw_press_busy && hw_now_us >= hw_press_done_us)
    {
        hw_press_busy = false;
        hw_model_land_press();
    }
    if (hw_temp_busy && hw_now_us >= hw_temp_done_us)
    {
        hw_temp_busy = false;
        hw_model_land_temp();
    }
    /* p.11: "When the measurement is finished, the sensor returns to sleep mode" */
    if (measuring && !hw_press_busy && !hw_temp_busy)
    {
        hw_regs[0x1B] &= 0x03u;
    }
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    /* A write that lands during the reset is overwritten by it */
    if (hw_resetting)
    {
        return true;
    }

    /* CMD, p.39: "Register will always read as 0x00". softreset: "Triggers a
     * reset, all user configuration settings are overwritten with their default
     * state" */
    if (reg == 0x7E)
    {
        if (val == 0xB6)
        {
            hw_model_defaults();
            hw_press_busy = false;
            hw_temp_busy = false;
            hw_resetting = true;
            hw_reset_done_us = hw_now_us + hw_reset_us;
        }
        return true;
    }

    /* Table 24: below 0x15 and at the NVM the registers are read only, and 0x1E
     * is reserved */
    if (reg < 0x15 || reg == 0x1E || reg > 0x1F)
    {
        return true;
    }

    hw_regs[reg] = val;

    /* PWR_CTRL: mode 01 and 10 are forced mode */
    if (reg == 0x1B)
    {
        if (hw_pwr_ctrl_len < HW_PWR_CTRL_LOG_MAX)
        {
            hw_pwr_ctrl_log[hw_pwr_ctrl_len++] = val;
        }
        if ((val & 0x30) == 0x10 || (val & 0x30) == 0x20)
        {
            hw_model_measure(val);
        }
    }

    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    hw_model_advance_us(HW_BUS_READ_US);

    for (uint16_t i = 0; i < len; i++) {
        uint8_t addr = (uint8_t)(reg + i);
        buf[i] = hw_regs[addr];
        /* Manual p.32: a flag "gets reset, when one ... DATA register is read out" */
        if (addr >= 0x04 && addr <= 0x06)
        {
            hw_regs[0x03] &= (uint8_t)~0x20u;
        }
        if (addr >= 0x07 && addr <= 0x09)
        {
            hw_regs[0x03] &= (uint8_t)~0x40u;
        }
        /* p.33: por_detected is "Clear-on-read", and INT_STATUS "is cleared after
         * reading". p.31: ERR_REG cmd_err and conf_err are "Cleared on read". */
        if (addr == 0x10 || addr == 0x11)
        {
            hw_regs[addr] = 0;
        }
        if (addr == 0x02)
        {
            hw_regs[0x02] &= 0x01u;
        }
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    hw_model_advance_us(ms * 1000u);
}

/* Inject raw ADC values for a measurement */
__attribute__((unused))
static void hw_model_inject_adc(uint32_t raw_press, uint32_t raw_temp)
{
    hw_adc_press = raw_press;
    hw_adc_temp  = raw_temp;
}

#endif /* HW_MODEL_BMP388_H */
