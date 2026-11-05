// SPDX-License-Identifier: MIT
/*
 * bmi270 — Bosch BMI270 SPI 6-axis IMU driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI interface: first byte = 0x80|addr (read) or addr (write).
 * Read operations require a dummy byte after the address byte.
 * Chip ID register 0x00 = 0x24.
 * Requires config file load after reset.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMI270_H
#define BMI270_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BMI270_OK          =  0,
    BMI270_ERR_PARAM   = -1,
    BMI270_ERR_SPI     = -2,
    BMI270_ERR_ID      = -3,
    BMI270_ERR_TIMEOUT = -4,
    BMI270_ERR_CONFIG  = -5,   /* config file load failed */
} bmi270_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);   /* required for init */
    void    *ctx;
} bmi270_spi_t;

/* ---------- register addresses ---------- */

#define BMI270_REG_CHIP_ID          0x00u
#define BMI270_REG_ERR_REG          0x02u
#define BMI270_REG_STATUS           0x03u
#define BMI270_REG_ACC_DATA_X_LSB   0x0Cu
#define BMI270_REG_ACC_DATA_X_MSB   0x0Du
#define BMI270_REG_ACC_DATA_Y_LSB   0x0Eu
#define BMI270_REG_ACC_DATA_Y_MSB   0x0Fu
#define BMI270_REG_ACC_DATA_Z_LSB   0x10u
#define BMI270_REG_ACC_DATA_Z_MSB   0x11u
#define BMI270_REG_GYR_DATA_X_LSB   0x12u
#define BMI270_REG_GYR_DATA_X_MSB   0x13u
#define BMI270_REG_GYR_DATA_Y_LSB   0x14u
#define BMI270_REG_GYR_DATA_Y_MSB   0x15u
#define BMI270_REG_GYR_DATA_Z_LSB   0x16u
#define BMI270_REG_GYR_DATA_Z_MSB   0x17u
#define BMI270_REG_TEMPERATURE_LSB  0x22u
#define BMI270_REG_TEMPERATURE_MSB  0x23u
#define BMI270_REG_INTERNAL_STATUS  0x21u
#define BMI270_REG_ACC_CONF         0x40u
#define BMI270_REG_ACC_RANGE        0x41u
#define BMI270_REG_GYR_CONF         0x42u
#define BMI270_REG_GYR_RANGE        0x43u
#define BMI270_REG_INIT_CTRL        0x59u
#define BMI270_REG_INIT_DATA        0x5Eu
#define BMI270_REG_PWR_CONF         0x7Cu
#define BMI270_REG_PWR_CTRL         0x7Du
#define BMI270_REG_CMD              0x7Eu

/* ---------- constants ---------- */

#define BMI270_CHIP_ID_VAL          0x24u

/* INTERNAL_STATUS bits */
#define BMI270_INIT_OK              0x01u
#define BMI270_INTERNAL_MESSAGE_MASK 0x0Fu

/* CMD register values */
#define BMI270_CMD_SOFTRESET        0xB6u

/* ---------- configuration enums ---------- */

typedef enum {
    BMI270_ACC_RANGE_2G  = 0,
    BMI270_ACC_RANGE_4G  = 1,
    BMI270_ACC_RANGE_8G  = 2,
    BMI270_ACC_RANGE_16G = 3,
} bmi270_acc_range_t;

typedef enum {
    BMI270_GYRO_RANGE_2000DPS = 0,
    BMI270_GYRO_RANGE_1000DPS = 1,
    BMI270_GYRO_RANGE_500DPS  = 2,
    BMI270_GYRO_RANGE_250DPS  = 3,
    BMI270_GYRO_RANGE_125DPS  = 4,
} bmi270_gyro_range_t;

typedef enum {
    BMI270_ODR_25HZ   = 0x06,
    BMI270_ODR_50HZ   = 0x07,
    BMI270_ODR_100HZ  = 0x08,
    BMI270_ODR_200HZ  = 0x09,
    BMI270_ODR_400HZ  = 0x0A,
    BMI270_ODR_800HZ  = 0x0B,
    BMI270_ODR_1600HZ = 0x0C,
} bmi270_odr_t;

/* ---------- configuration ---------- */

typedef struct {
    bmi270_acc_range_t   acc_range;
    bmi270_gyro_range_t  gyro_range;
    bmi270_odr_t         acc_odr;
    bmi270_odr_t         gyro_odr;
} bmi270_cfg_t;

/* ---------- sensor data ---------- */

typedef struct {
    float ax_g, ay_g, az_g;         /* acceleration (G) */
    float gx_dps, gy_dps, gz_dps;   /* angular rate (deg/s) */
} bmi270_accel_gyro_t;

/* ---------- device context ---------- */

typedef struct {
    const bmi270_spi_t *spi;
    float acc_div;
    float gyro_div;
} bmi270_t;

/* ---------- API ---------- */

/** Return sensible default configuration.
 *  @thread_safe: yes (pure)  @isr_safe: yes */
bmi270_cfg_t bmi270_default_cfg(void);

/** Initialize, verify chip ID, load config file, configure ranges/ODR.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_init(bmi270_t *dev,
                             const bmi270_spi_t *spi,
                             const bmi270_cfg_t *cfg);

/** Read accel + gyro sample (6 axes).
 *  @thread_safe: caller_must_lock @isr_safe: no */
bmi270_err_t bmi270_read_accel_gyro(const bmi270_t *dev,
                                        bmi270_accel_gyro_t *out);

/** Read die temperature.
 *  @thread_safe: caller_must_lock @isr_safe: no */
bmi270_err_t bmi270_read_temp(const bmi270_t *dev, float *temp_c);

/** Set accelerometer output data rate.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_set_acc_odr(const bmi270_t *dev, bmi270_odr_t odr);

/** Set gyroscope output data rate.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_set_gyro_odr(const bmi270_t *dev, bmi270_odr_t odr);

/** Set accelerometer full-scale range. Updates internal divisor.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_set_acc_range(bmi270_t *dev, bmi270_acc_range_t range);

/** Set gyroscope full-scale range. Updates internal divisor.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_set_gyro_range(bmi270_t *dev, bmi270_gyro_range_t range);

/** Soft-reset the device. All settings return to their defaults, so call
 *  bmi270_init before using it again.
 *  @thread_safe: no @isr_safe: no */
bmi270_err_t bmi270_soft_reset(const bmi270_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMI270_H */
