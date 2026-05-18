// SPDX-License-Identifier: MIT
/*
 * hw_model_lcd_st7789.h -- ST7789 SPI display hardware model from datasheet
 *
 * Models Sitronix ST7789V/ST7789VW behavior (datasheet rev 1.6):
 * - SPI command/data interface (DC pin: 0=command, 1=data)
 * - Internal power-on-reset state: sleep-in, display off, no inversion
 * - Command sequence validation: SLPOUT before DISPON (datasheet 6.2)
 * - MADCTL (0x36): rotation / mirror / color order
 * - COLMOD (0x3A): pixel format (RGB565 / RGB666)
 * - CASET/RASET (0x2A/0x2B): column/row address window (4-byte BE params)
 * - RAMWR (0x2C): pixel data write tracking
 * - INVON/INVOFF (0x21/0x20): display inversion
 * - DISPON/DISPOFF (0x29/0x28): display enable
 * - SLPIN/SLPOUT (0x10/0x11): sleep mode
 * - SWRESET (0x01): software reset to POR defaults
 *
 * The model provides SPI HAL callbacks (cs/dc/rst/xfer/delay) that the
 * driver injects, so the full driver init / draw path runs against this
 * model and the resulting internal state can be asserted in tests.
 */

#ifndef HW_MODEL_LCD_ST7789_H
#define HW_MODEL_LCD_ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../lcd_st7789.h"

/* --- Capacity for pixel-data tracking --- */
#define ST7789_MODEL_PIXEL_BUF_SIZE  2048

/* --- ST7789 internal state model --- */

typedef struct {
    /* Power / display state (POR defaults per datasheet section 6.2) */
    bool     sleep;              /* true = sleep-in (POR default) */
    bool     display_on;         /* DISPON / DISPOFF */
    bool     inversion;          /* INVON / INVOFF (POR: off) */

    /* MADCTL (0x36) */
    uint8_t  madctl;             /* raw MADCTL byte */

    /* COLMOD (0x3A) */
    uint8_t  colmod;             /* POR: 0x06 per datasheet (18-bit) */

    /* Column / row address window (CASET / RASET) */
    uint16_t col_start;
    uint16_t col_end;
    uint16_t row_start;
    uint16_t row_end;

    /* Pixel data tracking */
    uint8_t  pixel_buf[ST7789_MODEL_PIXEL_BUF_SIZE];
    int      pixel_buf_pos;      /* bytes written after last RAMWR */

    /* Command stream tracking */
    int      cmd_count;          /* total command bytes sent */
    int      data_byte_count;    /* total data bytes sent */
    uint8_t  last_cmd;           /* most recent command byte */

    /* Multi-byte data argument parser state */
    uint8_t  pending_cmd;        /* command awaiting data arguments */
    uint8_t  arg_buf[4];         /* argument accumulator */
    int      arg_pos;            /* bytes received so far */
    int      arg_expected;       /* bytes expected for this command */

    /* Sequence violation counters */
    int      seq_violations;     /* e.g. DISPON while still in sleep */

    /* Pin-level tracking (mirrors driver calls) */
    bool     dc_level;           /* false=command, true=data */
    bool     cs_level;           /* false=asserted(low), true=deasserted */
    bool     rst_level;          /* false=reset, true=normal */
    int      delay_total_ms;
    int      delay_call_count;
    int      xfer_count;
    int      rst_toggle_count;   /* how many times RST went low */
} lcd_st7789_model_t;

/* ================================================================
 * Model lifecycle
 * ================================================================ */

static void lcd_st7789_model_init(lcd_st7789_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* POR defaults (datasheet section 6.2, table of default states) */
    m->sleep       = true;       /* sleep-in after reset */
    m->display_on  = false;
    m->inversion   = false;
    m->madctl      = 0x00;
    m->colmod      = 0x06;       /* 18-bit (RGB666) */
    m->col_start   = 0x0000;
    m->col_end     = 0x00EF;     /* 239 */
    m->row_start   = 0x0000;
    m->row_end     = 0x013F;     /* 319 */
    m->dc_level    = true;
    m->cs_level    = true;
    m->rst_level   = true;
}

/* Software reset: returns most state to POR */
static void lcd_st7789_model_sw_reset(lcd_st7789_model_t *m)
{
    m->sleep       = true;
    m->display_on  = false;
    m->inversion   = false;
    m->madctl      = 0x00;
    m->colmod      = 0x06;
    m->col_start   = 0x0000;
    m->col_end     = 0x00EF;
    m->row_start   = 0x0000;
    m->row_end     = 0x013F;
    m->pending_cmd = 0;
    m->arg_pos     = 0;
    m->pixel_buf_pos = 0;
}

/* ================================================================
 * Command processor -- called when DC=low (command mode)
 * ================================================================ */

