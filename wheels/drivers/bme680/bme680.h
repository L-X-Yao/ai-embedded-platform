// SPDX-License-Identifier: MIT
/*
 * bme680 — Bosch BME680 environmental sensor driver (temp/hum/pres/gas)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Implements the Bosch compensation
 * algorithm from the BME680 datasheet (BST-BME680-DS001-00).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BME680_H
#define BME680_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BME680_OK          =  0,
    BME680_ERR_I2C     = -1,
    BME680_ERR_PARAM   = -2,
    BME680_ERR_ID      = -3,
    BME680_ERR_TIMEOUT = -4,
} bme680_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} bme680_i2c_t;

#define BME680_ADDR_LOW   0x76u
#define BME680_ADDR_HIGH  0x77u
#define BME680_CHIP_ID    0x61u

typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_pa;
    float gas_resistance_ohm;
} bme680_data_t;

typedef struct {
    const bme680_i2c_t *i2c;
    uint8_t addr;
    /* temperature calibration */
    uint16_t par_t1;
    int16_t  par_t2;
    int8_t   par_t3;
    /* pressure calibration */
    uint16_t par_p1;
    int16_t  par_p2;
    int8_t   par_p3;
    int16_t  par_p4;
    int16_t  par_p5;
    int8_t   par_p6;
    int8_t   par_p7;
    int16_t  par_p8;
    int16_t  par_p9;
    uint8_t  par_p10;
    /* humidity calibration */
    uint16_t par_h1;
    uint16_t par_h2;
    int8_t   par_h3;
    int8_t   par_h4;
    int8_t   par_h5;
    uint8_t  par_h6;
    int8_t   par_h7;
    /* gas calibration */
    int8_t   par_gh1;
    int16_t  par_gh2;
    int8_t   par_gh3;
    uint8_t  res_heat_range;
    int8_t   res_heat_val;
    int8_t   range_sw_err;
    /* internal */
    int32_t  t_fine;
} bme680_t;

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bme680_err_t bme680_init(bme680_t *dev, const bme680_i2c_t *i2c,
                         uint8_t addr);

/** Trigger forced-mode measurement and read all channels. @thread_safe: no @isr_safe: no */
bme680_err_t bme680_read(bme680_t *dev, bme680_data_t *data);

/** Set oversampling for temp, humidity, pressure (0=skip, 1..5 = 1x..16x). @thread_safe: no @isr_safe: no */
bme680_err_t bme680_set_oversampling(bme680_t *dev, uint8_t os_temp,
                                     uint8_t os_hum, uint8_t os_pres);

/** Configure gas heater target temperature (deg C) and duration (ms). @thread_safe: no @isr_safe: no */
bme680_err_t bme680_set_gas_heater(bme680_t *dev, uint16_t temp_c,
                                   uint16_t duration_ms);

/** Soft reset. @thread_safe: no @isr_safe: no */
bme680_err_t bme680_reset(bme680_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BME680_H */
