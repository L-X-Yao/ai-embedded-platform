// SPDX-License-Identifier: MIT
/*
 * dps310 — Infineon DPS310 barometric pressure and temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Implements the DPS310 compensation
 * algorithm from the datasheet. Uses float compensation.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef DPS310_H
#define DPS310_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DPS310_OK        =  0,
    DPS310_ERR_I2C   = -1,
    DPS310_ERR_ID    = -2,
    DPS310_ERR_PARAM = -3,
    DPS310_ERR_TIMEOUT = -4,
} dps310_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = no pause in polls */
    void *ctx;
} dps310_i2c_t;

/* ---------- oversampling ---------- */

typedef enum {
    DPS310_OVERSAMPLING_1  = 0,
    DPS310_OVERSAMPLING_2  = 1,
    DPS310_OVERSAMPLING_4  = 2,
    DPS310_OVERSAMPLING_8  = 3,
    DPS310_OVERSAMPLING_16 = 4,
    DPS310_OVERSAMPLING_32 = 5,
    DPS310_OVERSAMPLING_64 = 6,
} dps310_oversampling_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t              addr;
    dps310_oversampling_t osrs_t;
    dps310_oversampling_t osrs_p;
} dps310_cfg_t;

/* ---------- calibration data ---------- */

typedef struct {
    float c0;
    float c1;
    float c00;
    float c10;
    float c01;
    float c11;
    float c20;
    float c21;
    float c30;
} dps310_calib_t;

/* ---------- device context ---------- */

typedef struct {
    const dps310_i2c_t *i2c;
    dps310_cfg_t        cfg;
    dps310_calib_t      calib;
    float               kt;    /* temperature scale factor */
    float               kp;    /* pressure scale factor    */
} dps310_t;

/* ---------- data output ---------- */

typedef struct {
    float temperature_c;
    float pressure_pa;
} dps310_data_t;

/* ---------- constants ---------- */

#define DPS310_ADDR_HIGH  0x77u
#define DPS310_ADDR_LOW   0x76u
#define DPS310_PRODUCT_ID 0x10u

/* ---------- API ---------- */

/** Configure and verify product ID, wait for start-up to end, read calibration.
 *  Takes up to 40 ms after power-on or soft reset; DPS310_ERR_TIMEOUT if it
 *  never ends. @thread_safe: no @isr_safe: no */
dps310_err_t dps310_init(dps310_t *dev, const dps310_i2c_t *i2c,
                         const dps310_cfg_t *cfg);

/** Read temperature + pressure with compensation. @thread_safe: no @isr_safe: no */
dps310_err_t dps310_read(dps310_t *dev, dps310_data_t *out);

/** Soft reset (write 0x89 to RESET register). @thread_safe: no @isr_safe: no */
dps310_err_t dps310_reset(dps310_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DPS310_H */
