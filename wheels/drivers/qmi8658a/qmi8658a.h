// SPDX-License-Identifier: MIT
/*
 * qmi8658a — QMI8658A 6-axis IMU driver (I2C)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef QMI8658A_H
#define QMI8658A_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    QMI8658A_OK          =  0,
    QMI8658A_ERR_I2C     = -1,
    QMI8658A_ERR_CHIP_ID = -2,
} qmi8658a_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} qmi8658a_i2c_t;

/* ---------- configuration ---------- */

typedef enum {
    QMI8658A_ACC_RANGE_2G  = 0,
    QMI8658A_ACC_RANGE_4G  = 1,
    QMI8658A_ACC_RANGE_8G  = 2,
    QMI8658A_ACC_RANGE_16G = 3,
} qmi8658a_acc_range_t;

typedef enum {
    QMI8658A_GYR_RANGE_16    = 0,
    QMI8658A_GYR_RANGE_32    = 1,
    QMI8658A_GYR_RANGE_64    = 2,
    QMI8658A_GYR_RANGE_128   = 3,
    QMI8658A_GYR_RANGE_256   = 4,
    QMI8658A_GYR_RANGE_512   = 5,
    QMI8658A_GYR_RANGE_1024  = 6,
    QMI8658A_GYR_RANGE_2048  = 7,
} qmi8658a_gyr_range_t;

typedef enum {
    QMI8658A_ODR_8000  = 0,
    QMI8658A_ODR_4000  = 1,
    QMI8658A_ODR_2000  = 2,
    QMI8658A_ODR_1000  = 3,
    QMI8658A_ODR_500   = 4,
    QMI8658A_ODR_250   = 5,
    QMI8658A_ODR_125   = 6,
    QMI8658A_ODR_62    = 7,
    QMI8658A_ODR_31    = 8,
} qmi8658a_odr_t;

typedef struct {
    qmi8658a_acc_range_t acc_range;
    qmi8658a_gyr_range_t gyr_range;
    qmi8658a_odr_t       acc_odr;
    qmi8658a_odr_t       gyr_odr;
} qmi8658a_cfg_t;

/* ---------- sensor data ---------- */

typedef struct {
    float ax, ay, az;   /* acceleration (G) */
    float gx, gy, gz;   /* angular rate  (deg/s) */
} qmi8658a_data_t;

/* ---------- device instance ---------- */

typedef struct {
    const qmi8658a_i2c_t *i2c;
    float acc_div;
    float gyr_div;
} qmi8658a_t;

/* ---------- API ---------- */

/** Return sensible default configuration. @thread_safe: yes (pure) */
qmi8658a_cfg_t  qmi8658a_default_cfg(void);

/** Configure and verify chip ID. @thread_safe: no */
qmi8658a_err_t  qmi8658a_init(qmi8658a_t *dev,
                               const qmi8658a_i2c_t *i2c,
                               const qmi8658a_cfg_t *cfg);

/** Read accel + gyro sample. @thread_safe: caller_must_lock */
qmi8658a_err_t  qmi8658a_read(const qmi8658a_t *dev,
                               qmi8658a_data_t *out);

/** Soft-reset the device. @thread_safe: no */
qmi8658a_err_t  qmi8658a_reset(const qmi8658a_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* QMI8658A_H */
