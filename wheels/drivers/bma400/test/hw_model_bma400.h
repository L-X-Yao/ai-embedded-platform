// SPDX-License-Identifier: MIT
/*
 * hw_model_bma400.h — datasheet-accurate register model for BMA400
 *
 * Models Bosch BMA400 register behavior (BST-BMA400-DS000-02):
 * - Chip ID 0x90 at register 0x00 (read-only)
 * - Accel data at 0x04..0x09 (read-only, 12-bit signed, little-endian)
 * - ACC_CONFIG0 at 0x19 (power mode bits [1:0])
 * - ACC_CONFIG1 at 0x1A (range bits [7:6])
 *
 * Provides inject helpers for physical acceleration values in milli-g.
 */

#ifndef HW_MODEL_BMA400_H
#define HW_MODEL_BMA400_H

#include "hw_model.h"

/* --- BMA400 register addresses --- */
#define BMA400_REG_CHIP_ID     0x00u
#define BMA400_REG_ACC_X_LSB   0x04u
#define BMA400_REG_ACC_X_MSB   0x05u
#define BMA400_REG_ACC_Y_LSB   0x06u
#define BMA400_REG_ACC_Y_MSB   0x07u
#define BMA400_REG_ACC_Z_LSB   0x08u
#define BMA400_REG_ACC_Z_MSB   0x09u
#define BMA400_REG_ACC_CONFIG0 0x19u
#define BMA400_REG_ACC_CONFIG1 0x1Au

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_x, true_y, true_z;  /* 12-bit signed raw values */
} bma400_model_t;

/* Write 12-bit signed raw accel values into data registers (little-endian) */
static void bma400_model_set_data_regs(bma400_model_t *m)
{
    /* X axis: lo byte = raw[7:0], hi byte = raw[11:8] in bits [3:0] */
    m->hw.regs[BMA400_REG_ACC_X_LSB] = (uint8_t)((uint16_t)m->true_x & 0xFFu);
    m->hw.regs[BMA400_REG_ACC_X_MSB] = (uint8_t)(((uint16_t)m->true_x >> 8) & 0x0Fu);

    /* Y axis */
    m->hw.regs[BMA400_REG_ACC_Y_LSB] = (uint8_t)((uint16_t)m->true_y & 0xFFu);
    m->hw.regs[BMA400_REG_ACC_Y_MSB] = (uint8_t)(((uint16_t)m->true_y >> 8) & 0x0Fu);

    /* Z axis */
    m->hw.regs[BMA400_REG_ACC_Z_LSB] = (uint8_t)((uint16_t)m->true_z & 0xFFu);
    m->hw.regs[BMA400_REG_ACC_Z_MSB] = (uint8_t)(((uint16_t)m->true_z >> 8) & 0x0Fu);
}

static void bma400_model_init(bma400_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BMA400_REG_CHIP_ID, 0x90);
    hw_model_set_readonly(&m->hw, BMA400_REG_CHIP_ID);

    /* Accel data registers (read-only, 6 bytes) */
    for (unsigned i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BMA400_REG_ACC_X_LSB + i));
    }

    /* ACC_CONFIG0: only bits [1:0] are power_mode */
    hw_model_set_writable_mask(&m->hw, BMA400_REG_ACC_CONFIG0, 0x03);

    /* ACC_CONFIG1: range, OSR and ODR, all writable. POR is 4g at 200 Hz. */
    hw_model_set_reset_value(&m->hw, BMA400_REG_ACC_CONFIG1, 0x49);

    /* Default "true" sensor output: 0g X, 0g Y, +1g Z
     * At +/-2g range, sensitivity = 1024 LSB/g, so 1g = 1024 raw. */
    m->true_x = 0;
    m->true_y = 0;
    m->true_z = 1024;

    hw_model_reset(&m->hw);
    bma400_model_set_data_regs(m);
}

/* Set raw 12-bit signed accel values and update data registers */
static void bma400_model_set_accel_raw(bma400_model_t *m,
                                       int16_t x, int16_t y, int16_t z)
{
    m->true_x = x;
    m->true_y = y;
    m->true_z = z;
    bma400_model_set_data_regs(m);
}

/*
 * Inject acceleration in milli-g based on current range setting.
 * Sensitivity (LSB/g): 2g=1024, 4g=512, 8g=256, 16g=128.
 */
static void bma400_model_inject_accel_mg(bma400_model_t *m,
                                         float x_mg, float y_mg, float z_mg)
{
    /* Read range from ACC_CONFIG1 bits [7:6] */
    uint8_t range_bits = (m->hw.regs[BMA400_REG_ACC_CONFIG1] >> 6) & 0x03u;
    static const float sens[] = { 1024.0f, 512.0f, 256.0f, 128.0f };
    float s = sens[range_bits];

    /* mg to raw: raw = mg / 1000 * sensitivity */
    m->true_x = (int16_t)(x_mg / 1000.0f * s);
    m->true_y = (int16_t)(y_mg / 1000.0f * s);
    m->true_z = (int16_t)(z_mg / 1000.0f * s);
    bma400_model_set_data_regs(m);
}

#endif /* HW_MODEL_BMA400_H */
