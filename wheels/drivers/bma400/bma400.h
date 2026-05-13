// SPDX-License-Identifier: MIT
/*
 * bma400 — Bosch BMA400 ultra-low-power 3-axis accelerometer driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads 12-bit signed acceleration
 * with configurable full-scale range and power mode.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMA400_H
#define BMA400_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BMA400_OK        =  0,
    BMA400_ERR_I2C   = -1,
    BMA400_ERR_ID    = -2,
    BMA400_ERR_PARAM = -3,
} bma400_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bma400_i2c_t;

/* ---------- constants ---------- */

#define BMA400_ADDR_LOW   0x14u  /* SDO = GND */
#define BMA400_ADDR_HIGH  0x15u  /* SDO = VDDIO */
#define BMA400_CHIP_ID    0x90u

/* ---------- enums ---------- */

typedef enum {
    BMA400_RANGE_2G  = 0,
    BMA400_RANGE_4G  = 1,
    BMA400_RANGE_8G  = 2,
    BMA400_RANGE_16G = 3,
} bma400_range_t;

typedef enum {
    BMA400_POWER_SLEEP     = 0,
    BMA400_POWER_LOW_POWER = 1,
    BMA400_POWER_NORMAL    = 2,
} bma400_power_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t        addr;
    bma400_range_t range;
    bma400_power_t power;
} bma400_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float x_mg, y_mg, z_mg;  /* acceleration in milli-g */
} bma400_data_t;

/* ---------- device context ---------- */

typedef struct {
    const bma400_i2c_t *i2c;
    bma400_cfg_t  cfg;
    float         sensitivity;  /* internal: LSB per g */
} bma400_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bma400_err_t bma400_init(bma400_t *dev, const bma400_i2c_t *i2c,
                         const bma400_cfg_t *cfg);

/** Read acceleration in milli-g. @thread_safe: caller_must_lock @isr_safe: no */
bma400_err_t bma400_read(bma400_t *dev, bma400_data_t *out);

/** Change power mode at runtime. @thread_safe: no @isr_safe: no */
bma400_err_t bma400_set_power(bma400_t *dev, bma400_power_t mode);

/** Change full-scale range at runtime. @thread_safe: no @isr_safe: no */
bma400_err_t bma400_set_range(bma400_t *dev, bma400_range_t range);

#ifdef __cplusplus
}
#endif

#endif /* BMA400_H */
