/*
 * hw_model_qmi8658a.h — QMI8658A hardware register model
 *
 * Models QST QMI8658A register behavior:
 * - WHO_AM_I 0x05 at register 0x00 (read-only)
 * - CTRL1 at 0x02 (SPI/address auto-increment)
 * - CTRL2 at 0x03 (accel range + ODR)
 * - CTRL3 at 0x04 (gyro range + ODR)
 * - CTRL7 at 0x08 (accel/gyro enable)
 * - Soft reset via register 0x60 = 0xB0
 * - Accel+Gyro data at 0x35..0x40 (12 bytes, read-only, little-endian)
 */

#ifndef HW_MODEL_QMI8658A_H
#define HW_MODEL_QMI8658A_H

#include "hw_model.h"

/* --- QMI8658A register addresses --- */
#define QMI_REG_WHO_AM_I  0x00
#define QMI_REG_CTRL1     0x02
#define QMI_REG_CTRL2     0x03
#define QMI_REG_CTRL3     0x04
#define QMI_REG_CTRL7     0x08
#define QMI_REG_AX_L      0x35
#define QMI_REG_RESET     0x60

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_ax, true_ay, true_az;
    int16_t          true_gx, true_gy, true_gz;
    int              reset_count;
} qmi8658a_model_t;

/* Write accel+gyro data into registers (little-endian) */
static void qmi8658a_model_set_data_regs(qmi8658a_model_t *m)
{
    /* Accel: 0x35..0x3A (AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H) */
    m->hw.regs[0x35] = (uint8_t)((uint16_t)m->true_ax & 0xFF);
    m->hw.regs[0x36] = (uint8_t)((uint16_t)m->true_ax >> 8);
    m->hw.regs[0x37] = (uint8_t)((uint16_t)m->true_ay & 0xFF);
    m->hw.regs[0x38] = (uint8_t)((uint16_t)m->true_ay >> 8);
    m->hw.regs[0x39] = (uint8_t)((uint16_t)m->true_az & 0xFF);
    m->hw.regs[0x3A] = (uint8_t)((uint16_t)m->true_az >> 8);
    /* Gyro: 0x3B..0x40 */
    m->hw.regs[0x3B] = (uint8_t)((uint16_t)m->true_gx & 0xFF);
    m->hw.regs[0x3C] = (uint8_t)((uint16_t)m->true_gx >> 8);
    m->hw.regs[0x3D] = (uint8_t)((uint16_t)m->true_gy & 0xFF);
    m->hw.regs[0x3E] = (uint8_t)((uint16_t)m->true_gy >> 8);
    m->hw.regs[0x3F] = (uint8_t)((uint16_t)m->true_gz & 0xFF);
    m->hw.regs[0x40] = (uint8_t)((uint16_t)m->true_gz >> 8);
}

/* Callback: reset register write */
static void qmi8658a_model_on_reset(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    qmi8658a_model_t *m = (qmi8658a_model_t *)user;
    if (new_val == 0xB0) {
        hw_model_reset(&m->hw);
        m->reset_count++;
    }
}

static void qmi8658a_model_init(qmi8658a_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* WHO_AM_I (read-only) */
    hw_model_set_reset_value(&m->hw, QMI_REG_WHO_AM_I, 0x05);
    hw_model_set_readonly(&m->hw, QMI_REG_WHO_AM_I);

    /* Data registers 0x35..0x40 (read-only) */
    for (int i = 0x35; i <= 0x40; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)i);
    }

    /* Reset callback */
    hw_model_set_on_write(&m->hw, QMI_REG_RESET, qmi8658a_model_on_reset);

    /* Default: 0g X, 0g Y, +1g Z at 8g range (4096 LSB/g) */
    m->true_ax = 0;
    m->true_ay = 0;
    m->true_az = 4096;
    m->true_gx = 0;
    m->true_gy = 0;
    m->true_gz = 0;

    hw_model_reset(&m->hw);
    qmi8658a_model_set_data_regs(m);
}

static void qmi8658a_model_set_accel(qmi8658a_model_t *m,
                                      int16_t ax, int16_t ay, int16_t az)
{
    m->true_ax = ax;
    m->true_ay = ay;
    m->true_az = az;
    qmi8658a_model_set_data_regs(m);
}

static void qmi8658a_model_set_gyro(qmi8658a_model_t *m,
                                     int16_t gx, int16_t gy, int16_t gz)
{
    m->true_gx = gx;
    m->true_gy = gy;
    m->true_gz = gz;
    qmi8658a_model_set_data_regs(m);
}

#endif /* HW_MODEL_QMI8658A_H */
