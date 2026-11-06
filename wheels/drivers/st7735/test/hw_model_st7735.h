// SPDX-License-Identifier: MIT
/*
 * hw_model_st7735.h -- ST7735 SPI display hardware model from datasheet
 *
 * Models Sitronix ST7735 behavior (datasheet V2.1, 2010-02-01):
 * - SPI command/data interface (DC pin: 0=command, 1=data)
 * - Internal POR state: sleep-in, display off
 * - Command sequence tracking: SLPOUT, COLMOD, MADCTL, NORON, DISPON
 * - CASET/RASET (0x2A/0x2B): column/row address window
 * - RAMWR (0x2C): pixel data write tracking
 * - Reset tracking, with the values of the reset table (p.69)
 * - Bytes clocked with CSX high are lost (p.30)
 * - Time, which only the delay callback lets pass. A command inside a wait
 *   the manual asks for is refused and counted. The manual does not say what
 *   the chip does with one, so refusing it makes an early one show.
 */

#ifndef HW_MODEL_ST7735_H
#define HW_MODEL_ST7735_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../st7735.h"

#define ST7735_MODEL_PIXEL_BUF_SIZE  2048

/* Waits before the next command */
#define ST7735_MODEL_RESX_CMD_MS     5u    /* p.72 note 7 */
#define ST7735_MODEL_RESX_SLPOUT_MS  120u  /* p.72 note 7 */
#define ST7735_MODEL_SWRESET_MS      120u  /* p.81 */
#define ST7735_MODEL_SLPOUT_MS       120u  /* p.92 */

#define ST7735_MODEL_MADCTL_MV       0x20u

typedef struct {
    /* Power / display state */
    bool     sleep;
    bool     display_on;
    bool     normal_mode;

    /* MADCTL / COLMOD */
    uint8_t  madctl;
    uint8_t  colmod;

    /* Window */
    uint16_t col_start;
    uint16_t col_end;
    uint16_t row_start;
    uint16_t row_end;

    /* Pixel data tracking */
    uint8_t  pixel_buf[ST7735_MODEL_PIXEL_BUF_SIZE];
    int      pixel_buf_pos;

    /* Command stream tracking */
    int      cmd_count;
    int      data_byte_count;
    uint8_t  last_cmd;

    /* Multi-byte data parser */
    uint8_t  pending_cmd;
    uint8_t  arg_buf[4];
    int      arg_pos;
    int      arg_expected;

    /* Pin-level tracking */
    bool     dc_level;       /* false=command, true=data */
    bool     cs_level;       /* false=selected, true=deselected */
    int      delay_total_ms;
    int      delay_call_count;
    int      xfer_count;
    int      reset_count;
    int      early_cmd_count;  /* commands refused inside a wait */

    /* Time */
    uint32_t now_ms;
    uint32_t cmd_ok_ms;        /* no command before this time */
    uint32_t slpout_ok_ms;     /* no SLPOUT before this time */
} st7735_model_t;

static st7735_model_t st7735_mdl;

static void st7735_model_init(void)
{
    memset(&st7735_mdl, 0, sizeof(st7735_mdl));
    st7735_mdl.sleep       = true;
    st7735_mdl.display_on  = false;
    st7735_mdl.normal_mode = true;
    st7735_mdl.madctl      = 0x00;
    st7735_mdl.colmod      = 0x06;  /* POR: 18-bit */
    st7735_mdl.col_start   = 0;
    st7735_mdl.col_end     = 127;
    st7735_mdl.row_start   = 0;
    st7735_mdl.row_end     = 159;
    st7735_mdl.dc_level    = true;
    st7735_mdl.cs_level    = true;
}

/* What both resets do (p.69). Frame memory is "No Change". */
static void st7735_model_common_reset(void)
{
    bool mv = (st7735_mdl.madctl & ST7735_MODEL_MADCTL_MV) != 0;

    st7735_mdl.sleep       = true;
    st7735_mdl.display_on  = false;
    st7735_mdl.normal_mode = true;
    st7735_mdl.col_start   = 0;
    st7735_mdl.col_end     = mv ? 159 : 127;
    st7735_mdl.row_start   = 0;
    st7735_mdl.row_end     = mv ? 127 : 159;
    st7735_mdl.pending_cmd = 0;
    st7735_mdl.arg_pos     = 0;
}

