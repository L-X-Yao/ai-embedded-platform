// SPDX-License-Identifier: MIT
/*
 * hw_model_ili9341.h — model of the ILI9341 display controller's command side
 *
 * Simulates command/data protocol via DC pin state tracking.
 * Since ILI9341 is a write-mostly device (commands + pixel data),
 * the model primarily records commands and data for verification.
 *
 * From ILI9341 V1.11, page numbers being those of the PDF:
 * - Bytes count only with CSX low (p.33)
 * - The delay callback is the time base, for the waits after RESX, SWRESET and
 *   SLPOUT (p.90, p.101, p.230). A command inside one is counted, not refused.
 * - Sleep and display state, and MADCTL and COLMOD with their reset values
 *
 * Not modelled: frame memory, and the window and pixel order it would need.
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

/* Bytes clocked with CSX high, which the chip never saw */
static int     hw_dropped_bytes;

/* Time from the delay callback, and when each wait ends */
static uint32_t hw_now_ms;
static uint32_t hw_rst_fell_ms;
static uint32_t hw_cmd_ok_ms;      /* any command */
static uint32_t hw_slpout_ok_ms;   /* SLPOUT */
static uint32_t hw_slpout_end_ms;  /* the Sleep Out sequence, for SWRESET */
/* Commands sent inside a wait or with RESX low */
static int     hw_early_cmds;
/* RESX pulses with no wait at all, and the ones that reset the chip */
static int     hw_short_resets;
static int     hw_reset_count;

/* p.101: "Sleep IN Mode" and p.108: "Display OFF" after every kind of reset */
static bool    hw_sleep_out;
static bool    hw_display_on;

#define HW_CMD_WAIT_MS     5u
#define HW_SLEEP_WAIT_MS   120u
/* COLMOD: DPI and DBI both 3'b110 at power-on (p.134) */
#define HW_COLMOD_RESET    0x66u

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
    hw_last_colmod  = HW_COLMOD_RESET;
    hw_last_cmd     = 0;
    hw_dropped_bytes = 0;
    hw_now_ms       = 0;
    hw_rst_fell_ms  = 0;
    hw_cmd_ok_ms    = 0;
    hw_slpout_ok_ms = 0;
    hw_slpout_end_ms = 0;
    hw_early_cmds   = 0;
    hw_short_resets = 0;
    hw_reset_count  = 0;
    hw_sleep_out    = false;
    hw_display_on   = false;
}

/* RESX rose after a real pulse. p.128 and p.134: MADCTL and COLMOD back to
 * their defaults. p.230 note 7: "It is necessary to wait 5msec after releasing
 * RESX before sending commands. Also Sleep Out command cannot be sent for
 * 120msec." */
static void hw_chip_reset(void)
{
    hw_last_madctl  = 0;
    hw_last_colmod  = HW_COLMOD_RESET;
    hw_sleep_out    = false;
    hw_display_on   = false;
    hw_cmd_ok_ms    = hw_now_ms + HW_CMD_WAIT_MS;
    hw_slpout_ok_ms = hw_now_ms + HW_SLEEP_WAIT_MS;
    hw_slpout_end_ms = 0;
    hw_reset_count++;
}

/* What a command byte does, and whether it came too soon */
static void hw_on_command(uint8_t cmd)
{
    bool early = !hw_rst_state || hw_now_ms < hw_cmd_ok_ms;

    switch (cmd) {
    case 0x01:  /* SWRESET */
        /* p.90: "Software Reset Command cannot be sent during Sleep Out
         * sequence." */
        if (hw_now_ms < hw_slpout_end_ms) early = true;
        /* p.90: "wait 5msec before sending new command", and "If Software
         * Reset is applied during Sleep Out mode, it will be necessary to wait
         * 120msec before sending Sleep out command." MADCTL and COLMOD have
         * "No change" (p.128, p.134). */
        hw_cmd_ok_ms = hw_now_ms + HW_CMD_WAIT_MS;
        if (hw_sleep_out) hw_slpout_ok_ms = hw_now_ms + HW_SLEEP_WAIT_MS;
        hw_sleep_out  = false;
        hw_display_on = false;
        break;
    case 0x11:  /* SLPOUT */
        if (hw_now_ms < hw_slpout_ok_ms) early = true;
        /* p.101: "wait 5msec before sending next command", and "It takes
         * 120msec to become Sleep Out mode" */
        hw_cmd_ok_ms     = hw_now_ms + HW_CMD_WAIT_MS;
        hw_slpout_end_ms = hw_now_ms + HW_SLEEP_WAIT_MS;
        hw_sleep_out     = true;
        break;
    case 0x28:  /* DISPOFF */
        hw_display_on = false;
        break;
    case 0x29:  /* DISPON */
        hw_display_on = true;
        break;
    default:
        break;
    }

    if (early) hw_early_cmds++;
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static void hw_model_dc_set(void *ctx, bool level) { (void)ctx; hw_dc_state = level; }

__attribute__((unused))
static void hw_model_rst_set(void *ctx, bool level)
{
    (void)ctx;
    if (!level && hw_rst_state) {
        hw_rst_fell_ms = hw_now_ms;
    } else if (level && !hw_rst_state) {
        /* p.230: tRW is 10 us at least, and a pulse "Shorter than 5us" is
         * "Reset Rejected". Time here is in ms, so only no wait at all shows. */
        if (hw_now_ms == hw_rst_fell_ms) hw_short_resets++;
        else hw_chip_reset();
    }
    hw_rst_state = level;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    hw_delay_total += ms;
    hw_now_ms += ms;
}

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    /* p.33: "The serial interface is initialized when CSX is high status. In
     * this state, SCL clock pulse and SDA data are no effect." */
    if (hw_cs_state) {
        hw_dropped_bytes++;
        return 0x00;
    }

    hw_xfer_count++;

    if (!hw_dc_state) {
        /* Command byte (DC=low) */
        if (hw_cmd_log_idx < HW_CMD_LOG_SIZE) {
            hw_cmd_log[hw_cmd_log_idx++] = tx;
        }
        hw_last_cmd = tx;
        hw_on_command(tx);
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
