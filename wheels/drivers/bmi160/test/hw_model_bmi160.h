// SPDX-License-Identifier: MIT
/*
 * hw_model_bmi160.h — BMI160 hardware register model from datasheet
 *
 * Models Bosch BMI160 register behavior:
 * - Chip ID 0xD1 at register 0x00 (read-only)
 * - PMU_STATUS at 0x03 (read-only, tracks power mode)
 * - Gyro data 0x0C..0x11 (6 bytes, little-endian, read-only)
 * - Accel data 0x12..0x17 (6 bytes, little-endian, read-only)
 * - ACC_RANGE at 0x41, GYR_RANGE at 0x43
 * - CMD at 0x7E: 0x11=acc_normal, 0x15=gyr_normal, 0xB6=soft_reset
 * - A running command occupies CMD for its Table 24 maximum. Writes in
 *   that time are dropped and set drop_cmd_err, except softreset (p.85).
 *   Only the delay callback lets the time pass.
 */

#ifndef HW_MODEL_BMI160_H
#define HW_MODEL_BMI160_H

#include "hw_model.h"

/* --- BMI160 register addresses --- */
#define BMI160_REG_CHIP_ID      0x00
#define BMI160_REG_ERR          0x02
#define BMI160_REG_PMU_STATUS   0x03
#define BMI160_REG_GYR_DATA_XL  0x0C
#define BMI160_REG_ACC_DATA_XL  0x12
#define BMI160_REG_ACC_RANGE    0x41
#define BMI160_REG_GYR_RANGE    0x43
#define BMI160_REG_CMD          0x7E

/* POR defaults */
#define BMI160_POR_CHIP_ID      0xD1
#define BMI160_POR_ACC_RANGE    0x03   /* +/-2g */
#define BMI160_POR_GYR_RANGE    0x00   /* +/-2000 dps */
#define BMI160_POR_PMU_STATUS   0x00   /* all sensors suspended */

/* PMU_STATUS bit positions: acc_pmu[5:4], gyr_pmu[3:2] */
#define PMU_ACC_SHIFT 4
#define PMU_GYR_SHIFT 2
#define PMU_NORMAL    0x01  /* normal mode value per field */

/* ERR_REG bit 6 (p.50) */
#define ERR_DROP_CMD  0x40u

/* Table 24 (p.86): maximum time a command occupies CMD. The footnote's
 * extra 0.3 ms from all-suspend is left out: see Suspected in the README. */
#define CMD_BUSY_ACC_US   3800u
#define CMD_BUSY_GYR_US  80000u

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    /* "true" sensor values as 16-bit signed raw */
    int16_t          true_ax, true_ay, true_az;
    int16_t          true_gx, true_gy, true_gz;
    int              reset_count;
    uint32_t         cmd_busy_us;   /* time left before CMD is free */
} bmi160_model_t;

/* Write accel+gyro raw values into data registers (little-endian) */
static void bmi160_model_set_data_regs(bmi160_model_t *m)
{
    /* Gyro: 0x0C..0x11 (little-endian) */
    m->hw.regs[0x0C] = (uint8_t)((uint16_t)m->true_gx & 0xFF);
    m->hw.regs[0x0D] = (uint8_t)((uint16_t)m->true_gx >> 8);
    m->hw.regs[0x0E] = (uint8_t)((uint16_t)m->true_gy & 0xFF);
    m->hw.regs[0x0F] = (uint8_t)((uint16_t)m->true_gy >> 8);
    m->hw.regs[0x10] = (uint8_t)((uint16_t)m->true_gz & 0xFF);
    m->hw.regs[0x11] = (uint8_t)((uint16_t)m->true_gz >> 8);
    /* Accel: 0x12..0x17 (little-endian) */
    m->hw.regs[0x12] = (uint8_t)((uint16_t)m->true_ax & 0xFF);
    m->hw.regs[0x13] = (uint8_t)((uint16_t)m->true_ax >> 8);
    m->hw.regs[0x14] = (uint8_t)((uint16_t)m->true_ay & 0xFF);
    m->hw.regs[0x15] = (uint8_t)((uint16_t)m->true_ay >> 8);
    m->hw.regs[0x16] = (uint8_t)((uint16_t)m->true_az & 0xFF);
    m->hw.regs[0x17] = (uint8_t)((uint16_t)m->true_az >> 8);
}

