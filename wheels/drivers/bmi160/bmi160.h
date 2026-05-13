// SPDX-License-Identifier: MIT
/*
 * bmi160 — Bosch BMI160 6-axis inertial measurement unit
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads 3-axis accelerometer (mg)
 * and 3-axis gyroscope (dps) with configurable full-scale ranges.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMI160_H
#define BMI160_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BMI160_OK        =  0,
    BMI160_ERR_I2C   = -1,
    BMI160_ERR_ID    = -2,
    BMI160_ERR_PARAM = -3,
} bmi160_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bmi160_i2c_t;

/* ---------- constants ---------- */

#define BMI160_ADDR_LOW    0x68
#define BMI160_ADDR_HIGH   0x69
#define BMI160_CHIP_ID_VAL 0xD1

/* ---------- configuration enums ---------- */

typedef enum {
    BMI160_ACCEL_2G  = 0x03,
    BMI160_ACCEL_4G  = 0x05,
    BMI160_ACCEL_8G  = 0x08,
    BMI160_ACCEL_16G = 0x0C,
} bmi160_accel_range_t;

typedef enum {
    BMI160_GYRO_125DPS  = 0x04,
    BMI160_GYRO_250DPS  = 0x03,
    BMI160_GYRO_500DPS  = 0x02,
    BMI160_GYRO_1000DPS = 0x01,
    BMI160_GYRO_2000DPS = 0x00,
} bmi160_gyro_range_t;

typedef struct {
    uint8_t              addr;
    bmi160_accel_range_t accel_range;
    bmi160_gyro_range_t  gyro_range;
} bmi160_cfg_t;

/* ---------- sensor data ---------- */

typedef struct {
    float accel_mg[3];   /* acceleration in milli-g */
    float gyro_dps[3];   /* angular rate in degrees per second */
} bmi160_data_t;

/* ---------- device context ---------- */

typedef struct {
    const bmi160_i2c_t *i2c;
    bmi160_cfg_t        cfg;
    float               accel_sensitivity;  /* mg per LSB */
    float               gyro_sensitivity;   /* dps per LSB */
} bmi160_t;

/* ---------- API ---------- */

/** Initialize: verify chip ID, set accel+gyro to normal mode.
 *  @thread_safe: no @isr_safe: no */
bmi160_err_t bmi160_init(bmi160_t *dev, const bmi160_i2c_t *i2c,
                         const bmi160_cfg_t *cfg);

/** Read accel (mg) + gyro (dps).
 *  @thread_safe: caller_must_lock @isr_safe: no */
bmi160_err_t bmi160_read(const bmi160_t *dev, bmi160_data_t *out);

/** Soft reset (CMD = 0xB6).
 *  @thread_safe: no @isr_safe: no */
bmi160_err_t bmi160_reset(bmi160_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMI160_H */
