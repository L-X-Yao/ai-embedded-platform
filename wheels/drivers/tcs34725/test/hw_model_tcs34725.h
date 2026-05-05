/*
 * hw_model_tcs34725.h — TCS34725 hardware register model from datasheet
 *
 * Models TCS34725 register behavior:
 * - Chip ID at register 0x12: 0x44 or 0x4D (read-only)
 * - Command protocol: driver sends (0x80 | reg), model stores at raw reg
 * - Enable register at 0x00 (PON bit 0, AEN bit 1)
 * - Integration time (ATIME) at 0x01
 * - Control (gain) at 0x0F
 * - RGBC data at 0x14..0x1B: C(14-15) R(16-17) G(18-19) B(1A-1B) LE
 *
 * NOTE: The driver uses CMD(reg) = 0x80|reg for all I2C transactions.
 *       The hw_model stores registers at the 0x80+ addresses since that
 *       is what the driver actually sends over I2C.
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

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_c, true_r, true_g, true_b;
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

/* Callback: enable register write — when AEN set, populate data */
static void tcs34725_model_on_enable(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    tcs34725_model_t *m = (tcs34725_model_t *)user;
    if ((new_val & 0x03u) == 0x03u) { /* PON + AEN */
        tcs34725_model_set_color_data(m);
    }
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
