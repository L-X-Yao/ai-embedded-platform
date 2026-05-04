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

    /* Command parsing state: some commands are multi-byte */
    uint8_t  pending_cmd;       /* command waiting for argument byte */
    int      cmd_count;         /* total commands received */
    int      data_count;        /* total data writes received */

    /* Data buffer for tracking what was flushed */
    uint8_t  gddram[1024];     /* 128x64 / 8 = 1024 bytes */
    int      gddram_pos;       /* current write position */
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
    m->gddram_pos       = 0;
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
        case 0x20: m->addressing_mode = byte & 0x03; break;
        case 0x8D: m->charge_pump = (byte == 0x14); break;
        case 0xA8: m->mux_ratio = byte; break;
        case 0xD3: m->display_offset = byte; break;
        case 0xDA: m->com_pin_cfg = byte; break;
        case 0xD5: m->clock_divide = byte; break;
        case 0xD9: m->precharge_period = byte; break;
        case 0xDB: m->vcomh_level = byte; break;
        case 0x21: /* set column address: start byte */
            m->col_start = byte;
            m->pending_cmd = 0xE1; /* synthetic: waiting for col_end */
            break;
        case 0xE1: /* synthetic: col_end */
            m->col_end = byte;
            break;
        case 0x22: /* set page address: start byte */
            m->page_start = byte;
            m->pending_cmd = 0xE2; /* synthetic: waiting for page_end */
            break;
        case 0xE2: /* synthetic: page_end */
            m->page_end = byte;
            m->gddram_pos = 0; /* reset write position for flush */
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
        }
        break;
    }
}

/* Process data bytes (GDDRAM write) */
static void oled_ssd1306_model_process_data(oled_ssd1306_model_t *m,
                                            const uint8_t *data, uint16_t len)
{
    m->data_count++;
    for (uint16_t i = 0; i < len && m->gddram_pos < 1024; i++) {
        m->gddram[m->gddram_pos++] = data[i];
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
