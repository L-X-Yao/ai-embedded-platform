// SPDX-License-Identifier: MIT
/*
 * icm42688 — TDK ICM-42688-P 6-axis IMU driver (SPI)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI interface injected via callbacks. Reads 3-axis accelerometer and
 * 3-axis gyroscope with configurable full-scale ranges and ODR.
 * Temperature sensor supported. Power mode control for low-power use.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef ICM42688_H
#define ICM42688_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ICM42688_OK          =  0,
    ICM42688_ERR_PARAM   = -1,
    ICM42688_ERR_SPI     = -2,
    ICM42688_ERR_ID      = -3,
    ICM42688_ERR_TIMEOUT = -4,
} icm42688_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} icm42688_spi_t;

/* ---------- configuration enums ---------- */

typedef enum {
    ICM42688_ACC_RANGE_2G  = 3,
    ICM42688_ACC_RANGE_4G  = 2,
    ICM42688_ACC_RANGE_8G  = 1,
    ICM42688_ACC_RANGE_16G = 0,
} icm42688_acc_range_t;

typedef enum {
    ICM42688_GYRO_RANGE_250DPS  = 3,
    ICM42688_GYRO_RANGE_500DPS  = 2,
    ICM42688_GYRO_RANGE_1000DPS = 1,
    ICM42688_GYRO_RANGE_2000DPS = 0,
} icm42688_gyro_range_t;

typedef enum {
    ICM42688_ODR_32KHZ  = 1,
    ICM42688_ODR_16KHZ  = 2,
    ICM42688_ODR_8KHZ   = 3,
    ICM42688_ODR_4KHZ   = 4,
    ICM42688_ODR_2KHZ   = 5,
    ICM42688_ODR_1KHZ   = 6,
    ICM42688_ODR_200HZ  = 7,
    ICM42688_ODR_100HZ  = 8,
    ICM42688_ODR_50HZ   = 9,
    ICM42688_ODR_25HZ   = 10,
    ICM42688_ODR_12HZ   = 11,
    ICM42688_ODR_6HZ    = 12,       /* accel LP only: init refuses it */
    ICM42688_ODR_3HZ    = 13,       /* accel LP only: init refuses it */
    ICM42688_ODR_1HZ    = 14,       /* accel LP only: init refuses it */
    ICM42688_ODR_500HZ  = 15,
} icm42688_odr_t;

typedef enum {
    ICM42688_PM_OFF      = 0,
    ICM42688_PM_STANDBY  = 1,       /* gyro only: standby */
    ICM42688_PM_LP       = 2,       /* accel only: low-power */
    ICM42688_PM_LN       = 3,       /* low-noise (normal) */
} icm42688_power_mode_t;

typedef struct {
    icm42688_acc_range_t   acc_range;
    icm42688_gyro_range_t  gyro_range;
    icm42688_odr_t         acc_odr;
    icm42688_odr_t         gyro_odr;
} icm42688_cfg_t;

/* ---------- sensor data ---------- */

typedef struct {
    float ax_g, ay_g, az_g;         /* acceleration (G) */
    float gx_dps, gy_dps, gz_dps;   /* angular rate (deg/s) */
    float temperature_c;             /* die temperature (Celsius) */
} icm42688_data_t;

/* ---------- device instance ---------- */

typedef struct {
    const icm42688_spi_t *spi;
    float acc_div;
    float gyro_div;
} icm42688_t;

/* ---------- key registers ---------- */

#define ICM42688_REG_DEVICE_CONFIG   0x11
#define ICM42688_REG_DRIVE_CONFIG    0x13
#define ICM42688_REG_INT_CONFIG      0x14
#define ICM42688_REG_TEMP_DATA1      0x1D
#define ICM42688_REG_TEMP_DATA0      0x1E
#define ICM42688_REG_ACCEL_DATA_X1   0x1F
#define ICM42688_REG_ACCEL_DATA_X0   0x20
#define ICM42688_REG_GYRO_DATA_X1    0x25
#define ICM42688_REG_GYRO_DATA_X0    0x26
#define ICM42688_REG_GYRO_CONFIG0    0x4F
#define ICM42688_REG_ACCEL_CONFIG0   0x50
#define ICM42688_REG_PWR_MGMT0      0x4E
#define ICM42688_REG_WHO_AM_I       0x75
#define ICM42688_REG_BANK_SEL       0x76

#define ICM42688_WHO_AM_I_VAL       0x47

/* ---------- API ---------- */

/** Return sensible default configuration.
 *  @thread_safe: yes (pure)  @isr_safe: yes */
icm42688_cfg_t   icm42688_default_cfg(void);

/** Initialize and verify chip identity (WHO_AM_I == 0x47).
 *  Configures accel/gyro range and ODR per cfg and starts both sensors in
 *  low-noise mode, so ICM42688_ODR_6HZ, 3HZ and 1HZ return ICM42688_ERR_PARAM.
 *  @thread_safe: no  @isr_safe: no */
icm42688_err_t   icm42688_init(icm42688_t *dev,
                                const icm42688_spi_t *spi,
                                const icm42688_cfg_t *cfg);

/** Read accel + gyro + temperature sample.
 *  @thread_safe: caller_must_lock  @isr_safe: no */
icm42688_err_t   icm42688_read(const icm42688_t *dev,
                                icm42688_data_t *out);

/** Read die temperature only.
 *  @thread_safe: caller_must_lock  @isr_safe: no */
icm42688_err_t   icm42688_read_temperature(const icm42688_t *dev,
                                            float *temp_c);

/** Set accel and gyro power modes independently.
 *  STANDBY is gyro only and LP is accel only: the other way round returns
 *  ICM42688_ERR_PARAM and PWR_MGMT0 is not written.
 *  Accel LP also needs an ODR of 500 Hz or lower, so it returns
 *  ICM42688_ERR_PARAM at the 1 kHz of icm42688_default_cfg().
 *  @thread_safe: no  @isr_safe: no */
icm42688_err_t   icm42688_set_power_mode(const icm42688_t *dev,
                                          icm42688_power_mode_t accel_mode,
                                          icm42688_power_mode_t gyro_mode);

/** Soft-reset the device (write 0x01 to DEVICE_CONFIG).
 *  @thread_safe: no  @isr_safe: no */
icm42688_err_t   icm42688_soft_reset(const icm42688_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ICM42688_H */
