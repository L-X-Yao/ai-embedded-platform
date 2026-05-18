// SPDX-License-Identifier: MIT
/*
 * hw_model_adxl345.h — ADXL345 hardware register model from datasheet
 *
 * Models Analog Devices ADXL345 datasheet (Rev. G) register behavior:
 * - Chip ID 0xE5 at 0x00 (read-only)
 * - DATA_FORMAT (0x31) controls range
 * - POWER_CTL (0x2D) controls measurement mode
 * - DATAX0..DATAZ1 (0x32..0x37) read-only, populated when measure enabled
 */

#ifndef HW_MODEL_ADXL345_H
#define HW_MODEL_ADXL345_H

#include "hw_model.h"

/* --- ADXL345 register addresses --- */
#define ADXL345_REG_DEVID        0x00
#define ADXL345_REG_BW_RATE      0x2C
#define ADXL345_REG_POWER_CTL    0x2D
#define ADXL345_REG_DATA_FORMAT  0x31
#define ADXL345_REG_DATAX0       0x32

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    int16_t        true_x;   /* raw accel X (LSB) */
    int16_t        true_y;
    int16_t        true_z;
    int            measure_count;
    bool           measuring;
} adxl345_model_t;

static void adxl345_model_update_data(adxl345_model_t *m)
{
    if (!m->measuring) return;
    m->hw.regs[0x32] = (uint8_t)(m->true_x & 0xFF);
    m->hw.regs[0x33] = (uint8_t)((m->true_x >> 8) & 0xFF);
    m->hw.regs[0x34] = (uint8_t)(m->true_y & 0xFF);
    m->hw.regs[0x35] = (uint8_t)((m->true_y >> 8) & 0xFF);
    m->hw.regs[0x36] = (uint8_t)(m->true_z & 0xFF);
    m->hw.regs[0x37] = (uint8_t)((m->true_z >> 8) & 0xFF);
    m->measure_count++;
}

static void adxl345_model_on_power_ctl(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    adxl345_model_t *m = (adxl345_model_t *)user;
    m->measuring = (new_val & 0x08) != 0;
    if (m->measuring) {
        adxl345_model_update_data(m);
    }
}

static void adxl345_model_init(adxl345_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, ADXL345_REG_DEVID, 0xE5);
    hw_model_set_readonly(&m->hw, ADXL345_REG_DEVID);

    /* Data registers (read-only, written by measurement engine) */
    for (int i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(ADXL345_REG_DATAX0 + i));
    }

    /* Write callback for POWER_CTL */
    hw_model_set_on_write(&m->hw, ADXL345_REG_POWER_CTL,
                          adxl345_model_on_power_ctl);

    /* Default "true" environment: 0g X, 0g Y, +1g Z (256 LSB at 3.9mg/LSB) */
    m->true_x = 0;
    m->true_y = 0;
    m->true_z = 256;

    hw_model_reset(&m->hw);
}

static void adxl345_model_set_accel(adxl345_model_t *m,
                                    int16_t x, int16_t y, int16_t z)
{
    m->true_x = x;
    m->true_y = y;
    m->true_z = z;
    if (m->measuring) {
        adxl345_model_update_data(m);
    }
}

#endif /* HW_MODEL_ADXL345_H */
