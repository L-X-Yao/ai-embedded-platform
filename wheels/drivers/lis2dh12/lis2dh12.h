// SPDX-License-Identifier: MIT
/*
 * lis2dh12 — ST LIS2DH12 3-axis accelerometer
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Ultra-low-power 10-bit normal mode. WHO_AM_I = 0x33.
 * Output in milligravity (mg). Pure C99, no heap.
 */

#ifndef LIS2DH12_H
#define LIS2DH12_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LIS2DH12_OK        =  0,
    LIS2DH12_ERR_I2C   = -1,
    LIS2DH12_ERR_ID    = -2,
    LIS2DH12_ERR_PARAM = -3,
} lis2dh12_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} lis2dh12_i2c_t;

typedef enum {
    LIS2DH12_RANGE_2G  = 0,
    LIS2DH12_RANGE_4G  = 1,
    LIS2DH12_RANGE_8G  = 2,
    LIS2DH12_RANGE_16G = 3,
} lis2dh12_range_t;

typedef enum {
    LIS2DH12_ODR_POWERDOWN = 0,
    LIS2DH12_ODR_1HZ       = 1,
    LIS2DH12_ODR_10HZ      = 2,
    LIS2DH12_ODR_25HZ      = 3,
    LIS2DH12_ODR_50HZ      = 4,
    LIS2DH12_ODR_100HZ     = 5,
    LIS2DH12_ODR_200HZ     = 6,
    LIS2DH12_ODR_400HZ     = 7,
} lis2dh12_odr_t;

typedef struct {
    uint8_t          addr;
    lis2dh12_range_t range;
    lis2dh12_odr_t   odr;
} lis2dh12_cfg_t;

typedef struct {
    const lis2dh12_i2c_t *i2c;
    lis2dh12_cfg_t cfg;
    float          sensitivity;  /* internal: mg per LSB */
} lis2dh12_t;

typedef struct {
    float x_mg;
    float y_mg;
    float z_mg;
} lis2dh12_data_t;

#define LIS2DH12_ADDR_LOW   0x18
#define LIS2DH12_ADDR_HIGH  0x19
#define LIS2DH12_WHO_AM_I   0x33

/** @thread_safe: no @isr_safe: no */
lis2dh12_err_t lis2dh12_init(lis2dh12_t *dev, const lis2dh12_i2c_t *i2c,
                              const lis2dh12_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
lis2dh12_err_t lis2dh12_read(lis2dh12_t *dev, lis2dh12_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* LIS2DH12_H */