/* Callback: CMD register write — handle soft reset and power mode commands */
static void bmi160_model_on_cmd(void *user, uint8_t reg,
                                uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bmi160_model_t *m = (bmi160_model_t *)user;

    if (m->cmd_busy_us > 0 && new_val != 0xB6) {
        /* p.85: dropped while the last command occupies CMD */
        m->hw.regs[BMI160_REG_ERR] =
            (uint8_t)(m->hw.regs[BMI160_REG_ERR] | ERR_DROP_CMD);
        m->hw.regs[BMI160_REG_CMD] = 0x00;
        return;
    }

    if (new_val == 0xB6) {
        /* Soft reset: restore all registers to POR defaults */
        hw_model_reset(&m->hw);
        bmi160_model_set_data_regs(m);
        m->reset_count++;
        m->cmd_busy_us = 0;
    } else if (new_val == 0x11) {
        /* ACC set to normal mode: PMU_STATUS acc_pmu[5:4] = 01 */
        uint8_t pmu = m->hw.regs[BMI160_REG_PMU_STATUS];
        pmu = (uint8_t)((pmu & ~(0x03u << PMU_ACC_SHIFT))
                        | (PMU_NORMAL << PMU_ACC_SHIFT));
        m->hw.regs[BMI160_REG_PMU_STATUS] = pmu;
        m->cmd_busy_us = CMD_BUSY_ACC_US;
    } else if (new_val == 0x15) {
        /* GYR set to normal mode: PMU_STATUS gyr_pmu[3:2] = 01 */
        uint8_t pmu = m->hw.regs[BMI160_REG_PMU_STATUS];
        pmu = (uint8_t)((pmu & ~(0x03u << PMU_GYR_SHIFT))
                        | (PMU_NORMAL << PMU_GYR_SHIFT));
        m->hw.regs[BMI160_REG_PMU_STATUS] = pmu;
        m->cmd_busy_us = CMD_BUSY_GYR_US;
    }
    /* CMD register always reads 0x00 after any write */
    m->hw.regs[BMI160_REG_CMD] = 0x00;
}

static void bmi160_model_init(bmi160_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BMI160_REG_CHIP_ID, BMI160_POR_CHIP_ID);
    hw_model_set_readonly(&m->hw, BMI160_REG_CHIP_ID);

    /* PMU_STATUS (read-only, managed by CMD callback) */
    hw_model_set_reset_value(&m->hw, BMI160_REG_PMU_STATUS, BMI160_POR_PMU_STATUS);
    hw_model_set_readonly(&m->hw, BMI160_REG_PMU_STATUS);

    /* Data registers: read-only */
    for (int i = 0x0C; i <= 0x17; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)i);
    }

    /* ACC_RANGE POR default: 0x03 (+/-2g) */
    hw_model_set_reset_value(&m->hw, BMI160_REG_ACC_RANGE, BMI160_POR_ACC_RANGE);

    /* GYR_RANGE POR default: 0x00 (+/-2000 dps) */
    hw_model_set_reset_value(&m->hw, BMI160_REG_GYR_RANGE, BMI160_POR_GYR_RANGE);

    /* CMD write callback */
    hw_model_set_on_write(&m->hw, BMI160_REG_CMD, bmi160_model_on_cmd);

    /* Default "true" sensor output: 0g X, 0g Y, +1g Z at 2g range
     * 1g / 0.061 mg/LSB = 16393 raw (approx 16384) */
    m->true_ax = 0;
    m->true_ay = 0;
    m->true_az = 16384;
    m->true_gx = 0;
    m->true_gy = 0;
    m->true_gz = 0;

    hw_model_reset(&m->hw);
    bmi160_model_set_data_regs(m);
}

/* Delay callback: the time passing is what frees CMD */
__attribute__((unused))
static void bmi160_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    bmi160_model_t *m = (bmi160_model_t *)hw->user;
    uint32_t us = ms * 1000u;

    m->cmd_busy_us = (m->cmd_busy_us > us) ? m->cmd_busy_us - us : 0u;
    hw_model_delay_ms(ctx, ms);
}

__attribute__((unused))
static void bmi160_model_set_accel(bmi160_model_t *m,
                                   int16_t ax, int16_t ay, int16_t az)
{
    m->true_ax = ax;
    m->true_ay = ay;
    m->true_az = az;
    bmi160_model_set_data_regs(m);
}

__attribute__((unused))
static void bmi160_model_set_gyro(bmi160_model_t *m,
                                  int16_t gx, int16_t gy, int16_t gz)
{
    m->true_gx = gx;
    m->true_gy = gy;
    m->true_gz = gz;
    bmi160_model_set_data_regs(m);
}

#endif /* HW_MODEL_BMI160_H */
