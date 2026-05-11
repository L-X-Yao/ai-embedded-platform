// SPDX-License-Identifier: MIT
/*
 * bno055 — Bosch BNO055 9-axis IMU with on-chip sensor fusion
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Euler angle output from
 * on-chip NDOF fusion. Scale: 1/16 degree per LSB (datasheet BST-BNO055-DS000).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BNO055_H
#define BNO055_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BNO055_OK        =  0,
    BNO055_ERR_I2C   = -1,
    BNO055_ERR_ID    = -2,
    BNO055_ERR_PARAM = -3,
} bno055_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required for mode switch delay */
    void *ctx;
} bno055_i2c_t;

/* ---------- operation modes ---------- */

typedef enum {
    BNO055_MODE_CONFIG = 0x00,
    BNO055_MODE_IMU    = 0x08,
    BNO055_MODE_NDOF   = 0x0C,
} bno055_opmode_t;

/* ---------- constants ---------- */

#define BNO055_DEV_ADDR      0x28
#define BNO055_DEV_ADDR_ALT  0x29
#define BNO055_CHIP_ID       0xA0

/* ---------- configuration ---------- */

typedef struct {
    uint8_t          addr;    /* I2C address (0x28 or 0x29) */
    bno055_opmode_t  mode;    /* operation mode */
} bno055_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float heading_deg;   /* Euler heading in degrees (0..360) */
    float roll_deg;      /* Euler roll in degrees (-180..180) */
    float pitch_deg;     /* Euler pitch in degrees (-90..90) */
} bno055_euler_t;

typedef struct {
    uint8_t sys;   /* system calibration (0-3, 3=fully calibrated) */
    uint8_t gyro;
    uint8_t accel;
    uint8_t mag;
} bno055_calib_status_t;

/* ---------- device context ---------- */

typedef struct {
    const bno055_i2c_t *i2c;
    bno055_cfg_t        cfg;
} bno055_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg);

/** Read Euler angles from fusion output. @thread_safe: caller_must_lock @isr_safe: no */
bno055_err_t bno055_read_euler(bno055_t *dev, bno055_euler_t *out);

/** Read calibration status. @thread_safe: caller_must_lock @isr_safe: no */
bno055_err_t bno055_read_calib_status(bno055_t *dev, bno055_calib_status_t *out);

/** Switch operation mode. @thread_safe: no @isr_safe: no */
bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opmode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* BNO055_H */
