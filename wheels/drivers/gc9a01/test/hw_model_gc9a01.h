// SPDX-License-Identifier: MIT
/*
 * hw_model_gc9a01.h -- GC9A01 SPI display hardware model from datasheet
 *
 * Models GalaxyCore GC9A01 behavior:
 * - SPI command/data interface (DC pin: low=command, high=data)
 * - Internal power-on-reset state: sleep-in, display off, no inversion
 * - Command sequence validation: SLPOUT before DISPON
 * - MADCTL (0x36): rotation / mirror / color order
 * - COLMOD (0x3A): pixel format (RGB565 / RGB666)
 * - CASET/RASET (0x2A/0x2B): column/row address window (4-byte BE params)
 * - RAMWR (0x2C): pixel data write tracking
 * - INVON/INVOFF (0x21/0x20): display inversion
 * - DISPON/DISPOFF (0x29/0x28): display enable
 * - SLPIN/SLPOUT (0x10/0x11): sleep mode
 * - SWRESET (0x01): software reset to POR defaults
 *
 * The model provides SPI HAL callbacks (cs/dc_cmd/dc_data/rst/xfer/delay)
 * that the driver injects, so the full driver init / draw path runs against
 * this model and the resulting internal state can be asserted in tests.
 */

#ifndef HW_MODEL_GC9A01_H
#define HW_MODEL_GC9A01_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../gc9a01.h"

/* --- Capacity for pixel-data tracking --- */
#define GC9A01_MODEL_PIXEL_BUF_SIZE  2048

/* --- GC9A01 internal state model --- */

typedef struct {
    /* Power / display state (POR defaults) */
    bool     sleep;              /* true = sleep-in (POR default) */
    bool     display_on;         /* DISPON / DISPOFF */
    bool     inversion;          /* INVON / INVOFF (POR: off) */

    /* MADCTL (0x36) */
    uint8_t  madctl;             /* raw MADCTL byte */

    /* COLMOD (0x3A) */
    uint8_t  colmod;             /* POR: 0x06 (18-bit RGB666) */

    /* Column / row address window (CASET / RASET) */
    uint16_t col_start;
    uint16_t col_end;
    uint16_t row_start;
    uint16_t row_end;

    /* Pixel data tracking */
    uint8_t  pixel_buf[GC9A01_MODEL_PIXEL_BUF_SIZE];
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
} gc9a01_model_t;

/* ================================================================
 * Model lifecycle
 * ================================================================ */

static void gc9a01_model_sw_reset(gc9a01_model_t *m);

static void gc9a01_model_init(gc9a01_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* POR defaults */
    m->sleep       = true;       /* sleep-in after reset */
    m->display_on  = false;
    m->inversion   = false;
    m->madctl      = 0x00;
    m->colmod      = 0x06;       /* 18-bit (RGB666) */
    m->col_start   = 0x0000;
    m->col_end     = 0x00EF;     /* 239 */
    m->row_start   = 0x0000;
    m->row_end     = 0x00EF;     /* 239 */
    m->dc_level    = true;
    m->cs_level    = true;
    m->rst_level   = true;
}

/* Software reset: returns most state to POR */
static void gc9a01_model_sw_reset(gc9a01_model_t *m)
{
    m->sleep       = true;
    m->display_on  = false;
    m->inversion   = false;
    m->madctl      = 0x00;
    m->colmod      = 0x06;
    m->col_start   = 0x0000;
    m->col_end     = 0x00EF;
    m->row_start   = 0x0000;
    m->row_end     = 0x00EF;
    m->pending_cmd = 0;
    m->arg_pos     = 0;
    m->pixel_buf_pos = 0;
}

/* ================================================================
 * Command processor -- called when DC=low (command mode)
 * ================================================================ */

