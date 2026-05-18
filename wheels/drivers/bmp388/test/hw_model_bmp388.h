// SPDX-License-Identifier: MIT
/*
 * hw_model_bmp388.h — datasheet-accurate register model for BMP388
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults. Use in test_hw_model_bmp388.c
 * for round-trip verification against the real driver.
 */
#ifndef HW_MODEL_BMP388_H
#define HW_MODEL_BMP388_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;

    /* POR defaults from BMP388 datasheet */
    hw_regs[0x00] = 0x50;  /* CHIP_ID */
    hw_regs[0x03] = 0x20;  /* STATUS: drdy_press set */

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

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    /* CMD register: soft reset clears data, resets status */
    if (reg == 0x7E && val == 0xB6)
    {
        hw_regs[0x03] = 0x20;  /* re-assert drdy after reset */
        return true;
    }

    hw_regs[reg] = val;

    /* PWR_CTRL forced mode: simulate drdy immediately */
    if (reg == 0x1B && (val & 0x30) == 0x10)
    {
        hw_regs[0x03] |= 0x20;  /* drdy_press */
    }

    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw_regs[(uint8_t)(reg + i)];
    }
    return true;
}

/* Inject raw ADC values for a measurement */
__attribute__((unused))
static void hw_model_inject_adc(uint32_t raw_press, uint32_t raw_temp)
{
    hw_regs[0x04] = (uint8_t)(raw_press & 0xFF);
    hw_regs[0x05] = (uint8_t)((raw_press >> 8) & 0xFF);
    hw_regs[0x06] = (uint8_t)((raw_press >> 16) & 0xFF);
    hw_regs[0x07] = (uint8_t)(raw_temp & 0xFF);
    hw_regs[0x08] = (uint8_t)((raw_temp >> 8) & 0xFF);
    hw_regs[0x09] = (uint8_t)((raw_temp >> 16) & 0xFF);
}

#endif /* HW_MODEL_BMP388_H */