/* p.69: MADCTL and the pixel format are "No Change" after a S/W reset */
static void st7735_model_sw_reset(void)
{
    st7735_model_common_reset();
    st7735_mdl.cmd_ok_ms = st7735_mdl.now_ms + ST7735_MODEL_SWRESET_MS;
}

/* p.69: a H/W reset also restores MADCTL and 18-bit pixels */
static void st7735_model_hw_reset(void)
{
    st7735_mdl.madctl = 0x00;
    st7735_mdl.colmod = 0x06;
    st7735_model_common_reset();
    st7735_mdl.cmd_ok_ms    = st7735_mdl.now_ms + ST7735_MODEL_RESX_CMD_MS;
    st7735_mdl.slpout_ok_ms = st7735_mdl.now_ms + ST7735_MODEL_RESX_SLPOUT_MS;
}

static bool st7735_model_too_early(uint8_t cmd)
{
    if (st7735_mdl.now_ms < st7735_mdl.cmd_ok_ms) return true;
    return cmd == 0x11 && st7735_mdl.now_ms < st7735_mdl.slpout_ok_ms;
}

static void st7735_model_process_cmd(uint8_t cmd)
{
    st7735_mdl.cmd_count++;
    st7735_mdl.last_cmd = cmd;
    st7735_mdl.pending_cmd  = 0;
    st7735_mdl.arg_pos      = 0;
    st7735_mdl.arg_expected = 0;

    if (st7735_model_too_early(cmd)) {
        st7735_mdl.early_cmd_count++;
        return;
    }

    switch (cmd) {
    case 0x01: /* SWRESET */
        st7735_model_sw_reset();
        break;
    case 0x11: /* SLPOUT */
        st7735_mdl.sleep = false;
        st7735_mdl.cmd_ok_ms = st7735_mdl.now_ms + ST7735_MODEL_SLPOUT_MS;
        break;
    case 0x13: /* NORON */
        st7735_mdl.normal_mode = true;
        break;
    case 0x29: /* DISPON */
        st7735_mdl.display_on = true;
        break;
    case 0x28: /* DISPOFF */
        st7735_mdl.display_on = false;
        break;
    case 0x2A: /* CASET */
        st7735_mdl.pending_cmd  = cmd;
        st7735_mdl.arg_expected = 4;
        break;
    case 0x2B: /* RASET */
        st7735_mdl.pending_cmd  = cmd;
        st7735_mdl.arg_expected = 4;
        break;
    case 0x2C: /* RAMWR */
        st7735_mdl.pending_cmd  = cmd;
        st7735_mdl.arg_expected = 0; /* unlimited */
        st7735_mdl.pixel_buf_pos = 0;
        break;
    case 0x36: /* MADCTL */
        st7735_mdl.pending_cmd  = cmd;
        st7735_mdl.arg_expected = 1;
        break;
    case 0x3A: /* COLMOD */
        st7735_mdl.pending_cmd  = cmd;
        st7735_mdl.arg_expected = 1;
        break;
    default:
        break;
    }
}

