// SPDX-License-Identifier: MIT
/*
 * mpu6050 — InvenSense MPU-6050 6-axis IMU driver (accel + gyro)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads 3-axis accelerometer and
 * 3-axis gyroscope with configurable full-scale ranges.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MPU6050_OK        =  0,
    MPU6050_ERR_I2C   = -1,
    MPU6050_ERR_ID    = -2,
    MPU6050_ERR_PARAM = -3,
    MPU6050_ERR_TIMEOUT = -4,
} mpu6050_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional: see mpu6050_reset */
    void *ctx;
} mpu6050_i2c_t;

typedef enum {
    MPU6050_ACCEL_2G  = 0,
    MPU6050_ACCEL_4G  = 1,
    MPU6050_ACCEL_8G  = 2,
    MPU6050_ACCEL_16G = 3,
} mpu6050_accel_range_t;

typedef enum {
    MPU6050_GYRO_250DPS  = 0,
    MPU6050_GYRO_500DPS  = 1,
    MPU6050_GYRO_1000DPS = 2,
    MPU6050_GYRO_2000DPS = 3,
} mpu6050_gyro_range_t;

typedef struct {
    uint8_t               addr;
    mpu6050_accel_range_t accel_range;
    mpu6050_gyro_range_t  gyro_range;
} mpu6050_cfg_t;

typedef struct {
    float ax_g, ay_g, az_g; /* acceleration (G) */
} mpu6050_accel_t;

typedef struct {
    float gx_dps, gy_dps, gz_dps; /* angular rate (deg/s) */
} mpu6050_gyro_t;

typedef struct {
    const mpu6050_i2c_t  *i2c;
    mpu6050_cfg_t  cfg;
    float          accel_scale;
    float          gyro_scale;
} mpu6050_t;

#define MPU6050_ADDR_LOW   0x68
#define MPU6050_ADDR_HIGH  0x69
#define MPU6050_WHO_AM_I   0x68

/** @thread_safe: no @isr_safe: no */
mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           const mpu6050_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
mpu6050_err_t mpu6050_read_accel(const mpu6050_t *dev, mpu6050_accel_t *out);

/** @thread_safe: no @isr_safe: no */
mpu6050_err_t mpu6050_read_gyro(const mpu6050_t *dev, mpu6050_gyro_t *out);

/** Reset the chip, wait for DEVICE_RESET to clear, then wake it and restore the
 *  ranges init set, as a reset alone leaves it asleep. Calls delay_ms for 1 ms
 *  per poll, up to 1 s if the chip never finishes; with delay_ms NULL it polls
 *  with no pause. Returns MPU6050_ERR_TIMEOUT if the bit never clears, with the
 *  chip left unconfigured. Allow the same wait before the first read as after
 *  mpu6050_init.
 *  @thread_safe: no @isr_safe: no */
mpu6050_err_t mpu6050_reset(mpu6050_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
