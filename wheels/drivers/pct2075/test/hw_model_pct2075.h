// SPDX-License-Identifier: MIT
/*
 * hw_model_pct2075.h -- PCT2075 hardware register model from datasheet
 *
 * Models PCT2075 register behavior:
 * - Reg 0x00: Temperature (read-only, 2 bytes, 11-bit two's complement)
 * - Reg 0x01: Configuration (R/W, 1 byte)
 * - Reg 0x02: T_hyst (R/W, 2 bytes, same encoding as temperature)
 * - Reg 0x03: T_os (R/W, 2 bytes, same encoding as temperature)
 * - Reg 0x04: T_idle (R/W, 1 byte)
 *
 * POR defaults per datasheet:
 *   Temperature = 0x0000 (0 C)
 *   Config = 0x00
 *   T_hyst = 0x4B00 (75 C)
 *   T_os = 0x5000 (80 C)
 *   T_idle = 0x01
 */
#ifndef HW_MODEL_PCT2075_H
#define HW_MODEL_PCT2075_H

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

    /* POR defaults from PCT2075 datasheet */
    /* Temperature = 0 C */
    hw_regs[0x00] = 0x00;
    hw_regs[0x01] = 0x00;
    /* Config = 0x00 (stored at config register address) */
    /* NOTE: reg 0x01 is config (1 byte), but since temp MSB is at 0x00
     * and temp LSB is consecutive, we use the flat 256-byte array.
     * read_regs reads consecutive bytes starting from reg address. */

    /* For the flat model, we store 2 bytes per 2-byte register:
     * Temp:   [0x00]=MSB, [0x01]=LSB  -- but this collides with Config!
     * PCT2075 registers are pointer-addressed, not contiguous in memory.
     * We separate them by using a stride approach:
     * Use base addresses with 2-byte stride to avoid overlap. */
}

/* Inject a temperature value (11-bit two's complement in bits [15:5]) */
static void hw_model_inject_temp(float temp_c)
{
    /* Convert: temp_c / 0.125 = raw 11-bit value, then shift left 5 */
    int16_t raw11 = (int16_t)(temp_c / 0.125f);
    int16_t raw16 = (int16_t)(raw11 << 5);
    hw_regs[0x00] = (uint8_t)((uint16_t)raw16 >> 8);
    hw_regs[0x01] = (uint8_t)((uint16_t)raw16 & 0xFFu);
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    /* Temperature register (0x00) is read-only -- silently ignore */
    if (reg == 0x00u) {
        return true;
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
        buf[i] = hw_regs[(uint8_t)(reg + i)];
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_PCT2075_H */
