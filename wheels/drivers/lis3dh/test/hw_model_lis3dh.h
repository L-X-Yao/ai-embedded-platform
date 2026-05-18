// SPDX-License-Identifier: MIT
/*
 * hw_model_lis3dh.h — LIS3DH hardware register model from datasheet
 *
 * Models ST LIS3DH register behavior:
 * - WHO_AM_I 0x33 at register 0x0F (read-only)
 * - CTRL_REG1 at 0x20 (ODR + axis enable)
 * - CTRL_REG4 at 0x23 (BDU + full-scale)
 * - Accel data at 0x28..0x2D (6 bytes, read-only, little-endian,
 *   16-bit left-justified)
 *
 * Note: LIS3DH uses auto-increment bit (0x80) OR-ed with sub-address
 * for multi-byte reads. The hw_model I2C callback sees the OR-ed
 * address, so we populate register slots at both 0x28 and 0xA8 ranges.
 */

#ifndef HW_MODEL_LIS3DH_H
#define HW_MODEL_LIS3DH_H

#include "hw_model.h"

/* --- LIS3DH register addresses --- */
#define LIS3DH_REG_WHO_AM_I   0x0Fu
#define LIS3DH_REG_CTRL_REG1  0x20u
#define LIS3DH_REG_CTRL_REG2  0x21u
#define LIS3DH_REG_CTRL_REG3  0x22u
#define LIS3DH_REG_CTRL_REG4  0x23u
#define LIS3DH_REG_CTRL_REG5  0x24u
#define LIS3DH_REG_CTRL_REG6  0x25u
#define LIS3DH_REG_OUT_X_L    0x28u
#define LIS3DH_REG_OUT_X_H    0x29u
#define LIS3DH_REG_OUT_Y_L    0x2Au
#define LIS3DH_REG_OUT_Y_H    0x2Bu
#define LIS3DH_REG_OUT_Z_L    0x2Cu
#define LIS3DH_REG_OUT_Z_H    0x2Du

/* Auto-increment mirror addresses (0x28|0x80 = 0xA8) */
#define LIS3DH_AUTO_INC       0x80u

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_x, true_y, true_z;  /* 16-bit left-justified raw */
} lis3dh_model_t;

/* Write accel raw values into data registers (little-endian) */
static void lis3dh_model_set_data_regs(lis3dh_model_t *m)
{
    /* Normal register addresses 0x28..0x2D */
    m->hw.regs[0x28] = (uint8_t)((uint16_t)m->true_x & 0xFF);
    m->hw.regs[0x29] = (uint8_t)((uint16_t)m->true_x >> 8);
    m->hw.regs[0x2A] = (uint8_t)((uint16_t)m->true_y & 0xFF);
    m->hw.regs[0x2B] = (uint8_t)((uint16_t)m->true_y >> 8);
    m->hw.regs[0x2C] = (uint8_t)((uint16_t)m->true_z & 0xFF);
    m->hw.regs[0x2D] = (uint8_t)((uint16_t)m->true_z >> 8);

    /* Mirror at auto-increment addresses (0xA8..0xAD).
     * The driver reads via read_regs(dev, 0x28 | 0x80, buf, 6), which
     * makes the hw_model read starting at register 0xA8. */
    m->hw.regs[0xA8] = m->hw.regs[0x28];
    m->hw.regs[0xA9] = m->hw.regs[0x29];
    m->hw.regs[0xAA] = m->hw.regs[0x2A];
    m->hw.regs[0xAB] = m->hw.regs[0x2B];
    m->hw.regs[0xAC] = m->hw.regs[0x2C];
    m->hw.regs[0xAD] = m->hw.regs[0x2D];
}

static void lis3dh_model_init(lis3dh_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* WHO_AM_I (read-only) */
    hw_model_set_reset_value(&m->hw, LIS3DH_REG_WHO_AM_I, 0x33);
    hw_model_set_readonly(&m->hw, LIS3DH_REG_WHO_AM_I);

    /* Data registers (read-only) at both normal and auto-increment mirrors */
    for (unsigned i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(LIS3DH_REG_OUT_X_L + i));
        hw_model_set_readonly(&m->hw, (uint8_t)(LIS3DH_REG_OUT_X_L + LIS3DH_AUTO_INC + i));
    }

    /* Default "true" sensor output: 0g X, 0g Y, +1g Z
     * At 2g range, sensitivity = 1 mg/digit in 12-bit.
     * Left-justified: 1g = 1000 * 16 = 16000 */
    m->true_x = 0;
    m->true_y = 0;
    m->true_z = 16000;    /* ~1g at 2g range */

    hw_model_reset(&m->hw);
    lis3dh_model_set_data_regs(m);
}

static void lis3dh_model_set_accel(lis3dh_model_t *m,
                                    int16_t x, int16_t y, int16_t z)
{
    m->true_x = x;
    m->true_y = y;
    m->true_z = z;
    lis3dh_model_set_data_regs(m);
}

#endif /* HW_MODEL_LIS3DH_H */
