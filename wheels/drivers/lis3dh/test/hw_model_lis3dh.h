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
 * The I2C sub-address (p.21): "the 7 LSb represent the actual register
 * address while the MSB enables address auto increment". Use
 * lis3dh_model_read_regs and lis3dh_model_write_reg, which do that. Without
 * bit 7 every byte of a read comes from the same register.
 *
 * Table 17 (p.26): type r registers ignore writes, and writes to the rows
 * marked "Reserved (do not modify)" are refused and counted.
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

/* Sub-address bit 7: address auto increment, not part of the address */
#define LIS3DH_AUTO_INC       0x80u
#define LIS3DH_ADDR_MASK      0x7Fu

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_x, true_y, true_z;  /* 16-bit left-justified raw */
    int              reserved_write_count;    /* writes to Reserved registers */
} lis3dh_model_t;

/* Write accel raw values into data registers (little-endian) */
static void lis3dh_model_set_data_regs(lis3dh_model_t *m)
{
    m->hw.regs[0x28] = (uint8_t)((uint16_t)m->true_x & 0xFF);
    m->hw.regs[0x29] = (uint8_t)((uint16_t)m->true_x >> 8);
    m->hw.regs[0x2A] = (uint8_t)((uint16_t)m->true_y & 0xFF);
    m->hw.regs[0x2B] = (uint8_t)((uint16_t)m->true_y >> 8);
    m->hw.regs[0x2C] = (uint8_t)((uint16_t)m->true_z & 0xFF);
    m->hw.regs[0x2D] = (uint8_t)((uint16_t)m->true_z >> 8);
}

/* Table 17: 00-06 and 10-1E are "Reserved (do not modify)" */
static bool lis3dh_model_is_reserved(uint8_t reg)
{
    return reg <= 0x06u || (reg >= 0x10u && reg <= 0x1Eu);
}

__attribute__((unused))
static bool lis3dh_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t sub,
                                   uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(m)) return false;

    uint8_t reg = (uint8_t)(sub & LIS3DH_ADDR_MASK);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = m->regs[reg];
        if (sub & LIS3DH_AUTO_INC)
            reg = (uint8_t)((reg + 1u) & LIS3DH_ADDR_MASK);
    }
    return true;
}

__attribute__((unused))
static bool lis3dh_model_write_reg(void *ctx, uint8_t dev_addr, uint8_t sub,
                                   uint8_t val)
{
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    uint8_t reg = (uint8_t)(sub & LIS3DH_ADDR_MASK);

    /* p.27: writing to those registers "may cause permanent damages" */
    if (lis3dh_model_is_reserved(reg)) {
        ((lis3dh_model_t *)m->user)->reserved_write_count++;
        return !hw_model__should_fail(m);
    }
    return hw_model_i2c_write_reg(ctx, dev_addr, reg, val);
}

static void lis3dh_model_init(lis3dh_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* WHO_AM_I (read-only) */
    hw_model_set_reset_value(&m->hw, LIS3DH_REG_WHO_AM_I, 0x33);
    hw_model_set_readonly(&m->hw, LIS3DH_REG_WHO_AM_I);

    /* Table 17, p.26: CTRL_REG1 defaults to 00000111 */
    hw_model_set_reset_value(&m->hw, LIS3DH_REG_CTRL_REG1, 0x07);

    /* Table 17: the other type r registers, data included */
    for (uint8_t reg = 0x07u; reg <= 0x0Eu; reg++) {
        hw_model_set_readonly(&m->hw, reg);
    }
    for (uint8_t reg = 0x27u; reg <= LIS3DH_REG_OUT_Z_H; reg++) {
        hw_model_set_readonly(&m->hw, reg);
    }
    hw_model_set_readonly(&m->hw, 0x2Fu);  /* FIFO_SRC_REG */
    hw_model_set_readonly(&m->hw, 0x31u);  /* INT1_SOURCE */
    hw_model_set_readonly(&m->hw, 0x39u);  /* CLICK_SRC */

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
