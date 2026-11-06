// SPDX-License-Identifier: MIT
/*
 * hw_model_oled_ssd1306.h — SSD1306 OLED hardware command model
 *
 * Models SSD1306 command processor behavior:
 * - Commands sent via 0x00 control prefix (write_cmd)
 * - Data sent via 0x40 control prefix (write_data)
 * - Display on/off state (0xAE/0xAF)
 * - Contrast setting (0x81 + value)
 * - Addressing mode (0x20 + mode)
 * - Charge pump enable (0x8D + 0x14/0x10)
 * - MUX ratio (0xA8 + value)
 * - GDDRAM of 8 pages by 128 columns, written through the column and page
 *   pointers as the addressing mode moves them (Rev 1.1 section 10.1.3)
 *
 * Unlike register-based I2C devices, SSD1306 uses a command stream.
 * We model internal state that tracks commands and their arguments.
 */

#ifndef HW_MODEL_OLED_SSD1306_H
#define HW_MODEL_OLED_SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SSD1306 internal state model --- */

typedef struct {
    /* Display state */
    bool     display_on;
    uint8_t  contrast;
    uint8_t  addressing_mode;   /* 0x00=horiz, 0x01=vert, 0x02=page */
    bool     charge_pump;
    uint8_t  mux_ratio;
    uint8_t  display_offset;
    uint8_t  display_start_line;
    bool     seg_remap;         /* 0xA0=false, 0xA1=true */
    bool     com_scan_dir;      /* 0xC0=false (normal), 0xC8=true (remapped) */
    uint8_t  com_pin_cfg;
    uint8_t  precharge_period;
    uint8_t  vcomh_level;
    bool     entire_display_on; /* 0xA4=false (RAM), 0xA5=true (all-on) */
    bool     invert_display;    /* 0xA6=false (normal), 0xA7=true (inverted) */
    uint8_t  clock_divide;

    /* Column/page address window (for horizontal/vertical mode) */
    uint8_t  col_start;
    uint8_t  col_end;
    uint8_t  page_start;
    uint8_t  page_end;

    /* Where the next GDDRAM byte goes */
    uint8_t  col_ptr;
    uint8_t  page_ptr;

    /* Command parsing state: some commands are multi-byte */
    uint8_t  pending_cmd;       /* command waiting for argument byte */
    uint8_t  pending_start;     /* first argument of 21h or 22h */
    int      cmd_count;         /* total commands received */
    int      data_count;        /* total data writes received */
    /* Entries the manual calls invalid, and window commands sent in an
     * addressing mode they are not for */
    int      invalid_cmds;

    /* Data buffer for tracking what was flushed */
    uint8_t  gddram[1024];     /* page * 128 + column */
    int      data_bytes;       /* bytes written since reset */
} oled_ssd1306_model_t;

static void oled_ssd1306_model_init(oled_ssd1306_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* POR defaults from datasheet */
    m->display_on       = false;        /* display off after reset */
    m->contrast         = 0x7F;
    m->addressing_mode  = 0x02;         /* page addressing */
    m->charge_pump      = false;
    m->mux_ratio        = 63;           /* 64 lines */
    m->display_offset   = 0;
    m->display_start_line = 0;
    m->seg_remap        = false;
    m->com_scan_dir     = false;
    m->com_pin_cfg      = 0x12;
    m->precharge_period = 0x22;
    m->vcomh_level      = 0x20;
    m->entire_display_on = false;
    m->invert_display   = false;
    m->clock_divide     = 0x80;
    m->col_start        = 0;
    m->col_end          = 127;
    m->page_start       = 0;
    m->page_end         = 7;
    m->pending_cmd      = 0;
    m->cmd_count        = 0;
    m->data_count       = 0;
    m->data_bytes       = 0;
}

#define OLED_MODEL_MODE_HORIZONTAL 0x00
#define OLED_MODEL_MODE_VERTICAL   0x01
#define OLED_MODEL_MODE_PAGE       0x02

/* 00h-0Fh, 10h-1Fh and B0h-B7h, each one "only for page addressing mode" (p.30,
 * p.31). The first two set the "column start address register". */
static void oled_ssd1306_model_page_cmd(oled_ssd1306_model_t *m, uint8_t byte)
{
    if (m->addressing_mode != OLED_MODEL_MODE_PAGE) {
        m->invalid_cmds++;
    } else if (byte <= 0x0F) {
        m->col_start = (uint8_t)((m->col_start & 0xF0) | byte);
        m->col_ptr = m->col_start;
    } else if (byte <= 0x1F) {
        m->col_start = (uint8_t)((m->col_start & 0x0F) | ((byte & 0x07) << 4));
        m->col_ptr = m->col_start;
    } else {
        m->page_ptr = byte & 0x07;
    }
}

