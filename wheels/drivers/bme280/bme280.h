// SPDX-License-Identifier: MIT
/*
 * bme280 — Bosch BME280 temperature, humidity, and pressure sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * I2C interface injected via callbacks. Implements the Bosch compensation
 * algorithm from the BME280 datasheet (BST-BME280-DS002-15).
 * Extends BMP280 with humidity measurement.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BME280_OK           =  0,
    BME280_ERR_I2C      = -1,
    BME280_ERR_ID       = -2,
    BME280_ERR_PARAM    = -3,
    BME280_ERR_TIMEOUT  = -4,
} bme280_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} bme280_i2c_t;

typedef enum {
    BME280_OVERSAMPLING_SKIP = 0,
    BME280_OVERSAMPLING_1    = 1,
    BME280_OVERSAMPLING_2    = 2,
    BME280_OVERSAMPLING_4    = 3,
    BME280_OVERSAMPLING_8    = 4,
    BME280_OVERSAMPLING_16   = 5,
} bme280_oversampling_t;

typedef struct {
    uint8_t              addr;
    bme280_oversampling_t osrs_t;
    bme280_oversampling_t osrs_p;
    bme280_oversampling_t osrs_h;
} bme280_cfg_t;

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
    uint8_t  dig_h1;
    int16_t  dig_h2;
    uint8_t  dig_h3;
    int16_t  dig_h4;
    int16_t  dig_h5;
    int8_t   dig_h6;
} bme280_calib_t;

typedef struct {
    bme280_i2c_t   i2c;
    bme280_cfg_t   cfg;
    bme280_calib_t calib;
    int32_t        t_fine;
} bme280_t;

typedef struct {
    float temperature_c;
    float pressure_pa;
    float humidity_pct;
} bme280_data_t;

#define BME280_ADDR_LOW   0x76
#define BME280_ADDR_HIGH  0x77
#define BME280_CHIP_ID    0x60

/** @thread_safe: no */
bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg);

/** @thread_safe: no */
bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out);

/** @thread_safe: no */
bme280_err_t bme280_set_oversampling(bme280_t *dev,
                                     bme280_oversampling_t osrs_t,
                                     bme280_oversampling_t osrs_p,
                                     bme280_oversampling_t osrs_h);

/** @thread_safe: no */
bme280_err_t bme280_reset(bme280_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BME280_H */
