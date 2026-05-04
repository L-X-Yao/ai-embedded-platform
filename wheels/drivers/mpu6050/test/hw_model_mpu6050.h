/*
 * hw_model_mpu6050.h — MPU-6050 hardware register model from datasheet
 *
 * Models InvenSense MPU-6050 register behavior:
 * - WHO_AM_I 0x68 at register 0x75 (read-only)
 * - PWR_MGMT_1 at 0x6B (bit 7 = device reset)
 * - ACCEL_CONFIG at 0x1C, GYRO_CONFIG at 0x1B
 * - ACCEL data 0x3B..0x40 (6 bytes, read-only, big-endian)
 * - GYRO  data 0x43..0x48 (6 bytes, read-only, big-endian)
 * - TEMP  data 0x41..0x42 (2 bytes, read-only)
 */

#ifndef HW_MODEL_MPU6050_H
#define HW_MODEL_MPU6050_H

#include "hw_model.h"

/* --- MPU6050 register addresses --- */
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H   0x41
#define MPU6050_REG_GYRO_XOUT_H  0x43
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_PWR_MGMT_2   0x6C
#define MPU6050_REG_WHO_AM_I     0x75

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    /* "true" sensor values as 16-bit signed raw */
    int16_t          true_ax, true_ay, true_az;
    int16_t          true_gx, true_gy, true_gz;
    int16_t          true_temp;
    int              reset_count;
} mpu6050_model_t;

/* Write accel+temp+gyro raw values into data registers (big-endian) */
static void mpu6050_model_set_data_regs(mpu6050_model_t *m)
{
    /* Accel: 0x3B..0x40 */
    m->hw.regs[0x3B] = (uint8_t)((uint16_t)m->true_ax >> 8);
    m->hw.regs[0x3C] = (uint8_t)((uint16_t)m->true_ax & 0xFF);
    m->hw.regs[0x3D] = (uint8_t)((uint16_t)m->true_ay >> 8);
    m->hw.regs[0x3E] = (uint8_t)((uint16_t)m->true_ay & 0xFF);
    m->hw.regs[0x3F] = (uint8_t)((uint16_t)m->true_az >> 8);
    m->hw.regs[0x40] = (uint8_t)((uint16_t)m->true_az & 0xFF);
    /* Temp: 0x41..0x42 */
    m->hw.regs[0x41] = (uint8_t)((uint16_t)m->true_temp >> 8);
    m->hw.regs[0x42] = (uint8_t)((uint16_t)m->true_temp & 0xFF);
    /* Gyro: 0x43..0x48 */
    m->hw.regs[0x43] = (uint8_t)((uint16_t)m->true_gx >> 8);
    m->hw.regs[0x44] = (uint8_t)((uint16_t)m->true_gx & 0xFF);
    m->hw.regs[0x45] = (uint8_t)((uint16_t)m->true_gy >> 8);
    m->hw.regs[0x46] = (uint8_t)((uint16_t)m->true_gy & 0xFF);
    m->hw.regs[0x47] = (uint8_t)((uint16_t)m->true_gz >> 8);
    m->hw.regs[0x48] = (uint8_t)((uint16_t)m->true_gz & 0xFF);
}

/* Callback: PWR_MGMT_1 write — handle device reset (bit 7) */
static void mpu6050_model_on_pwr_mgmt1(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    mpu6050_model_t *m = (mpu6050_model_t *)user;
    if (new_val & 0x80) {
        /* Device reset: restore defaults, but keep WHO_AM_I */
        hw_model_reset(&m->hw);
        /* After reset, PWR_MGMT_1 = 0x40 (SLEEP bit set) per datasheet */
        m->hw.regs[MPU6050_REG_PWR_MGMT_1] = 0x40;
        m->reset_count++;
    }
}

static void mpu6050_model_init(mpu6050_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* WHO_AM_I (read-only, bits [6:1] = device ID, bit 0 and 7 reserved) */
    hw_model_set_reset_value(&m->hw, MPU6050_REG_WHO_AM_I, 0x68);
    hw_model_set_readonly(&m->hw, MPU6050_REG_WHO_AM_I);

    /* Data output registers: accel (0x3B..0x40), temp (0x41..0x42), gyro (0x43..0x48) */
    for (int i = 0x3B; i <= 0x48; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)i);
    }

    /* PWR_MGMT_1 default after POR: 0x40 (SLEEP=1) */
    hw_model_set_reset_value(&m->hw, MPU6050_REG_PWR_MGMT_1, 0x40);

    /* Write callback for reset detection */
    hw_model_set_on_write(&m->hw, MPU6050_REG_PWR_MGMT_1, mpu6050_model_on_pwr_mgmt1);

    /* Default "true" sensor output: 0g X, 0g Y, +1g Z (at 2g range: 16384 LSB/g) */
    m->true_ax = 0;
    m->true_ay = 0;
    m->true_az = 16384;    /* +1g */
    m->true_gx = 0;
    m->true_gy = 0;
    m->true_gz = 0;
    m->true_temp = 0;      /* ~36.53C */

    hw_model_reset(&m->hw);
    mpu6050_model_set_data_regs(m);
}

static void mpu6050_model_set_accel(mpu6050_model_t *m,
                                     int16_t ax, int16_t ay, int16_t az)
{
    m->true_ax = ax;
    m->true_ay = ay;
    m->true_az = az;
    mpu6050_model_set_data_regs(m);
}

static void mpu6050_model_set_gyro(mpu6050_model_t *m,
                                    int16_t gx, int16_t gy, int16_t gz)
{
    m->true_gx = gx;
    m->true_gy = gy;
    m->true_gz = gz;
    mpu6050_model_set_data_regs(m);
}

#endif /* HW_MODEL_MPU6050_H */