static void st7789_model_process_cmd(lcd_st7789_model_t *m, uint8_t cmd)
{
    m->cmd_count++;
    m->last_cmd = cmd;

    /* Any new command cancels a pending multi-byte command */
    m->pending_cmd   = 0;
    m->arg_pos       = 0;
    m->arg_expected  = 0;

    switch (cmd) {
    /* --- NOP --- */
    case 0x00: /* NOP */
        break;

    /* --- SWRESET --- */
    case 0x01:
        lcd_st7789_model_sw_reset(m);
        break;

    /* --- Sleep --- */
    case 0x10: /* SLPIN */
        m->sleep = true;
        break;
    case 0x11: /* SLPOUT */
        m->sleep = false;
        break;

    /* --- Inversion --- */
    case 0x20: /* INVOFF */
        m->inversion = false;
        break;
    case 0x21: /* INVON */
        m->inversion = true;
        break;

    /* --- Display on/off --- */
    case 0x28: /* DISPOFF */
        m->display_on = false;
        break;
    case 0x29: /* DISPON */
        if (m->sleep) {
            m->seq_violations++;  /* datasheet: must SLPOUT first */
        }
        m->display_on = true;
        break;

    /* --- Multi-byte commands (data follows in data mode) --- */
    case 0x2A: /* CASET: 4 data bytes */
        m->pending_cmd  = cmd;
        m->arg_expected = 4;
        break;
    case 0x2B: /* RASET: 4 data bytes */
        m->pending_cmd  = cmd;
        m->arg_expected = 4;
        break;
    case 0x2C: /* RAMWR: start pixel data */
        m->pending_cmd  = cmd;
        m->arg_expected = 0;   /* unlimited */
        m->pixel_buf_pos = 0;
        break;
    case 0x36: /* MADCTL: 1 data byte */
        m->pending_cmd  = cmd;
        m->arg_expected = 1;
        break;
    case 0x3A: /* COLMOD: 1 data byte */
        m->pending_cmd  = cmd;
        m->arg_expected = 1;
        break;

    default:
        break;
    }
}

/* ================================================================
 * Data processor -- called when DC=high (data mode)
 * ================================================================ */

static void st7789_model_process_data(lcd_st7789_model_t *m, uint8_t byte)
{
    m->data_byte_count++;

    if (m->pending_cmd == 0) {
        return;  /* data without a pending command -- ignore */
    }

    switch (m->pending_cmd) {
    case 0x2A: /* CASET */
        if (m->arg_pos < 4) {
            m->arg_buf[m->arg_pos++] = byte;
        }
        if (m->arg_pos == 4) {
            m->col_start = (uint16_t)((uint16_t)m->arg_buf[0] << 8 | m->arg_buf[1]);
            m->col_end   = (uint16_t)((uint16_t)m->arg_buf[2] << 8 | m->arg_buf[3]);
            m->pending_cmd = 0;
        }
        break;

    case 0x2B: /* RASET */
        if (m->arg_pos < 4) {
            m->arg_buf[m->arg_pos++] = byte;
        }
        if (m->arg_pos == 4) {
            m->row_start = (uint16_t)((uint16_t)m->arg_buf[0] << 8 | m->arg_buf[1]);
            m->row_end   = (uint16_t)((uint16_t)m->arg_buf[2] << 8 | m->arg_buf[3]);
            m->pending_cmd = 0;
        }
        break;

    case 0x2C: /* RAMWR -- accumulate pixel data */
        if (m->pixel_buf_pos < ST7789_MODEL_PIXEL_BUF_SIZE) {
            m->pixel_buf[m->pixel_buf_pos++] = byte;
        }
        /* pending_cmd stays 0x2C until a new command is issued */
        break;

    case 0x36: /* MADCTL */
        m->madctl = byte;
        m->pending_cmd = 0;
        break;

    case 0x3A: /* COLMOD */
        m->colmod = byte;
        m->pending_cmd = 0;
        break;

    default:
        /* Unknown pending command argument -- just consume */
        m->arg_pos++;
        if (m->arg_expected > 0 && m->arg_pos >= m->arg_expected) {
            m->pending_cmd = 0;
        }
        break;
    }
}

/* ================================================================
 * SPI HAL callbacks (injected into lcd_st7789_spi_t)
 * ================================================================ */

__attribute__((unused))
static void st7789_model_cs_low(void *ctx)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    m->cs_level = false;
}

__attribute__((unused))
static void st7789_model_cs_high(void *ctx)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    m->cs_level = true;
}

__attribute__((unused))
static void st7789_model_dc_set(void *ctx, bool level)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    m->dc_level = level;
}

__attribute__((unused))
static void st7789_model_rst_set(void *ctx, bool level)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    if (!level && m->rst_level) {
        /* Falling edge: hardware reset */
        m->rst_toggle_count++;
    }
    m->rst_level = level;
    if (!level) {
        /* Reset asserted -- return to POR defaults */
        lcd_st7789_model_sw_reset(m);
    }
}

__attribute__((unused))
static uint8_t st7789_model_xfer(void *ctx, uint8_t tx)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    m->xfer_count++;

    if (!m->dc_level) {
        /* Command mode */
        st7789_model_process_cmd(m, tx);
    } else {
        /* Data mode */
        st7789_model_process_data(m, tx);
    }
    return 0xFF;  /* ST7789 does not return meaningful data on MISO for writes */
}

__attribute__((unused))
static void st7789_model_delay_ms(void *ctx, uint32_t ms)
{
    lcd_st7789_model_t *m = (lcd_st7789_model_t *)ctx;
    m->delay_total_ms += (int)ms;
    m->delay_call_count++;
}

/* --- Convenience: build a lcd_st7789_spi_t pointing at the model --- */

__attribute__((unused))
static lcd_st7789_spi_t st7789_model_spi(lcd_st7789_model_t *m)
{
    lcd_st7789_spi_t spi;
    spi.cs_low   = st7789_model_cs_low;
    spi.cs_high  = st7789_model_cs_high;
    spi.xfer     = st7789_model_xfer;
    spi.dc_set   = st7789_model_dc_set;
    spi.rst_set  = st7789_model_rst_set;
    spi.delay_ms = st7789_model_delay_ms;
    spi.ctx      = m;
    return spi;
}

/* --- Query helpers for tests --- */

__attribute__((unused))
static bool st7789_model_is_awake(const lcd_st7789_model_t *m)
{
    return !m->sleep;
}

__attribute__((unused))
static bool st7789_model_is_display_on(const lcd_st7789_model_t *m)
{
    return m->display_on;
}

#endif /* HW_MODEL_LCD_ST7789_H */
