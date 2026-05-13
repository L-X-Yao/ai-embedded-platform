// SPDX-License-Identifier: MIT
/*
 * lsm6dso — ST LSM6DSO 6-axis accelerometer + gyroscope driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads 3-axis accelerometer and
 * 3-axis gyroscope with configurable full-scale ranges and ODR.
 * Temperature sensor supported. Software reset.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef LSM6DSO_H
#define LSM6DSO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LSM6DSO_OK        =  0,
    LSM6DSO_ERR_I2C   = -1,
    LSM6DSO_ERR_ID    = -2,
    LSM6DSO_ERR_PARAM = -3,
} lsm6dso_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} lsm6dso_i2c_t;

/* ---------- constants ---------- */

#define LSM6DSO_ADDR_LOW   0x6A   /* SDO/SA0 = 0 */
#define LSM6DSO_ADDR_HIGH  0x6B   /* SDO/SA0 = 1 */
#define LSM6DSO_WHO_AM_I   0x6C

/* ---------- configuration enums ---------- */

typedef enum {
    LSM6DSO_ACCEL_2G  = 0,
    LSM6DSO_ACCEL_4G  = 2,
    LSM6DSO_ACCEL_8G  = 3,
    LSM6DSO_ACCEL_16G = 1,
} lsm6dso_accel_range_t;

typedef enum {
    LSM6DSO_GYRO_125DPS  = 2,
    LSM6DSO_GYRO_250DPS  = 0,
    LSM6DSO_GYRO_500DPS  = 4,
    LSM6DSO_GYRO_1000DPS = 8,
    LSM6DSO_GYRO_2000DPS = 12,
} lsm6dso_gyro_range_t;

typedef enum {
    LSM6DSO_ODR_12_5HZ = 1,
    LSM6DSO_ODR_26HZ   = 2,
    LSM6DSO_ODR_52HZ   = 3,
    LSM6DSO_ODR_104HZ  = 4,
    LSM6DSO_ODR_208HZ  = 5,
    LSM6DSO_ODR_416HZ  = 6,
    LSM6DSO_ODR_833HZ  = 7,
    LSM6DSO_ODR_1660HZ = 8,
    LSM6DSO_ODR_3330HZ = 9,
    LSM6DSO_ODR_6660HZ = 10,
} lsm6dso_odr_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t              addr;
    lsm6dso_accel_range_t accel_range;
    lsm6dso_gyro_range_t  gyro_range;
    lsm6dso_odr_t         odr;
} lsm6dso_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float x_mg, y_mg, z_mg;   /* acceleration (millig) */
} lsm6dso_accel_data_t;

typedef struct {
    float x_dps, y_dps, z_dps;   /* angular rate (degrees per second) */
} lsm6dso_gyro_data_t;

/* ---------- device context ---------- */

typedef struct {
    const lsm6dso_i2c_t *i2c;
    lsm6dso_cfg_t         cfg;
    float                  accel_sensitivity;   /* mg/LSB */
    float                  gyro_sensitivity;    /* mdps/LSB */
} lsm6dso_t;

/* ---------- API ---------- */

/** Configure and verify chip ID (WHO_AM_I == 0x6C).
 *  Performs SW_RESET, configures accel/gyro range and ODR.
 *  @thread_safe: no @isr_safe: no */
lsm6dso_err_t lsm6dso_init(lsm6dso_t *dev, const lsm6dso_i2c_t *i2c,
                            const lsm6dso_cfg_t *cfg);

/** Read 3-axis accelerometer data in millig.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lsm6dso_err_t lsm6dso_read_accel(const lsm6dso_t *dev,
                                  lsm6dso_accel_data_t *out);

/** Read 3-axis gyroscope data in degrees per second.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lsm6dso_err_t lsm6dso_read_gyro(const lsm6dso_t *dev,
                                 lsm6dso_gyro_data_t *out);

/** Read die temperature in degrees Celsius.
 *  @thread_safe: caller_must_lock @isr_safe: no */
lsm6dso_err_t lsm6dso_read_temp(const lsm6dso_t *dev, float *temperature_c);

/** Software reset (set SW_RESET bit in CTRL3_C).
 *  @thread_safe: no @isr_safe: no */
lsm6dso_err_t lsm6dso_reset(lsm6dso_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSO_H */
