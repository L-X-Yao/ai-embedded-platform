// SPDX-License-Identifier: MIT
/*
 * hw_model_lis3mdl.h — LIS3MDL hardware register model from datasheet
 *
 * Models LIS3MDL register behavior:
 * - WHO_AM_I at 0x0F: 0x3D (read-only)
 * - CTRL_REG1 at 0x20: ODR + XY performance mode
 * - CTRL_REG2 at 0x21: full-scale range [6:5]
 * - CTRL_REG3 at 0x22: operating mode [1:0]
 * - Data registers 0x28..0x2D: XL,XH,YL,YH,ZL,ZH (read-only, little-endian)
 * - Auto-increment via bit 7 of sub-address
 */

#ifndef HW_MODEL_LIS3MDL_H
#define HW_MODEL_LIS3MDL_H

#include "hw_model.h"

/* --- LIS3MDL register addresses --- */
#define LIS3MDL_REG_WHO_AM_I  0x0Fu
#define LIS3MDL_REG_CTRL_REG1 0x20u
#define LIS3MDL_REG_CTRL_REG2 0x21u
#define LIS3MDL_REG_CTRL_REG3 0x22u
#define LIS3MDL_REG_OUT_X_L   0x28u
#define LIS3MDL_REG_OUT_X_H   0x29u
#define LIS3MDL_REG_OUT_Y_L   0x2Au
#define LIS3MDL_REG_OUT_Y_H   0x2Bu
#define LIS3MDL_REG_OUT_Z_L   0x2Cu
#define LIS3MDL_REG_OUT_Z_H   0x2Du

typedef struct {
    hw_reg_model_t  hw;
    int16_t         true_x;     /* simulated raw X counts */
    int16_t         true_y;     /* simulated raw Y counts */
    int16_t         true_z;     /* simulated raw Z counts */
    int             measure_count;
} lis3mdl_model_t;

/* Populate data registers from true values (little-endian) */
static void lis3mdl_model_set_data(lis3mdl_model_t *m)
{
    m->hw.regs[LIS3MDL_REG_OUT_X_L] = (uint8_t)((uint16_t)m->true_x & 0xFFu);
    m->hw.regs[LIS3MDL_REG_OUT_X_H] = (uint8_t)((uint16_t)m->true_x >> 8);
    m->hw.regs[LIS3MDL_REG_OUT_Y_L] = (uint8_t)((uint16_t)m->true_y & 0xFFu);
    m->hw.regs[LIS3MDL_REG_OUT_Y_H] = (uint8_t)((uint16_t)m->true_y >> 8);
    m->hw.regs[LIS3MDL_REG_OUT_Z_L] = (uint8_t)((uint16_t)m->true_z & 0xFFu);
    m->hw.regs[LIS3MDL_REG_OUT_Z_H] = (uint8_t)((uint16_t)m->true_z >> 8);
}

/* Callback: CTRL_REG3 write — continuous mode (0x00) triggers data */
static void lis3mdl_model_on_ctrl3(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    lis3mdl_model_t *m = (lis3mdl_model_t *)user;
    uint8_t mode = new_val & 0x03u;
    if (mode == 0x00u) { /* continuous measurement */
        lis3mdl_model_set_data(m);
        m->measure_count++;
    }
}

/*
 * Override hw_model_i2c_read_regs to handle auto-increment (bit 7).
 * When bit 7 is set, strip it and read consecutive registers.
 */
__attribute__((unused))
static bool lis3mdl_hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t *buf,
                                        uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    uint8_t base = reg & 0x7Fu;  /* strip auto-increment bit */
    uint16_t i;
    for (i = 0; i < len; i++) {
        unsigned idx = (unsigned)base + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->regs[idx] : 0xFFu;
    }
    return true;
}

static void lis3mdl_model_init(lis3mdl_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* WHO_AM_I (read-only): 0x3D */
    hw_model_set_reset_value(&m->hw, LIS3MDL_REG_WHO_AM_I, 0x3Du);
    hw_model_set_readonly(&m->hw, LIS3MDL_REG_WHO_AM_I);

    /* Data registers 0x28..0x2D (read-only, written by measurement engine) */
    for (unsigned i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(LIS3MDL_REG_OUT_X_L + i));
    }

    /* CTRL_REG2: only bits [6:5] writable (full-scale selection) */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG2, 0x60u);

    /* CTRL_REG3: only bits [1:0] writable (operating mode) */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG3, 0x03u);

    /* Write callback for CTRL_REG3 (operating mode) */
    hw_model_set_on_write(&m->hw, LIS3MDL_REG_CTRL_REG3, lis3mdl_model_on_ctrl3);

    /* Default simulated field: moderate values */
    m->true_x = 1000;
    m->true_y = -500;
    m->true_z = 750;

    hw_model_reset(&m->hw);
}

/*
 * Inject magnetic field in microtesla for a given range.
 * Computes the raw count that the driver should read back as ~ut.
 */
static void lis3mdl_model_inject_ut(lis3mdl_model_t *m,
                                     float x_ut, float y_ut, float z_ut,
                                     float sensitivity)
{
    m->true_x = (int16_t)(x_ut / 100.0f * sensitivity);
    m->true_y = (int16_t)(y_ut / 100.0f * sensitivity);
    m->true_z = (int16_t)(z_ut / 100.0f * sensitivity);
    lis3mdl_model_set_data(m);
}

#endif /* HW_MODEL_LIS3MDL_H */
