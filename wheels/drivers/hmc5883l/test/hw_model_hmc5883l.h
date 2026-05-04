/*
 * hw_model_hmc5883l.h — HMC5883L hardware register model from datasheet
 *
 * Models HMC5883L register behavior:
 * - ID registers at 0x0A-0x0C: 'H','4','3' (read-only)
 * - Config A at 0x00: data output rate [4:2]
 * - Config B at 0x01: gain [7:5]
 * - Mode at 0x02: measurement mode [1:0]
 * - Data registers 0x03..0x08: X_H,X_L,Z_H,Z_L,Y_H,Y_L (read-only, big-endian)
 * - Continuous mode populates data on mode register write
 */

#ifndef HW_MODEL_HMC5883L_H
#define HW_MODEL_HMC5883L_H

#include "hw_model.h"

/* --- HMC5883L register addresses --- */
#define HMC5883L_REG_CONFIG_A  0x00u
#define HMC5883L_REG_CONFIG_B  0x01u
#define HMC5883L_REG_MODE      0x02u
#define HMC5883L_REG_DATA_X_H  0x03u
#define HMC5883L_REG_ID_A      0x0Au
#define HMC5883L_REG_ID_B      0x0Bu
#define HMC5883L_REG_ID_C      0x0Cu

typedef struct {
    hw_reg_model_t  hw;
    int16_t         true_x;     /* simulated raw X counts */
    int16_t         true_y;     /* simulated raw Y counts */
    int16_t         true_z;     /* simulated raw Z counts */
    int             measure_count;
} hmc5883l_model_t;

/* Populate data registers from true values (big-endian: X, Z, Y order) */
static void hmc5883l_model_set_data(hmc5883l_model_t *m)
{
    m->hw.regs[0x03] = (uint8_t)((uint16_t)m->true_x >> 8);
    m->hw.regs[0x04] = (uint8_t)(m->true_x & 0xFF);
    m->hw.regs[0x05] = (uint8_t)((uint16_t)m->true_z >> 8);
    m->hw.regs[0x06] = (uint8_t)(m->true_z & 0xFF);
    m->hw.regs[0x07] = (uint8_t)((uint16_t)m->true_y >> 8);
    m->hw.regs[0x08] = (uint8_t)(m->true_y & 0xFF);
}

/* Callback: mode register write — continuous mode (0x00) triggers data */
static void hmc5883l_model_on_mode(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    hmc5883l_model_t *m = (hmc5883l_model_t *)user;
    uint8_t mode = new_val & 0x03u;
    if (mode == 0x00u) { /* continuous measurement */
        hmc5883l_model_set_data(m);
        m->measure_count++;
    }
}

static void hmc5883l_model_init(hmc5883l_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* ID registers (read-only): 'H', '4', '3' */
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_A, 'H');
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_B, '4');
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_C, '3');
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_A);
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_B);
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_C);

    /* Data registers 0x03..0x08 (read-only, written by measurement engine) */
    for (int i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(HMC5883L_REG_DATA_X_H + i));
    }

    /* Config A: only bits [6:0] writable (bit 7 reserved, always 0) */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_CONFIG_A, 0x7Fu);

    /* Config B: only bits [7:5] writable (gain) */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_CONFIG_B, 0xE0u);

    /* Mode: only bits [1:0] writable */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_MODE, 0x03u);

    /* Write callback for mode register */
    hw_model_set_on_write(&m->hw, HMC5883L_REG_MODE, hmc5883l_model_on_mode);

    /* Default simulated field: ~500 counts each axis at gain 1090 */
    m->true_x = 500;
    m->true_y = -200;
    m->true_z = 350;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_HMC5883L_H */