static void st7735_model_process_data(uint8_t byte)
{
    st7735_mdl.data_byte_count++;
    if (st7735_mdl.pending_cmd == 0) return;

    switch (st7735_mdl.pending_cmd) {
    case 0x2A: /* CASET */
        if (st7735_mdl.arg_pos < 4) {
            st7735_mdl.arg_buf[st7735_mdl.arg_pos++] = byte;
        }
        if (st7735_mdl.arg_pos == 4) {
            st7735_mdl.col_start = (uint16_t)((uint16_t)st7735_mdl.arg_buf[0] << 8 |
                                               st7735_mdl.arg_buf[1]);
            st7735_mdl.col_end   = (uint16_t)((uint16_t)st7735_mdl.arg_buf[2] << 8 |
                                               st7735_mdl.arg_buf[3]);
            st7735_mdl.pending_cmd = 0;
        }
        break;
    case 0x2B: /* RASET */
        if (st7735_mdl.arg_pos < 4) {
            st7735_mdl.arg_buf[st7735_mdl.arg_pos++] = byte;
        }
        if (st7735_mdl.arg_pos == 4) {
            st7735_mdl.row_start = (uint16_t)((uint16_t)st7735_mdl.arg_buf[0] << 8 |
                                               st7735_mdl.arg_buf[1]);
            st7735_mdl.row_end   = (uint16_t)((uint16_t)st7735_mdl.arg_buf[2] << 8 |
                                               st7735_mdl.arg_buf[3]);
            st7735_mdl.pending_cmd = 0;
        }
        break;
    case 0x2C: /* RAMWR */
        if (st7735_mdl.pixel_buf_pos < ST7735_MODEL_PIXEL_BUF_SIZE) {
            st7735_mdl.pixel_buf[st7735_mdl.pixel_buf_pos++] = byte;
        }
        break;
    case 0x36: /* MADCTL */
        st7735_mdl.madctl = byte;
        st7735_mdl.pending_cmd = 0;
        break;
    case 0x3A: /* COLMOD */
        st7735_mdl.colmod = byte;
        st7735_mdl.pending_cmd = 0;
        break;
    default:
        st7735_mdl.arg_pos++;
        if (st7735_mdl.arg_expected > 0 && st7735_mdl.arg_pos >= st7735_mdl.arg_expected) {
            st7735_mdl.pending_cmd = 0;
        }
        break;
    }
}

/* ---------- SPI HAL callbacks ---------- */

__attribute__((unused))
static bool st7735_model_cs_select(void *ctx)
{
    (void)ctx;
    st7735_mdl.cs_level = false;
    return true;
}

__attribute__((unused))
static void st7735_model_cs_deselect(void *ctx)
{
    (void)ctx;
    st7735_mdl.cs_level = true;
}

__attribute__((unused))
static bool st7735_model_transfer(void *ctx, const uint8_t *tx,
                                   uint8_t *rx, uint16_t len)
{
    (void)ctx;
    st7735_mdl.xfer_count++;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t byte = tx ? tx[i] : 0;
        if (rx) rx[i] = 0xFF;
        /* p.30: "When CSX is "high", SCL clock is ignored." */
        if (st7735_mdl.cs_level) continue;
        if (!st7735_mdl.dc_level) {
            st7735_model_process_cmd(byte);
        } else {
            st7735_model_process_data(byte);
        }
    }
    return true;
}

__attribute__((unused))
static void st7735_model_dc_command(void *ctx)
{
    (void)ctx;
    st7735_mdl.dc_level = false;
}

__attribute__((unused))
static void st7735_model_dc_data(void *ctx)
{
    (void)ctx;
    st7735_mdl.dc_level = true;
}

__attribute__((unused))
static void st7735_model_reset(void *ctx)
{
    (void)ctx;
    st7735_mdl.reset_count++;
    st7735_model_hw_reset();
}

__attribute__((unused))
static void st7735_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    st7735_mdl.now_ms += ms;
    st7735_mdl.delay_total_ms += (int)ms;
    st7735_mdl.delay_call_count++;
}

__attribute__((unused))
static st7735_spi_t st7735_model_spi(void)
{
    st7735_spi_t spi;
    spi.ctx         = NULL;
    spi.cs_select   = st7735_model_cs_select;
    spi.cs_deselect = st7735_model_cs_deselect;
    spi.transfer    = st7735_model_transfer;
    spi.dc_command  = st7735_model_dc_command;
    spi.dc_data     = st7735_model_dc_data;
    spi.reset       = st7735_model_reset;
    spi.delay_ms    = st7735_model_delay_ms;
    return spi;
}

#endif /* HW_MODEL_ST7735_H */
