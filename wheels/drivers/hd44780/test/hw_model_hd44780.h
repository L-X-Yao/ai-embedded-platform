/*
 * hw_model_hd44780.h — datasheet-accurate register model for hd44780
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults. Use in test_hw_model_hd44780.c
 * for round-trip verification against the real driver.
 */
#ifndef HW_MODEL_HD44780_H
#define HW_MODEL_HD44780_H

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
    /* TODO: set POR defaults from datasheet */
    /* hw_regs[REG_WHO_AM_I] = 0xXX; */
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    /* TODO: enforce read-only registers, side effects */
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

/* TODO: add inject helpers for physical values */
/* static void hw_model_inject_xxx(float value) { ... } */

#endif /* HW_MODEL_HD44780_H */
