// SPDX-License-Identifier: MIT
/*
 * hw_model_ili9341.h — datasheet-accurate model for ILI9341 display controller
 *
 * Simulates command/data protocol via DC pin state tracking.
 * Since ILI9341 is a write-mostly device (commands + pixel data),
 * the model primarily records commands and data for verification.
 */
#ifndef HW_MODEL_ILI9341_H
#define HW_MODEL_ILI9341_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Track commands and their data for verification */
#define HW_CMD_LOG_SIZE 128
#define HW_DATA_LOG_SIZE 2048

static uint8_t hw_cmd_log[HW_CMD_LOG_SIZE];
static int     hw_cmd_log_idx;
static uint8_t hw_data_log[HW_DATA_LOG_SIZE];
static int     hw_data_log_idx;
static int     hw_xfer_count;
static int     hw_cs_state;
static bool    hw_dc_state;
static bool    hw_rst_state;
static uint32_t hw_delay_total;

/* MADCTL value last written */
static uint8_t hw_last_madctl;
/* COLMOD value last written */
static uint8_t hw_last_colmod;
/* Last command received (for tracking data that follows) */
static uint8_t hw_last_cmd;

static void hw_model_reset(void)
{
    memset(hw_cmd_log, 0, sizeof(hw_cmd_log));
    hw_cmd_log_idx  = 0;
    memset(hw_data_log, 0, sizeof(hw_data_log));
    hw_data_log_idx = 0;
    hw_xfer_count   = 0;
    hw_cs_state     = 1;
    hw_dc_state     = true;
    hw_rst_state    = true;
    hw_delay_total  = 0;
    hw_last_madctl  = 0;
    hw_last_colmod  = 0;
    hw_last_cmd     = 0;
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static void hw_model_dc_set(void *ctx, bool level) { (void)ctx; hw_dc_state = level; }

__attribute__((unused))
static void hw_model_rst_set(void *ctx, bool level) { (void)ctx; hw_rst_state = level; }

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms) { (void)ctx; hw_delay_total += ms; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    hw_xfer_count++;

    if (!hw_dc_state) {
        /* Command byte (DC=low) */
        if (hw_cmd_log_idx < HW_CMD_LOG_SIZE) {
            hw_cmd_log[hw_cmd_log_idx++] = tx;
        }
        hw_last_cmd = tx;
    } else {
        /* Data byte (DC=high) */
        if (hw_data_log_idx < HW_DATA_LOG_SIZE) {
            hw_data_log[hw_data_log_idx++] = tx;
        }
        /* Track specific register writes */
        if (hw_last_cmd == 0x36) {
            hw_last_madctl = tx;
        } else if (hw_last_cmd == 0x3A) {
            hw_last_colmod = tx;
        }
    }

    return 0x00;
}

/* Check if a specific command was sent */
__attribute__((unused))
static bool hw_model_cmd_sent(uint8_t cmd)
{
    for (int i = 0; i < hw_cmd_log_idx; i++) {
        if (hw_cmd_log[i] == cmd) return true;
    }
    return false;
}

#endif /* HW_MODEL_ILI9341_H */
