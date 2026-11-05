// SPDX-License-Identifier: MIT
/*
 * hw_model_tcs34725.h — TCS34725 hardware register model from datasheet
 *
 * Models TCS34725 register behavior:
 * - Chip ID at register 0x12: 0x44 or 0x4D (read-only)
 * - Command byte: CMD bit 7, TYPE bits 6:5, ADDR bits 4:0 (p.14). On reads
 *   TYPE 00b repeats one register, TYPE 01b steps to successive registers
 * - Enable register at 0x00 (PON bit 0, AEN bit 1). AEN starts an RGBC only
 *   2.4 ms after PON rose (p.15); only the delay callback lets time pass
 * - Integration time (ATIME) at 0x01
 * - Control (gain) at 0x0F
 * - RGBC data at 0x14..0x1B: C(14-15) R(16-17) G(18-19) B(1A-1B) LE
 *
 * NOTE: The hw_model stores registers at 0x80 | addr, the TYPE 00b command
 *       byte. Use tcs34725_model_read_regs, not the harness callback, which
 *       steps whatever the command byte says.
 */

#ifndef HW_MODEL_TCS34725_H
#define HW_MODEL_TCS34725_H

#include "hw_model.h"

/* Register addresses as the driver sees them (with 0x80 command bit) */
#define TCS_REG_ENABLE  0x80u  /* 0x80 | 0x00 */
#define TCS_REG_ATIME   0x81u  /* 0x80 | 0x01 */
#define TCS_REG_CONTROL 0x8Fu  /* 0x80 | 0x0F */
#define TCS_REG_ID      0x92u  /* 0x80 | 0x12 */
#define TCS_REG_CDATAL  0x94u  /* 0x80 | 0x14 */

/* p.15: minimum interval between PON and the start of an RGBC */
#define TCS_PON_WARMUP_US 2400u

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_c, true_r, true_g, true_b;
    uint32_t        pon_us;   /* time since PON rose */
} tcs34725_model_t;

/* Populate RGBC data registers (little-endian) */
static void tcs34725_model_set_color_data(tcs34725_model_t *m)
{
    m->hw.regs[0x94] = (uint8_t)(m->true_c & 0xFF);
    m->hw.regs[0x95] = (uint8_t)(m->true_c >> 8);
    m->hw.regs[0x96] = (uint8_t)(m->true_r & 0xFF);
    m->hw.regs[0x97] = (uint8_t)(m->true_r >> 8);
    m->hw.regs[0x98] = (uint8_t)(m->true_g & 0xFF);
    m->hw.regs[0x99] = (uint8_t)(m->true_g >> 8);
    m->hw.regs[0x9A] = (uint8_t)(m->true_b & 0xFF);
    m->hw.regs[0x9B] = (uint8_t)(m->true_b >> 8);
}

/* Callback: enable register write — when AEN rises after the PON interval,
 * populate data. AEN rising with or too soon after PON starts nothing. */
static void tcs34725_model_on_enable(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    tcs34725_model_t *m = (tcs34725_model_t *)user;
    if (!(old_val & 0x01u)) { /* PON rising restarts the interval */
        m->pon_us = 0;
    }
    if ((new_val & 0x03u) == 0x03u && !(old_val & 0x02u) &&
        m->pon_us >= TCS_PON_WARMUP_US) {
        tcs34725_model_set_color_data(m);
    }
}

/* Delay callback: the only way time passes in the model */
__attribute__((unused))
static void tcs34725_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    tcs34725_model_t *m = (tcs34725_model_t *)hw->user;

    m->pon_us += ms * 1000u;
    hw_model_delay_ms(ctx, ms);
}

/* Read callback: TYPE in the command byte picks the protocol (p.14) */
__attribute__((unused))
static bool tcs34725_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t cmd,
                                     uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(hw)) return false;

    unsigned reg  = 0x80u | (cmd & 0x1Fu);
    unsigned step = ((cmd & 0x60u) == 0x20u) ? 1u : 0u;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw->regs[(reg + i * step) & 0xFFu];
    }
    return true;
}

static void tcs34725_model_init(tcs34725_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID: 0x44 at command address 0x92 (read-only) */
    hw_model_set_reset_value(&m->hw, TCS_REG_ID, 0x44u);
    hw_model_set_readonly(&m->hw, TCS_REG_ID);

    /* RGBC data registers (read-only, written by measurement engine) */
    for (unsigned i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(TCS_REG_CDATAL + i));
    }

    /* Enable register callback */
    hw_model_set_on_write(&m->hw, TCS_REG_ENABLE, tcs34725_model_on_enable);

    /* Default simulated RGBC */
    m->true_c = 5000;
    m->true_r = 1800;
    m->true_g = 2200;
    m->true_b = 1000;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_TCS34725_H */
