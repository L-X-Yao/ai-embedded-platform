// SPDX-License-Identifier: MIT
/*
 * ms5611 — TE Connectivity MS5611 precision barometric pressure sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Command-based protocol (not register-
 * based). Implements 2nd-order temperature compensation from the MS5611-01BA03
 * datasheet (DA5611-01BA03_011).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MS5611_H
#define MS5611_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MS5611_OK        =  0,
    MS5611_ERR_I2C   = -1,
    MS5611_ERR_PROM  = -2,
    MS5611_ERR_PARAM = -3,
} ms5611_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required for conversion wait */
    void *ctx;
} ms5611_i2c_t;

/* ---------- oversampling ratio ---------- */

typedef enum {
    MS5611_OSR_256  = 0,
    MS5611_OSR_512  = 1,
    MS5611_OSR_1024 = 2,
    MS5611_OSR_2048 = 3,
    MS5611_OSR_4096 = 4,
} ms5611_osr_t;

/* ---------- constants ---------- */

#define MS5611_DEV_ADDR      0x77
#define MS5611_DEV_ADDR_ALT  0x76

/* ---------- configuration ---------- */

typedef struct {
    uint8_t     addr;    /* I2C address (0x77 or 0x76) */
    ms5611_osr_t osr;    /* oversampling ratio */
} ms5611_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    float temperature_c;   /* temperature in degrees Celsius */
    float pressure_pa;     /* pressure in Pascals */
} ms5611_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ms5611_i2c_t *i2c;
    ms5611_cfg_t  cfg;
    uint16_t      prom[8];   /* factory calibration data */
} ms5611_t;

/* ---------- API ---------- */

/** Reset chip and read PROM calibration. @thread_safe: no @isr_safe: no */
ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg);

/** Read temperature and pressure. @thread_safe: caller_must_lock @isr_safe: no */
ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MS5611_H */
