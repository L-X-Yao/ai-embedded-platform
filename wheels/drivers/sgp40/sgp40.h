// SPDX-License-Identifier: MIT
/*
 * sgp40 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef SGP40_H
#define SGP40_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SGP40_OK        =  0,
    SGP40_ERR_I2C   = -1,
    SGP40_ERR_ID    = -2,
    SGP40_ERR_PARAM = -3,
} sgp40_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} sgp40_i2c_t;

/* ---------- constants ---------- */

#define SGP40_DEV_ADDR  0x59

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} sgp40_data_t;

/* ---------- device context ---------- */

typedef struct {
    const sgp40_i2c_t *i2c;
} sgp40_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
sgp40_err_t sgp40_init(sgp40_t *dev,
                               const sgp40_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
sgp40_err_t sgp40_read(sgp40_t *dev,
                               sgp40_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SGP40_H */
