// SPDX-License-Identifier: MIT
/*
 * bmp280 — Bosch BMP280 temperature and pressure sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Implements the Bosch compensation
 * algorithm from the BMP280 datasheet (BST-BMP280-DS001-26).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BMP280_OK           =  0,
    BMP280_ERR_I2C      = -1,
    BMP280_ERR_ID       = -2,
    BMP280_ERR_PARAM    = -3,
    BMP280_ERR_TIMEOUT  = -4,
} bmp280_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bmp280_i2c_t;

typedef enum {
    BMP280_OVERSAMPLING_SKIP = 0,  /* no measurement: see bmp280_read */
    BMP280_OVERSAMPLING_1    = 1,
    BMP280_OVERSAMPLING_2    = 2,
    BMP280_OVERSAMPLING_4    = 3,
    BMP280_OVERSAMPLING_8    = 4,
    BMP280_OVERSAMPLING_16   = 5,
} bmp280_oversampling_t;

typedef struct {
    uint8_t              addr;
    bmp280_oversampling_t osrs_t;
    bmp280_oversampling_t osrs_p;
} bmp280_cfg_t;

typedef struct {
    uint16_t dig_t1;
    int16_t  dig_t2;
    int16_t  dig_t3;
    uint16_t dig_p1;
    int16_t  dig_p2;
    int16_t  dig_p3;
    int16_t  dig_p4;
    int16_t  dig_p5;
    int16_t  dig_p6;
    int16_t  dig_p7;
    int16_t  dig_p8;
    int16_t  dig_p9;
} bmp280_calib_t;

typedef struct {
    const bmp280_i2c_t   *i2c;
    bmp280_cfg_t   cfg;
    bmp280_calib_t calib;
    int32_t        t_fine;
} bmp280_t;

typedef struct {
    float temperature_c;
    float pressure_pa;
} bmp280_data_t;

#define BMP280_ADDR_LOW   0x76
#define BMP280_ADDR_HIGH  0x77
#define BMP280_CHIP_ID    0x58

/** Check the chip ID, read the calibration and set the oversampling.
 *  0 in osrs_t or osrs_p is SKIP, so set both. Returns BMP280_ERR_PARAM if both
 *  are SKIP, because nothing would be left to measure. With osrs_t SKIP this
 *  takes one temperature measurement at x1, because pressure compensation needs
 *  a temperature, and can then also return BMP280_ERR_TIMEOUT.
 *  @thread_safe: no @isr_safe: no */
bmp280_err_t bmp280_init(bmp280_t *dev, const bmp280_i2c_t *i2c,
                         const bmp280_cfg_t *cfg);

/** One forced measurement of what the oversampling enables. A skipped pressure
 *  reads 0. A skipped temperature reads the one init measured, and pressure is
 *  compensated with it.
 *  @thread_safe: no @isr_safe: no */
bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out);

/** @thread_safe: no @isr_safe: no */
bmp280_err_t bmp280_reset(bmp280_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMP280_H */
