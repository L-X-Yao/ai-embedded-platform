// SPDX-License-Identifier: MIT
/*
 * hw_model_as5600.h — AS5600 hardware register model from datasheet
 *
 * Models ams AS5600 register behavior:
 * - Fixed I2C address 0x36
 * - Status register 0x0B (read-only, magnet status bits)
 * - Raw angle 0x0C-0x0D (read-only, 12-bit, set from "true" angle)
 * - ZPOS 0x01-0x02, MANG 0x05-0x06 (r/w start position / max angle)
 * - CONF 0x07-0x08 (r/w configuration)
 */

#ifndef HW_MODEL_AS5600_H
#define HW_MODEL_AS5600_H

#include "hw_model.h"

/* --- AS5600 register addresses --- */
#define AS5600_REG_ZPOS_H     0x01u
#define AS5600_REG_ZPOS_L     0x02u
#define AS5600_REG_MPOS_H     0x03u
#define AS5600_REG_MPOS_L     0x04u
#define AS5600_REG_MANG_H     0x05u
#define AS5600_REG_MANG_L     0x06u
#define AS5600_REG_CONF_H     0x07u
#define AS5600_REG_CONF_L     0x08u
#define AS5600_REG_STATUS     0x0Bu
#define AS5600_REG_RAW_ANG_H  0x0Cu
#define AS5600_REG_RAW_ANG_L  0x0Du
#define AS5600_REG_ANGLE_H    0x0Eu
#define AS5600_REG_ANGLE_L    0x0Fu
#define AS5600_REG_AGC        0x1Au
#define AS5600_REG_MAGNITUDE_H 0x1Bu
#define AS5600_REG_MAGNITUDE_L 0x1Cu

/* Status bit masks */
#define AS5600_STATUS_MH      0x08u  /* magnet too strong */
#define AS5600_STATUS_ML      0x10u  /* magnet too weak   */
#define AS5600_STATUS_MD      0x20u  /* magnet detected   */

/* --- Simulated environment --- */
typedef struct {
    hw_reg_model_t   hw;
    uint16_t         true_angle;    /* raw 12-bit angle (0-4095) */
} as5600_model_t;

/* Update raw angle registers from true_angle */
static void as5600_model_update_angle(as5600_model_t *m)
{
    uint16_t a = m->true_angle & 0x0FFFu;
    m->hw.regs[AS5600_REG_RAW_ANG_H] = (uint8_t)(a >> 8);
    m->hw.regs[AS5600_REG_RAW_ANG_L] = (uint8_t)(a & 0xFF);
    /* Processed angle same as raw when no ZPOS/MANG configured */
    m->hw.regs[AS5600_REG_ANGLE_H] = (uint8_t)(a >> 8);
    m->hw.regs[AS5600_REG_ANGLE_L] = (uint8_t)(a & 0xFF);
}

static void as5600_model_init(as5600_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Status register (read-only) — default: magnet detected */
    hw_model_set_reset_value(&m->hw, AS5600_REG_STATUS, AS5600_STATUS_MD);
    hw_model_set_readonly(&m->hw, AS5600_REG_STATUS);

    /* Raw angle registers (read-only, set by model) */
    hw_model_set_readonly(&m->hw, AS5600_REG_RAW_ANG_H);
    hw_model_set_readonly(&m->hw, AS5600_REG_RAW_ANG_L);

    /* Processed angle registers (read-only) */
    hw_model_set_readonly(&m->hw, AS5600_REG_ANGLE_H);
    hw_model_set_readonly(&m->hw, AS5600_REG_ANGLE_L);

    /* AGC and magnitude (read-only) */
    hw_model_set_readonly(&m->hw, AS5600_REG_AGC);
    hw_model_set_readonly(&m->hw, AS5600_REG_MAGNITUDE_H);
    hw_model_set_readonly(&m->hw, AS5600_REG_MAGNITUDE_L);
    hw_model_set_reset_value(&m->hw, AS5600_REG_AGC, 0x80);

    /* ZPOS, MPOS, MANG, CONF: writable (12-bit values, upper nibble masked) */
    hw_model_set_writable_mask(&m->hw, AS5600_REG_ZPOS_H, 0x0F);
    hw_model_set_writable_mask(&m->hw, AS5600_REG_MPOS_H, 0x0F);
    hw_model_set_writable_mask(&m->hw, AS5600_REG_MANG_H, 0x0F);

    /* Default angle: 180 degrees = 2048 */
    m->true_angle = 2048;

    hw_model_reset(&m->hw);
    as5600_model_update_angle(m);
}

/* Helper: set simulated angle (0-4095 maps to 0-360 degrees) */
static void as5600_model_set_angle(as5600_model_t *m, uint16_t raw_12bit)
{
    m->true_angle = raw_12bit & 0x0FFFu;
    as5600_model_update_angle(m);
}

/* Helper: set magnet status bits */
static void as5600_model_set_status(as5600_model_t *m, uint8_t status)
{
    m->hw.regs[AS5600_REG_STATUS] = status;
    m->hw.reset_values[AS5600_REG_STATUS] = status;
}

#endif /* HW_MODEL_AS5600_H */
