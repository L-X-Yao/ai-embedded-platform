// SPDX-License-Identifier: MIT
/*
 * bme280 — Bosch BME280 temperature, humidity, and pressure sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
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
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bme280_i2c_t;

typedef enum {
    BME280_OVERSAMPLING_SKIP = 0,  /* no measurement: see bme280_read */
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
    const bme280_i2c_t   *i2c;
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

/** Check the chip ID, read the calibration and set the oversampling.
 *  0 in osrs_t, osrs_p or osrs_h is SKIP, so set all three. Returns
 *  BME280_ERR_PARAM if all are SKIP, because nothing would be left to measure.
 *  With osrs_t SKIP this takes one temperature measurement at x1, because
 *  pressure and humidity compensation need a temperature, and can then also
 *  return BME280_ERR_TIMEOUT.
 *  @thread_safe: no @isr_safe: no */
bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg);

/** One forced measurement of what the oversampling enables. A skipped pressure
 *  or humidity reads 0. A skipped temperature reads the last one measured, and
 *  pressure and humidity are compensated with it.
 *  @thread_safe: no @isr_safe: no */
bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out);

/** Takes effect at the next bme280_read. Returns BME280_ERR_PARAM if all three
 *  are SKIP. With osrs_t SKIP and no temperature measured yet, the next read
 *  measures one first.
 *  @thread_safe: no @isr_safe: yes */
bme280_err_t bme280_set_oversampling(bme280_t *dev,
                                     bme280_oversampling_t osrs_t,
                                     bme280_oversampling_t osrs_p,
                                     bme280_oversampling_t osrs_h);

/** @thread_safe: no @isr_safe: no */
bme280_err_t bme280_reset(bme280_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BME280_H */
