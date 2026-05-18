// SPDX-License-Identifier: MIT
/*
 * hw_model_ft6x36.h — FT6x36 hardware model from datasheet
 *
 * Models FocalTech FT6236/FT6336 behavior:
 * - Register-based I2C interface at 0x38
 * - Up to 2 simultaneous touch points
 * - Touch point data: 6 bytes per point (XH/XL/YH/YL + weight/misc)
 * - TD_STATUS (0x02) reports active touch count
 * - CHIP_ID (0xA8), FW_VER (0xA6) identification registers
 * - TH_GROUP (0x80) touch detection threshold
 *
 * Uses the standard register-based mock_i2c callbacks.
 */

#ifndef HW_MODEL_FT6X36_H
#define HW_MODEL_FT6X36_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../ft6x36.h"

typedef struct {
    uint8_t  chip_id;
    uint8_t  fw_version;
    uint8_t  threshold;
    uint8_t  touch_count;       /* 0-2 */
    uint16_t x1, y1;
    uint8_t  event1;            /* 0=press, 1=release, 2=contact */
    uint16_t x2, y2;
    uint8_t  event2;
    int      write_count;
    int      read_count;
} ft6x36_model_t;

static void ft6x36_model_init(ft6x36_model_t *m, uint8_t chip_id)
{
    memset(m, 0, sizeof(*m));
    m->chip_id    = chip_id;
    m->fw_version = 0x10;
    m->threshold  = 22;         /* factory default */
}

static void ft6x36_model_set_touch1(ft6x36_model_t *m,
                                    uint16_t x, uint16_t y, uint8_t event)
{
    m->x1 = x & 0x0FFFu;
    m->y1 = y & 0x0FFFu;
    m->event1 = event & 0x03u;
    if (m->touch_count < 1) m->touch_count = 1;
}

static void ft6x36_model_set_touch2(ft6x36_model_t *m,
                                    uint16_t x, uint16_t y, uint8_t event)
{
    m->x2 = x & 0x0FFFu;
    m->y2 = y & 0x0FFFu;
    m->event2 = event & 0x03u;
    m->touch_count = 2;
}

static void ft6x36_model_clear_touch(ft6x36_model_t *m)
{
    m->touch_count = 0;
    m->x1 = 0; m->y1 = 0; m->event1 = 0;
    m->x2 = 0; m->y2 = 0; m->event2 = 0;
}

/* Build the register file image from model state */
static void ft6x36_model_build_regs(const ft6x36_model_t *m,
                                    uint8_t *regs, uint16_t len)
{
    if (len < 256) return;
    memset(regs, 0, len);

    /* Identification */
    regs[FT6X36_REG_CHIP_ID] = m->chip_id;
    regs[FT6X36_REG_FW_VER]  = m->fw_version;

    /* Configuration */
    regs[FT6X36_REG_TH_GROUP] = m->threshold;

    /* Touch status */
    regs[FT6X36_REG_TD_STATUS] = m->touch_count;

    /* Point 1 */
    if (m->touch_count >= 1) {
        regs[FT6X36_REG_P1_XH] = (uint8_t)((m->event1 << 6) |
                                            ((m->x1 >> 8) & 0x0Fu));
        regs[FT6X36_REG_P1_XL] = (uint8_t)(m->x1 & 0xFFu);
        regs[FT6X36_REG_P1_YH] = (uint8_t)((m->y1 >> 8) & 0x0Fu);
        regs[FT6X36_REG_P1_YL] = (uint8_t)(m->y1 & 0xFFu);
    }

    /* Point 2 */
    if (m->touch_count >= 2) {
        regs[FT6X36_REG_P2_XH] = (uint8_t)((m->event2 << 6) |
                                            ((m->x2 >> 8) & 0x0Fu));
        regs[FT6X36_REG_P2_XL] = (uint8_t)(m->x2 & 0xFFu);
        regs[FT6X36_REG_P2_YH] = (uint8_t)((m->y2 >> 8) & 0x0Fu);
        regs[FT6X36_REG_P2_YL] = (uint8_t)(m->y2 & 0xFFu);
    }
}

/* I2C callbacks backed by model */

__attribute__((unused))
static bool ft6x36_model_write_reg(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    ft6x36_model_t *m = (ft6x36_model_t *)ctx;
    m->write_count++;

    if (reg == FT6X36_REG_TH_GROUP) {
        m->threshold = val;
    }
    return true;
}

__attribute__((unused))
static bool ft6x36_model_read_regs(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    ft6x36_model_t *m = (ft6x36_model_t *)ctx;
    m->read_count++;

    /* Build register snapshot from model state */
    uint8_t regs[256];
    ft6x36_model_build_regs(m, regs, 256);

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < 256) ? regs[idx] : 0xFFu;
    }
    return true;
}

#endif /* HW_MODEL_FT6X36_H */
