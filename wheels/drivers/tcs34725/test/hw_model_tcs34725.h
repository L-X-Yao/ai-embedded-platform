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
 * - Integration time (ATIME) at 0x01 and wait time (WTIME) at 0x03, both 0xFF
 *   after reset (Table 3)
 * - Control (gain) at 0x0F
 * - STATUS at 0x13 (read-only): AVALID bit 0 once a cycle has finished
 * - RGBC data at 0x14..0x1B: C(14-15) R(16-17) G(18-19) B(1A-1B) LE, loaded at
 *   the end of each cycle and limited to the Max RGBC Count of p.16
 *
 * A cycle is RGBC Init, 2.4 ms (p.10), then 2.4 ms x (256 - ATIME) (p.8). The
 * manual does not say when AVALID falls: here it falls when the RGBC stops or
 * starts again. The wait state (WEN) is not modelled.
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
#define TCS_REG_WTIME   0x83u  /* 0x80 | 0x03 */
#define TCS_REG_CONTROL 0x8Fu  /* 0x80 | 0x0F */
#define TCS_REG_ID      0x92u  /* 0x80 | 0x12 */
#define TCS_REG_STATUS  0x93u  /* 0x80 | 0x13 */
#define TCS_REG_CDATAL  0x94u  /* 0x80 | 0x14 */

/* p.15: minimum interval between PON and the start of an RGBC */
#define TCS_PON_WARMUP_US 2400u

/* p.10: RGBC Init. p.8: one integration step. */
#define TCS_RGBC_INIT_US  2400u
#define TCS_STEP_US       2400u

#define TCS_STATUS_AVALID 0x01u

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_c, true_r, true_g, true_b;
    uint32_t        pon_us;   /* time since PON rose */
    bool            rgbc_running;
    uint32_t        until_cycle_end_us;
} tcs34725_model_t;

/* One channel into its data registers (little-endian) */
static void tcs34725_model_load(tcs34725_model_t *m, unsigned reg, uint16_t val)
{
    m->hw.regs[reg]     = (uint8_t)(val & 0xFF);
    m->hw.regs[reg + 1] = (uint8_t)(val >> 8);
}

/* Populate RGBC data registers as they are, whatever a cycle would allow */
__attribute__((unused))
static void tcs34725_model_set_color_data(tcs34725_model_t *m)
{
    tcs34725_model_load(m, 0x94, m->true_c);
    tcs34725_model_load(m, 0x96, m->true_r);
    tcs34725_model_load(m, 0x98, m->true_g);
    tcs34725_model_load(m, 0x9A, m->true_b);
}

static uint32_t tcs34725_model_cycle_us(const tcs34725_model_t *m)
{
    return TCS_RGBC_INIT_US +
           TCS_STEP_US * (256u - m->hw.regs[TCS_REG_ATIME]);
}

/* p.16: "Max RGBC Count = (256 - ATIME) x 1024 up to a maximum of 65535." */
static uint16_t tcs34725_model_limit(const tcs34725_model_t *m, uint16_t count)
{
    uint32_t max = (256u - m->hw.regs[TCS_REG_ATIME]) * 1024u;

    if (max > 65535u) max = 65535u;
    return (count > max) ? (uint16_t)max : count;
}

/* A cycle ends: the result reaches the registers and AVALID rises */
static void tcs34725_model_end_cycle(tcs34725_model_t *m)
{
    tcs34725_model_load(m, 0x94, tcs34725_model_limit(m, m->true_c));
    tcs34725_model_load(m, 0x96, tcs34725_model_limit(m, m->true_r));
    tcs34725_model_load(m, 0x98, tcs34725_model_limit(m, m->true_g));
    tcs34725_model_load(m, 0x9A, tcs34725_model_limit(m, m->true_b));
    m->hw.regs[TCS_REG_STATUS] |= TCS_STATUS_AVALID;
}

/* Callback: enable register write. AEN rising after the PON interval starts an
 * RGBC. AEN rising with or too soon after PON starts nothing. */
static void tcs34725_model_on_enable(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    tcs34725_model_t *m = (tcs34725_model_t *)user;
    if (!(old_val & 0x01u)) { /* PON rising restarts the interval */
        m->pon_us = 0;
    }
    if ((new_val & 0x03u) != 0x03u) {
        m->rgbc_running = false;
        m->hw.regs[TCS_REG_STATUS] &= (uint8_t)~TCS_STATUS_AVALID;
    } else if (!(old_val & 0x02u) && m->pon_us >= TCS_PON_WARMUP_US) {
        m->rgbc_running = true;
        m->until_cycle_end_us = tcs34725_model_cycle_us(m);
        m->hw.regs[TCS_REG_STATUS] &= (uint8_t)~TCS_STATUS_AVALID;
    }
}

/* Let time pass: cycles end one after the other while the RGBC runs */
static void tcs34725_model_advance_us(tcs34725_model_t *m, uint32_t us)
{
    m->pon_us += us;
    if (!m->rgbc_running) return;

    while (us >= m->until_cycle_end_us) {
        us -= m->until_cycle_end_us;
        tcs34725_model_end_cycle(m);
        m->until_cycle_end_us = tcs34725_model_cycle_us(m);
    }
    m->until_cycle_end_us -= us;
}

/* Delay callback: the only way time passes in the model */
__attribute__((unused))
static void tcs34725_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    tcs34725_model_t *m = (tcs34725_model_t *)hw->user;

    tcs34725_model_advance_us(m, ms * 1000u);
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

    /* Table 3: ATIME and WTIME reset to 0xFF */
    hw_model_set_reset_value(&m->hw, TCS_REG_ATIME, 0xFFu);
    hw_model_set_reset_value(&m->hw, TCS_REG_WTIME, 0xFFu);

    /* STATUS (read-only, written at the end of a cycle) */
    hw_model_set_readonly(&m->hw, TCS_REG_STATUS);

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
