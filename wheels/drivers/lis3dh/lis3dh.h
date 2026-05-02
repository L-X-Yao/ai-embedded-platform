// SPDX-License-Identifier: MIT
/*
 * lis3dh — ST LIS3DH I2C 3-axis accelerometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * I2C interface injected via callbacks. Reads 3-axis acceleration with
 * configurable full-scale range and output data rate. 12-bit resolution,
 * left-justified in 16-bit registers.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef LIS3DH_H
#define LIS3DH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LIS3DH_OK        =  0,
    LIS3DH_ERR_I2C   = -1,
    LIS3DH_ERR_ID    = -2,
    LIS3DH_ERR_PARAM = -3,
} lis3dh_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} lis3dh_i2c_t;

typedef enum {
    LIS3DH_RANGE_2G  = 0,
    LIS3DH_RANGE_4G  = 1,
    LIS3DH_RANGE_8G  = 2,
    LIS3DH_RANGE_16G = 3,
} lis3dh_range_t;

typedef enum {
    LIS3DH_ODR_POWER_DOWN = 0,
    LIS3DH_ODR_1HZ        = 1,
    LIS3DH_ODR_10HZ       = 2,
    LIS3DH_ODR_25HZ       = 3,
    LIS3DH_ODR_50HZ       = 4,
    LIS3DH_ODR_100HZ      = 5,
    LIS3DH_ODR_200HZ      = 6,
    LIS3DH_ODR_400HZ      = 7,
    LIS3DH_ODR_1620HZ_LP  = 8,
    LIS3DH_ODR_5376HZ_LP  = 9,
} lis3dh_odr_t;

typedef struct {
    uint8_t        addr;
    lis3dh_range_t range;
    lis3dh_odr_t   odr;
} lis3dh_cfg_t;

typedef struct {
    float x, y, z;  /* acceleration (G) */
} lis3dh_accel_t;

typedef struct {
    int16_t x, y, z;  /* raw 16-bit left-justified */
} lis3dh_raw_t;

typedef struct {
    lis3dh_i2c_t  i2c;
    lis3dh_cfg_t  cfg;
    float         scale;  /* internal: raw-to-g divisor */
} lis3dh_t;

#define LIS3DH_ADDR_LOW   0x18u  /* SA0 = 0 */
#define LIS3DH_ADDR_HIGH  0x19u  /* SA0 = 1 */
#define LIS3DH_WHO_AM_I   0x33u

/** Initialize and verify WHO_AM_I. @thread_safe: no */
lis3dh_err_t lis3dh_init(lis3dh_t *dev, const lis3dh_i2c_t *i2c,
                         const lis3dh_cfg_t *cfg);

/** Read acceleration in G. @thread_safe: no */
lis3dh_err_t lis3dh_read_accel(const lis3dh_t *dev, lis3dh_accel_t *out);

/** Read raw 16-bit left-justified values. @thread_safe: no */
lis3dh_err_t lis3dh_read_raw(const lis3dh_t *dev, lis3dh_raw_t *out);

/** Change full-scale range at runtime. @thread_safe: no */
lis3dh_err_t lis3dh_set_range(lis3dh_t *dev, lis3dh_range_t range);

/** Change output data rate at runtime. @thread_safe: no */
lis3dh_err_t lis3dh_set_odr(lis3dh_t *dev, lis3dh_odr_t odr);

#ifdef __cplusplus
}
#endif

#endif /* LIS3DH_H */
