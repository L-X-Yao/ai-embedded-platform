// SPDX-License-Identifier: MIT
/*
 * hw_model_bno055.h — BNO055 hardware register model from datasheet
 *
 * Models BST-BNO055-DS000 register behavior:
 * - Chip ID 0xA0 at 0x00 (read-only)
 * - OPR_MODE at 0x3D controls fusion mode
 * - Euler angles at 0x1A..0x1F (read-only, int16 LE, 1/16 deg/LSB)
 * - CALIB_STAT at 0x35 (read-only)
 * - SYS_TRIGGER at 0x3F for reset
 */

#ifndef HW_MODEL_BNO055_H
#define HW_MODEL_BNO055_H

#include "hw_model.h"

#define BNO055_REG_CHIP_ID     0x00
#define BNO055_REG_EUL_HEAD    0x1A
#define BNO055_REG_CALIB_STAT  0x35
#define BNO055_REG_OPR_MODE    0x3D
#define BNO055_REG_SYS_TRIGGER 0x3F

typedef struct {
    hw_reg_model_t hw;
    int16_t        true_heading;   /* LSB, 1/16 degree */
    int16_t        true_roll;
    int16_t        true_pitch;
    uint8_t        calib_status;
    bool           fusion_active;
    int            mode_switch_count;
} bno055_model_t;

static void bno055_model_update_euler(bno055_model_t *m)
{
    if (!m->fusion_active) return;
    m->hw.regs[0x1A] = (uint8_t)(m->true_heading & 0xFF);
    m->hw.regs[0x1B] = (uint8_t)((m->true_heading >> 8) & 0xFF);
    m->hw.regs[0x1C] = (uint8_t)(m->true_roll & 0xFF);
    m->hw.regs[0x1D] = (uint8_t)((m->true_roll >> 8) & 0xFF);
    m->hw.regs[0x1E] = (uint8_t)(m->true_pitch & 0xFF);
    m->hw.regs[0x1F] = (uint8_t)((m->true_pitch >> 8) & 0xFF);
}

static void bno055_model_on_opr_mode(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bno055_model_t *m = (bno055_model_t *)user;
    m->mode_switch_count++;
    /* Fusion modes: IMU(0x08), COMPASS(0x09), M4G(0x0A), NDOF_FMC_OFF(0x0B), NDOF(0x0C) */
    m->fusion_active = (new_val >= 0x08 && new_val <= 0x0C);
    if (m->fusion_active) {
        bno055_model_update_euler(m);
    }
    m->hw.regs[BNO055_REG_CALIB_STAT] = m->calib_status;
}

static void bno055_model_on_sys_trigger(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bno055_model_t *m = (bno055_model_t *)user;
    if (new_val & 0x20) {
        /* Reset: restore defaults but keep chip ID */
        hw_model_reset(&m->hw);
        m->fusion_active = false;
    }
}

static void bno055_model_init(bno055_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BNO055_REG_CHIP_ID, 0xA0);
    hw_model_set_readonly(&m->hw, BNO055_REG_CHIP_ID);

    /* Euler data registers (read-only) */
    for (int i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BNO055_REG_EUL_HEAD + i));
    }

    /* Calib status (read-only) */
    hw_model_set_readonly(&m->hw, BNO055_REG_CALIB_STAT);

    /* Callbacks */
    hw_model_set_on_write(&m->hw, BNO055_REG_OPR_MODE, bno055_model_on_opr_mode);
    hw_model_set_on_write(&m->hw, BNO055_REG_SYS_TRIGGER,
                          bno055_model_on_sys_trigger);

    /* Default: heading=180 deg (2880 LSB), roll=0, pitch=0 */
    m->true_heading = 2880;
    m->true_roll    = 0;
    m->true_pitch   = 0;
    m->calib_status = 0xFF;  /* fully calibrated */

    hw_model_reset(&m->hw);
}

static void bno055_model_set_euler(bno055_model_t *m,
                                   int16_t heading, int16_t roll, int16_t pitch)
{
    m->true_heading = heading;
    m->true_roll    = roll;
    m->true_pitch   = pitch;
    if (m->fusion_active) {
        bno055_model_update_euler(m);
    }
}

#endif /* HW_MODEL_BNO055_H */
