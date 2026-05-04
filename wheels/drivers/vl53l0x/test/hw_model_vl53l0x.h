/*
 * hw_model_vl53l0x.h — VL53L0X hardware register model from datasheet
 *
 * Models VL53L0X register behavior:
 * - Model ID 0xEE at register 0xC0 (read-only)
 * - SYSRANGE_START at 0x00: write 0x01 to start single-shot
 * - SYSTEM_SEQUENCE_CONFIG at 0x01
 * - SYSTEM_INTERRUPT_CLEAR at 0x0B
 * - RESULT_RANGE_STATUS at 0x14: bit 0 = ready flag (read-only)
 * - Range data at 0x1E-0x1F: 16-bit big-endian millimeters (read-only)
 * - MSRC_CONFIG_CONTROL at 0x60
 * - FINAL_RANGE_CONFIG_TIMEOUT at 0x71-0x72
 * - Single-shot: write 0x01 to reg 0x00, ready bit set, range populated
 */

#ifndef HW_MODEL_VL53L0X_H
#define HW_MODEL_VL53L0X_H

#include "hw_model.h"

/* --- VL53L0X register addresses --- */
#define VL_REG_SYSRANGE_START          0x00u
#define VL_REG_SYSTEM_SEQUENCE_CONFIG  0x01u
#define VL_REG_SYSTEM_INTERRUPT_CLEAR  0x0Bu
#define VL_REG_RESULT_RANGE_STATUS     0x14u
#define VL_REG_RANGE_DATA_HI          0x1Eu  /* 0x14 + 10 */
#define VL_REG_RANGE_DATA_LO          0x1Fu  /* 0x14 + 11 */
#define VL_REG_MSRC_CONFIG_CONTROL    0x60u
#define VL_REG_TIMEOUT_HI             0x71u
#define VL_REG_TIMEOUT_LO             0x72u
#define VL_REG_MODEL_ID               0xC0u

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_range_mm;  /* simulated distance */
    int             measure_count;
} vl53l0x_model_t;

/* Callback: SYSRANGE_START write — start measurement */
static void vl53l0x_model_on_sysrange(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    vl53l0x_model_t *m = (vl53l0x_model_t *)user;
    if (new_val & 0x01u) {
        /* Populate range data (big-endian at 0x1E-0x1F) */
        m->hw.regs[VL_REG_RANGE_DATA_HI] =
            (uint8_t)(m->true_range_mm >> 8);
        m->hw.regs[VL_REG_RANGE_DATA_LO] =
            (uint8_t)(m->true_range_mm & 0xFF);
        /* Set ready flag in RESULT_RANGE_STATUS bit 0 */
        m->hw.regs[VL_REG_RESULT_RANGE_STATUS] |= 0x01u;
        m->measure_count++;
    }
}

static void vl53l0x_model_init(vl53l0x_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Model ID: 0xEE at 0xC0 (read-only) */
    hw_model_set_reset_value(&m->hw, VL_REG_MODEL_ID, 0xEEu);
    hw_model_set_readonly(&m->hw, VL_REG_MODEL_ID);

    /* RESULT_RANGE_STATUS (read-only) */
    hw_model_set_readonly(&m->hw, VL_REG_RESULT_RANGE_STATUS);

    /* Range data registers (read-only, written by measurement engine) */
    hw_model_set_readonly(&m->hw, VL_REG_RANGE_DATA_HI);
    hw_model_set_readonly(&m->hw, VL_REG_RANGE_DATA_LO);

    /* Write callback for SYSRANGE_START */
    hw_model_set_on_write(&m->hw, VL_REG_SYSRANGE_START,
                          vl53l0x_model_on_sysrange);

    /* Default simulated range: 250 mm */
    m->true_range_mm = 250;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_VL53L0X_H */
