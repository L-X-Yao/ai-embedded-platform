// SPDX-License-Identifier: MIT
/*
 * bno055 — Bosch BNO055 9-axis IMU with sensor fusion driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads Euler angles from on-chip
 * sensor fusion (heading, roll, pitch). Supports CONFIG, IMU, and NDOF
 * operation modes. Calibration status readback.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BNO055_H
#define BNO055_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BNO055_OK           =  0,
    BNO055_ERR_I2C      = -1,
    BNO055_ERR_ID       = -2,
    BNO055_ERR_PARAM    = -3,
} bno055_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} bno055_i2c_t;

typedef enum {
    BNO055_MODE_CONFIG  = 0x00,
    BNO055_MODE_ACCONLY = 0x01,
    BNO055_MODE_MAGONLY = 0x02,
    BNO055_MODE_GYRO    = 0x03,
    BNO055_MODE_ACCMAG  = 0x04,
    BNO055_MODE_ACCGYRO = 0x05,
    BNO055_MODE_MAGGYRO = 0x06,
    BNO055_MODE_AMG     = 0x07,
    BNO055_MODE_IMU     = 0x08,
    BNO055_MODE_COMPASS = 0x09,
    BNO055_MODE_M4G     = 0x0A,
    BNO055_MODE_NDOF_FMC_OFF = 0x0B,
    BNO055_MODE_NDOF    = 0x0C,
} bno055_opr_mode_t;

typedef struct {
    uint8_t          addr;
    bno055_opr_mode_t mode;
} bno055_cfg_t;

typedef struct {
    float heading_deg;
    float roll_deg;
    float pitch_deg;
} bno055_euler_t;

typedef struct {
    uint8_t sys;    /* 0-3 */
    uint8_t gyro;   /* 0-3 */
    uint8_t accel;  /* 0-3 */
    uint8_t mag;    /* 0-3 */
} bno055_calib_status_t;

typedef struct {
    const bno055_i2c_t *i2c;
    bno055_cfg_t cfg;
} bno055_t;

#define BNO055_ADDR_PRIMARY  0x28
#define BNO055_ADDR_ALT      0x29
#define BNO055_CHIP_ID       0xA0

/** @thread_safe: no @isr_safe: no */
bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
bno055_err_t bno055_read_euler(const bno055_t *dev, bno055_euler_t *out);

/** @thread_safe: no @isr_safe: no */
bno055_err_t bno055_get_calib_status(const bno055_t *dev,
                                     bno055_calib_status_t *out);

/** @thread_safe: no @isr_safe: no */
bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opr_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* BNO055_H */