static void gc9a01_model_process_cmd(gc9a01_model_t *m, uint8_t cmd)
{
    m->cmd_count++;
    m->last_cmd = cmd;

    /* Any new command cancels a pending multi-byte command */
    m->pending_cmd   = 0;
    m->arg_pos       = 0;
    m->arg_expected  = 0;

    switch (cmd) {
    case 0x00: /* NOP */
        break;

    case 0x01: /* SWRESET */
        gc9a01_model_sw_reset(m);
        break;

    case 0x10: /* SLPIN */
        m->sleep = true;
        break;
    case 0x11: /* SLPOUT */
        m->sleep = false;
        break;

    case 0x20: /* INVOFF */
        m->inversion = false;
        break;
    case 0x21: /* INVON */
        m->inversion = true;
        break;

    case 0x28: /* DISPOFF */
        m->display_on = false;
        break;
    case 0x29: /* DISPON */
        if (m->sleep) {
            m->seq_violations++;
        }
        m->display_on = true;
        break;

    /* Multi-byte commands (data follows in data mode) */
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

static void gc9a01_model_process_data(gc9a01_model_t *m, uint8_t byte)
{
    m->data_byte_count++;

    if (m->pending_cmd == 0) {
        return;
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
        if (m->pixel_buf_pos < GC9A01_MODEL_PIXEL_BUF_SIZE) {
            m->pixel_buf[m->pixel_buf_pos++] = byte;
        }
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
        m->arg_pos++;
        if (m->arg_expected > 0 && m->arg_pos >= m->arg_expected) {
            m->pending_cmd = 0;
        }
        break;
    }
}

/* ================================================================
 * SPI HAL callbacks (injected into gc9a01_spi_t)
 * ================================================================ */

__attribute__((unused))
static void gc9a01_model_cs_low(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->cs_level = false;
}

__attribute__((unused))
static void gc9a01_model_cs_high(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->cs_level = true;
}

__attribute__((unused))
static void gc9a01_model_dc_cmd(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->dc_level = false;
}

__attribute__((unused))
static void gc9a01_model_dc_data(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->dc_level = true;
}

__attribute__((unused))
static void gc9a01_model_rst_low(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    if (m->rst_level) {
        m->rst_toggle_count++;
    }
    m->rst_level = false;
    gc9a01_model_sw_reset(m);
}

__attribute__((unused))
static void gc9a01_model_rst_high(void *ctx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->rst_level = true;
}

__attribute__((unused))
static uint8_t gc9a01_model_xfer(void *ctx, uint8_t tx)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->xfer_count++;

    if (!m->dc_level) {
        gc9a01_model_process_cmd(m, tx);
    } else {
        gc9a01_model_process_data(m, tx);
    }
    return 0xFF;
}

__attribute__((unused))
static void gc9a01_model_delay_ms(void *ctx, uint16_t ms)
{
    gc9a01_model_t *m = (gc9a01_model_t *)ctx;
    m->delay_total_ms += (int)ms;
    m->delay_call_count++;
}

/* --- Convenience: build a gc9a01_spi_t pointing at the model --- */

__attribute__((unused))
static gc9a01_spi_t gc9a01_model_spi(gc9a01_model_t *m)
{
    gc9a01_spi_t spi;
    spi.cs_low   = gc9a01_model_cs_low;
    spi.cs_high  = gc9a01_model_cs_high;
    spi.xfer     = gc9a01_model_xfer;
    spi.dc_cmd   = gc9a01_model_dc_cmd;
    spi.dc_data  = gc9a01_model_dc_data;
    spi.rst_low  = gc9a01_model_rst_low;
    spi.rst_high = gc9a01_model_rst_high;
    spi.delay_ms = gc9a01_model_delay_ms;
    spi.ctx      = m;
    return spi;
}

/* --- Query helpers for tests --- */

__attribute__((unused))
static bool gc9a01_model_is_awake(const gc9a01_model_t *m)
{
    return !m->sleep;
}

__attribute__((unused))
static bool gc9a01_model_is_display_on(const gc9a01_model_t *m)
{
    return m->display_on;
}

#endif /* HW_MODEL_GC9A01_H */
