// SPDX-License-Identifier: MIT
/*
 * hw_model_sh1106.h — display model for SH1106 OLED
 *
 * Tracks commands sent to the display controller for verification.
 * SH1106 is write-only from the host perspective (no read-back in
 * normal operation), so this model records the command sequence.
 */
#ifndef HW_MODEL_SH1106_H
#define HW_MODEL_SH1106_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_CMD_LOG_SIZE 2048

static uint8_t hw_cmd_log[HW_CMD_LOG_SIZE];
static int     hw_cmd_count;
static uint8_t hw_last_ctrl;  /* last control byte (0x00=cmd, 0x40=data) */
static uint8_t hw_last_val;
static int     hw_data_count;

static void hw_model_reset(void)
{
    memset(hw_cmd_log, 0, sizeof(hw_cmd_log));
    hw_cmd_count  = 0;
    hw_last_ctrl  = 0;
    hw_last_val   = 0;
    hw_data_count = 0;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_last_ctrl = reg;
    hw_last_val  = val;
    if (reg == 0x00) {
        /* Command byte */
        if (hw_cmd_count < HW_CMD_LOG_SIZE)
            hw_cmd_log[hw_cmd_count++] = val;
    } else if (reg == 0x40) {
        /* Data byte */
        hw_data_count++;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    /* SH1106 doesn't have readable registers in typical use */
    memset(buf, 0xFF, len);
    return true;
}

#endif /* HW_MODEL_SH1106_H */