/* Process a single command byte (may consume argument on next call) */
static void oled_ssd1306_model_process_cmd(oled_ssd1306_model_t *m, uint8_t byte)
{
    m->cmd_count++;

    /* If we are expecting an argument for a previous command */
    if (m->pending_cmd != 0) {
        uint8_t cmd = m->pending_cmd;
        m->pending_cmd = 0;

        switch (cmd) {
        case 0x81: m->contrast = byte; break;
        case 0x20:
            /* p.30: "A[1:0] = 11b, Invalid" */
            if ((byte & 0x03) == 0x03)
                m->invalid_cmds++;
            else
                m->addressing_mode = byte & 0x03;
            break;
        case 0x8D: m->charge_pump = (byte == 0x14); break;
        case 0xA8:
            /* p.31: "A[5:0] from 0 to 14 are invalid entry." */
            if ((byte & 0x3F) <= 14)
                m->invalid_cmds++;
            else
                m->mux_ratio = byte & 0x3F;
            break;
        case 0xD3: m->display_offset = byte; break;
        case 0xDA: m->com_pin_cfg = byte; break;
        case 0xD5: m->clock_divide = byte; break;
        case 0xD9: m->precharge_period = byte; break;
        case 0xDB: m->vcomh_level = byte; break;
        case 0x21: /* set column address: start byte */
            m->pending_start = byte & 0x7F;
            m->pending_cmd = 0xE1; /* synthetic: waiting for col_end */
            break;
        case 0xE1: /* synthetic: col_end */
            /* p.30: "This command is only for horizontal or vertical
             * addressing mode." p.35: it "also sets the column address pointer
             * to column start address." */
            if (m->addressing_mode == OLED_MODEL_MODE_PAGE) {
                m->invalid_cmds++;
            } else {
                m->col_start = m->pending_start;
                m->col_end = byte & 0x7F;
                m->col_ptr = m->col_start;
            }
            break;
        case 0x22: /* set page address: start byte */
            m->pending_start = byte & 0x07;
            m->pending_cmd = 0xE2; /* synthetic: waiting for page_end */
            break;
        case 0xE2: /* synthetic: page_end */
            /* p.31: the same limit, and p.36: 22h "also sets the page address
             * pointer to page start address." */
            if (m->addressing_mode == OLED_MODEL_MODE_PAGE) {
                m->invalid_cmds++;
            } else {
                m->page_start = m->pending_start;
                m->page_end = byte & 0x07;
                m->page_ptr = m->page_start;
            }
            break;
        default:
            break;
        }
        return;
    }

    /* Single-byte commands */
    switch (byte) {
    case 0xAE: m->display_on = false; break;
    case 0xAF: m->display_on = true; break;
    case 0xA4: m->entire_display_on = false; break;
    case 0xA5: m->entire_display_on = true; break;
    case 0xA6: m->invert_display = false; break;
    case 0xA7: m->invert_display = true; break;
    case 0xA0: m->seg_remap = false; break;
    case 0xA1: m->seg_remap = true; break;
    case 0xC0: m->com_scan_dir = false; break;
    case 0xC8: m->com_scan_dir = true; break;

    /* Multi-byte commands: set pending */
    case 0x81: /* set contrast */
    case 0x20: /* addressing mode */
    case 0x8D: /* charge pump */
    case 0xA8: /* MUX ratio */
    case 0xD3: /* display offset */
    case 0xDA: /* COM pin config */
    case 0xD5: /* clock divide */
    case 0xD9: /* precharge period */
    case 0xDB: /* VCOMH deselect level */
    case 0x21: /* column address */
    case 0x22: /* page address */
        m->pending_cmd = byte;
        break;

    default:
        /* Display start line: 0x40-0x7F */
        if ((byte & 0xC0) == 0x40) {
            m->display_start_line = byte & 0x3F;
        } else if (byte <= 0x1F || (byte >= 0xB0 && byte <= 0xB7)) {
            oled_ssd1306_model_page_cmd(m, byte);
        }
        break;
    }
}

/* Column pointer one on, back at the start after the end column */
static bool oled_ssd1306_model_next_col(oled_ssd1306_model_t *m)
{
    if (m->col_ptr == m->col_end) {
        m->col_ptr = m->col_start;
        return true;
    }
    m->col_ptr = (uint8_t)((m->col_ptr + 1) & 0x7F);
    return false;
}

/* Page pointer one on, back at the start after the end page */
static bool oled_ssd1306_model_next_page(oled_ssd1306_model_t *m)
{
    if (m->page_ptr == m->page_end) {
        m->page_ptr = m->page_start;
        return true;
    }
    m->page_ptr = (uint8_t)((m->page_ptr + 1) & 0x07);
    return false;
}

/* p.34 and p.35: how each addressing mode moves on after a byte */
static void oled_ssd1306_model_put(oled_ssd1306_model_t *m, uint8_t byte)
{
    m->gddram[(uint16_t)(m->page_ptr * 128u + m->col_ptr)] = byte;
    m->data_bytes++;

    switch (m->addressing_mode) {
    case OLED_MODEL_MODE_HORIZONTAL:
        if (oled_ssd1306_model_next_col(m))
            (void)oled_ssd1306_model_next_page(m);
        break;
    case OLED_MODEL_MODE_VERTICAL:
        if (oled_ssd1306_model_next_page(m))
            (void)oled_ssd1306_model_next_col(m);
        break;
    default:
        (void)oled_ssd1306_model_next_col(m);
        break;
    }
}

/* Process data bytes (GDDRAM write) */
static void oled_ssd1306_model_process_data(oled_ssd1306_model_t *m,
                                            const uint8_t *data, uint16_t len)
{
    m->data_count++;
    for (uint16_t i = 0; i < len; i++) {
        oled_ssd1306_model_put(m, data[i]);
    }
}

/* --- HAL callbacks for driver injection --- */

__attribute__((unused))
static bool oled_model_write_cmd(void *ctx, uint8_t dev_addr, uint8_t cmd)
{
    (void)dev_addr;
    oled_ssd1306_model_t *m = (oled_ssd1306_model_t *)ctx;
    oled_ssd1306_model_process_cmd(m, cmd);
    return true;
}

__attribute__((unused))
static bool oled_model_write_data(void *ctx, uint8_t dev_addr,
                                  const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    oled_ssd1306_model_t *m = (oled_ssd1306_model_t *)ctx;
    oled_ssd1306_model_process_data(m, data, len);
    return true;
}

#endif /* HW_MODEL_OLED_SSD1306_H */
