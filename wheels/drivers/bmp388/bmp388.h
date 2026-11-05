// SPDX-License-Identifier: MIT
/*
 * bmp388 — Bosch BMP388 barometric pressure and temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Implements the Bosch compensation
 * algorithm from the BMP388 datasheet (BST-BMP388-DS001).
 * Uses float compensation (datasheet Sections 9.2 and 9.3).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMP388_H
#define BMP388_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BMP388_OK           =  0,
    BMP388_ERR_I2C      = -1,
    BMP388_ERR_ID       = -2,
    BMP388_ERR_PARAM    = -3,
    BMP388_ERR_TIMEOUT  = -4,
} bmp388_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bmp388_i2c_t;

typedef enum {
    BMP388_OVERSAMPLING_SKIP = 0,  /* does not skip: measures at x1 */
    BMP388_OVERSAMPLING_1    = 1,
    BMP388_OVERSAMPLING_2    = 2,
    BMP388_OVERSAMPLING_4    = 3,
    BMP388_OVERSAMPLING_8    = 4,
    BMP388_OVERSAMPLING_16   = 5,
    BMP388_OVERSAMPLING_32   = 6,
} bmp388_oversampling_t;

typedef struct {
    uint8_t              addr;
    bmp388_oversampling_t osrs_t;
    bmp388_oversampling_t osrs_p;
} bmp388_cfg_t;

/*
 * BMP388 calibration data from NVM registers 0x31-0x45.
 * Stored as float after parsing (datasheet Section 9.1).
 */
typedef struct {
    float par_t1;
    float par_t2;
    float par_t3;
    float par_p1;
    float par_p2;
    float par_p3;
    float par_p4;
    float par_p5;
    float par_p6;
    float par_p7;
    float par_p8;
    float par_p9;
    float par_p10;
    float par_p11;
} bmp388_calib_t;

typedef struct {
    const bmp388_i2c_t *i2c;
    bmp388_cfg_t   cfg;
    bmp388_calib_t calib;
    float          t_lin;   /* linearized temperature for pressure comp */
} bmp388_t;

typedef struct {
    float temperature_c;
    float pressure_pa;
} bmp388_data_t;

#define BMP388_ADDR_LOW   0x76
#define BMP388_ADDR_HIGH  0x77
#define BMP388_CHIP_ID    0x50

/** @thread_safe: no @isr_safe: no */
bmp388_err_t bmp388_init(bmp388_t *dev, const bmp388_i2c_t *i2c,
                         const bmp388_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
bmp388_err_t bmp388_read(bmp388_t *dev, bmp388_data_t *out);

/** @thread_safe: no @isr_safe: no */
bmp388_err_t bmp388_reset(bmp388_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMP388_H */
