// SPDX-License-Identifier: MIT
/*
 * bme688 — Bosch BME688 environmental sensor driver (temp/hum/pres/gas)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Implements the Bosch compensation
 * algorithm from the BME688 datasheet (BST-BME688-DS000-18).
 * Pure C99, no heap, fully reentrant per-context.
 *
 * The BME688 shares its chip ID (0x61) with the BME680, but not its whole
 * register map: run_gas and gas_r sit elsewhere, and the gas formula differs.
 */

#ifndef BME688_H
#define BME688_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BME688_OK          =  0,
    BME688_ERR_HAL     = -1,
    BME688_ERR_PARAM   = -2,
    BME688_ERR_ID      = -3,
    BME688_ERR_TIMEOUT = -4,
} bme688_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bme688_i2c_t;

/* ---------- constants ---------- */

#define BME688_ADDR_LOW   0x76u
#define BME688_ADDR_HIGH  0x77u
#define BME688_CHIP_ID    0x61u

/* ---------- data types ---------- */

typedef struct {
    float    temperature_c;
    float    pressure_pa;
    float    humidity_pct;
    uint32_t gas_resistance_ohm;
    bool     gas_valid;
} bme688_data_t;

typedef struct {
    uint8_t  osrs_t;             /* oversampling temp  (0=skip, 1..5 = 1x..16x) */
    uint8_t  osrs_p;             /* oversampling press (0=skip, 1..5 = 1x..16x) */
    uint8_t  osrs_h;             /* oversampling hum   (0=skip, 1..5 = 1x..16x) */
    uint8_t  filter;             /* IIR filter coeff   (0..7) */
    uint16_t gas_heater_temp_c;  /* target heater temp (200..400 C) */
    uint16_t gas_heater_dur_ms;  /* heater duration    (1..4032 ms) */
    uint8_t  addr;               /* I2C address (0x76 or 0x77) */
} bme688_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const bme688_i2c_t *i2c;
    bme688_cfg_t cfg;
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
} bme688_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bme688_err_t bme688_init(bme688_t *dev, const bme688_i2c_t *i2c,
                         const bme688_cfg_t *cfg);

/** Trigger forced-mode measurement and read all channels. Blocks for the
 *  conversions plus the heater duration, up to about 5 s when a sensor fails.
 *  @thread_safe: caller_must_lock @isr_safe: no */
bme688_err_t bme688_read(bme688_t *dev, bme688_data_t *out);

/** Set gas heater target temperature and duration.
 *  @thread_safe: no @isr_safe: no */
bme688_err_t bme688_set_gas_heater(bme688_t *dev, uint16_t temp_c,
                                   uint16_t dur_ms);

/** Soft reset. @thread_safe: no @isr_safe: no */
bme688_err_t bme688_reset(bme688_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BME688_H */
