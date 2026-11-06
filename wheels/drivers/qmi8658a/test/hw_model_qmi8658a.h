// SPDX-License-Identifier: MIT
/*
 * hw_model_qmi8658a.h — QMI8658A hardware register model
 *
 * Models QST QMI8658A register behavior:
 * - WHO_AM_I 0x05 at register 0x00 (read-only)
 * - CTRL1 at 0x02, reset value 0x20: bit 6 ADDR_AI and bit 5 BE decide what a
 *   burst read returns
 * - CTRL2 at 0x03 (accel range + ODR)
 * - CTRL3 at 0x04 (gyro range + ODR)
 * - CTRL7 at 0x08 (accel/gyro enable)
 * - Soft reset via register 0x60 = 0xB0
 * - Accel+Gyro data at 0x35..0x40 (12 bytes, read-only), which change only
 *   while the sensor is enabled
 *
 * Reads go through qmi8658a_model_read_regs for CTRL1 to count. A sensor gives
 * data at once when enabled: the turn-on time and the 15 ms of a reset are not
 * modelled, because the driver waits for neither and is held back.
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
#define QMI_REG_GZ_H      0x40
#define QMI_REG_RESET     0x60

#define QMI_CTRL1_ADDR_AI 0x40
#define QMI_CTRL1_BE      0x20
#define QMI_CTRL7_AEN     0x01
#define QMI_CTRL7_GEN     0x02

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int16_t          true_ax, true_ay, true_az;
    int16_t          true_gx, true_gy, true_gz;
    int              reset_count;
} qmi8658a_model_t;

/* Write accel+gyro data into registers (L byte first). Table 31 p.50: a sensor
 * that is not enabled measures nothing, so its registers stay as they are. */
static void qmi8658a_model_set_data_regs(qmi8658a_model_t *m)
{
    uint8_t ctrl7 = m->hw.regs[QMI_REG_CTRL7];

    /* Accel: 0x35..0x3A (AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H) */
    if (ctrl7 & QMI_CTRL7_AEN) {
        m->hw.regs[0x35] = (uint8_t)((uint16_t)m->true_ax & 0xFF);
        m->hw.regs[0x36] = (uint8_t)((uint16_t)m->true_ax >> 8);
        m->hw.regs[0x37] = (uint8_t)((uint16_t)m->true_ay & 0xFF);
        m->hw.regs[0x38] = (uint8_t)((uint16_t)m->true_ay >> 8);
        m->hw.regs[0x39] = (uint8_t)((uint16_t)m->true_az & 0xFF);
        m->hw.regs[0x3A] = (uint8_t)((uint16_t)m->true_az >> 8);
    }
    /* Gyro: 0x3B..0x40 */
    if (ctrl7 & QMI_CTRL7_GEN) {
        m->hw.regs[0x3B] = (uint8_t)((uint16_t)m->true_gx & 0xFF);
        m->hw.regs[0x3C] = (uint8_t)((uint16_t)m->true_gx >> 8);
        m->hw.regs[0x3D] = (uint8_t)((uint16_t)m->true_gy & 0xFF);
        m->hw.regs[0x3E] = (uint8_t)((uint16_t)m->true_gy >> 8);
        m->hw.regs[0x3F] = (uint8_t)((uint16_t)m->true_gz & 0xFF);
        m->hw.regs[0x40] = (uint8_t)((uint16_t)m->true_gz >> 8);
    }
}

/* Callback: a sensor enabled through CTRL7 starts to give data */
static void qmi8658a_model_on_ctrl7(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    qmi8658a_model_set_data_regs((qmi8658a_model_t *)user);
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

    /* Table 22 p.31: only BE is 1 at reset */
    hw_model_set_reset_value(&m->hw, QMI_REG_CTRL1, QMI_CTRL1_BE);

    /* Reset callback */
    hw_model_set_on_write(&m->hw, QMI_REG_RESET, qmi8658a_model_on_reset);
    hw_model_set_on_write(&m->hw, QMI_REG_CTRL7, qmi8658a_model_on_ctrl7);

    /* Default: 0g X, 0g Y, +1g Z at 8g range (4096 LSB/g) */
    m->true_ax = 0;
    m->true_ay = 0;
    m->true_az = 4096;
    m->true_gx = 0;
    m->true_gy = 0;
    m->true_gz = 0;

    hw_model_reset(&m->hw);
}

/*
 * HAL read. p.76: with ADDR_AI 0 "the register address will not increase". With
 * BE 1 a 16-bit data value comes high byte first, Table 22 p.31.
 */
__attribute__((unused))
static bool qmi8658a_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(hw)) return false;

    uint8_t ctrl1 = hw->regs[QMI_REG_CTRL1];
    for (uint16_t i = 0; i < len; i++) {
        unsigned at = (ctrl1 & QMI_CTRL1_ADDR_AI) ? (unsigned)reg + i : reg;
        if ((ctrl1 & QMI_CTRL1_BE) && at >= QMI_REG_AX_L && at <= QMI_REG_GZ_H)
            at = QMI_REG_AX_L + ((at - QMI_REG_AX_L) ^ 1u);
        buf[i] = hw->regs[at & 0xFFu];
    }
    return true;
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
