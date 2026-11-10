// SPDX-License-Identifier: MIT
/*
 * lis3mdl — ST LIS3MDL 3-axis digital magnetometer driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C register-based interface. Address 0x1C or 0x1E (SA1 pin).
 * Outputs in microtesla (1 gauss = 100 uT).
 * Ranges: +/-4, +/-8, +/-12, +/-16 gauss.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the lis3mdl_t struct.
 */

#ifndef LIS3MDL_H
#define LIS3MDL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LIS3MDL_OK        =  0,
    LIS3MDL_ERR_I2C   = -1,
    LIS3MDL_ERR_ID    = -2,
    LIS3MDL_ERR_PARAM = -3,
    LIS3MDL_ERR_NOT_READY = -4,   /* no sample yet: the first comes 100 ms after init */
} lis3mdl_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} lis3mdl_i2c_t;

/* ---------- constants ---------- */

#define LIS3MDL_ADDR_LOW   0x1C   /* SA1 = GND */
#define LIS3MDL_ADDR_HIGH  0x1E   /* SA1 = VDD */
#define LIS3MDL_DEV_ADDR   LIS3MDL_ADDR_LOW  /* default */

/* ---------- configuration ---------- */

typedef enum {
    LIS3MDL_RANGE_4G  = 0,   /* +/-4 gauss  (6842 LSB/gauss) */
    LIS3MDL_RANGE_8G  = 1,   /* +/-8 gauss  (3421 LSB/gauss) */
    LIS3MDL_RANGE_12G = 2,   /* +/-12 gauss (2281 LSB/gauss) */
    LIS3MDL_RANGE_16G = 3,   /* +/-16 gauss (1711 LSB/gauss) */
} lis3mdl_range_t;

typedef struct {
    lis3mdl_range_t range;
    uint8_t         addr;    /* 7-bit I2C address (0x1C or 0x1E) */
} lis3mdl_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float x_ut, y_ut, z_ut;   /* magnetic field in microtesla */
} lis3mdl_data_t;

typedef struct {
    int16_t x, y, z;          /* raw signed 16-bit counts */
} lis3mdl_raw_t;

/* ---------- device context ---------- */

typedef struct {
    const lis3mdl_i2c_t *i2c;
    lis3mdl_cfg_t        cfg;
    float                sensitivity;  /* LSB/gauss for current range */
} lis3mdl_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
lis3mdl_err_t lis3mdl_init(lis3mdl_t *dev, const lis3mdl_i2c_t *i2c,
                           const lis3mdl_cfg_t *cfg);

/**
 * Read magnetometer in microtesla. @thread_safe: caller_must_lock @isr_safe: no
 *
 * LIS3MDL_ERR_NOT_READY before the first sample, and out is left alone. Later reads
 * between samples return the last sample with LIS3MDL_OK.
 */
lis3mdl_err_t lis3mdl_read(lis3mdl_t *dev, lis3mdl_data_t *out);

/** Read raw 16-bit magnetometer values, as lis3mdl_read. @thread_safe: caller_must_lock @isr_safe: no */
lis3mdl_err_t lis3mdl_read_raw(const lis3mdl_t *dev, lis3mdl_raw_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* LIS3MDL_H */
