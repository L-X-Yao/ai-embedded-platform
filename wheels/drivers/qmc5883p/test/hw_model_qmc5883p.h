/*
 * hw_model_qmc5883p.h — QMC5883P hardware register model from datasheet
 *
 * Models QMC5883P register behavior:
 * - Chip ID 0x80 at register 0x00 (read-only)
 * - Magnetometer XYZ data at 0x01..0x06 (read-only, 16-bit LE per axis)
 * - Status register at 0x09 with DRDY bit 0
 * - Control register 1 at 0x0A (mode/ODR/OSR)
 * - Control register 2 at 0x0B (range/soft reset)
 * - Sign register at 0x29
 * - Wake register at 0x0D
 * - Continuous mode writes ADC data and sets DRDY on CR1 write
 */

#ifndef HW_MODEL_QMC5883P_H
#define HW_MODEL_QMC5883P_H

#include "hw_model.h"

/* --- QMC5883P register addresses --- */
#define QMC5883P_REG_CHIP_ID     0x00u
#define QMC5883P_REG_DATA_X_LSB  0x01u
#define QMC5883P_REG_STATUS      0x09u
#define QMC5883P_REG_CR1         0x0Au
#define QMC5883P_REG_CR2         0x0Bu
#define QMC5883P_REG_WAKE        0x0Du
#define QMC5883P_REG_SIGN        0x29u

#define QMC5883P_CR2_SOFT_RST    0x80u

typedef struct {
    hw_reg_model_t  hw;
    int16_t         true_mx;    /* simulated raw X (LSB counts) */
    int16_t         true_my;    /* simulated raw Y */
    int16_t         true_mz;    /* simulated raw Z */
    int             measure_count;
} qmc5883p_model_t;

/* Populate data registers from true values and set DRDY */
static void qmc5883p_model_set_data(qmc5883p_model_t *m)
{
    m->hw.regs[0x01] = (uint8_t)(m->true_mx & 0xFF);
    m->hw.regs[0x02] = (uint8_t)((uint16_t)m->true_mx >> 8);
    m->hw.regs[0x03] = (uint8_t)(m->true_my & 0xFF);
    m->hw.regs[0x04] = (uint8_t)((uint16_t)m->true_my >> 8);
    m->hw.regs[0x05] = (uint8_t)(m->true_mz & 0xFF);
    m->hw.regs[0x06] = (uint8_t)((uint16_t)m->true_mz >> 8);
    m->hw.regs[QMC5883P_REG_STATUS] |= 0x01u; /* DRDY */
}

/* Callback: CR1 write — entering continuous mode triggers measurement */
static void qmc5883p_model_on_cr1(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    qmc5883p_model_t *m = (qmc5883p_model_t *)user;
    uint8_t mode = new_val & 0x03u;
    if (mode == 0x03u) { /* continuous mode */
        qmc5883p_model_set_data(m);
        m->measure_count++;
    }
}

/* Callback: CR2 write — soft reset */
static void qmc5883p_model_on_cr2(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    qmc5883p_model_t *m = (qmc5883p_model_t *)user;
    if (new_val & QMC5883P_CR2_SOFT_RST) {
        hw_model_reset(&m->hw);
    }
}

static void qmc5883p_model_init(qmc5883p_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID: 0x80 at register 0x00 (read-only) */
    hw_model_set_reset_value(&m->hw, QMC5883P_REG_CHIP_ID, 0x80u);
    hw_model_set_readonly(&m->hw, QMC5883P_REG_CHIP_ID);

    /* Data registers 0x01..0x06 (read-only, written by measurement) */
    for (unsigned i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(QMC5883P_REG_DATA_X_LSB + i));
    }

    /* Status register (read-only) */
    hw_model_set_readonly(&m->hw, QMC5883P_REG_STATUS);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, QMC5883P_REG_CR1, qmc5883p_model_on_cr1);
    hw_model_set_on_write(&m->hw, QMC5883P_REG_CR2, qmc5883p_model_on_cr2);

    /* Default simulated field: ~0.3 Gauss X, ~0.1 Y, ~0.4 Z */
    m->true_mx = 4500;   /* 4500 / 15000 = 0.3 Gauss */
    m->true_my = 1500;   /* 1500 / 15000 = 0.1 Gauss */
    m->true_mz = 6000;   /* 6000 / 15000 = 0.4 Gauss */

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_QMC5883P_H */
