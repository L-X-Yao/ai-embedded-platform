// SPDX-License-Identifier: MIT
/*
 * hw_model_hd44780.h — PCF8574 + HD44780 hardware model
 *
 * Tracks the byte sequence sent through the I2C expander for
 * verification of the 4-bit LCD protocol.
 */
#ifndef HW_MODEL_HD44780_H
#define HW_MODEL_HD44780_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_WRITE_LOG_SIZE 512

static uint8_t hw_write_log[HW_WRITE_LOG_SIZE];
static int     hw_write_count;
static uint32_t hw_delay_count;

static void hw_model_reset(void)
{
    memset(hw_write_log, 0, sizeof(hw_write_log));
    hw_write_count = 0;
    hw_delay_count = 0;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr; (void)val;
    if (hw_write_count < HW_WRITE_LOG_SIZE)
        hw_write_log[hw_write_count++] = reg;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    memset(buf, 0, len);
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    hw_delay_count++;
}

#endif /* HW_MODEL_HD44780_H */
