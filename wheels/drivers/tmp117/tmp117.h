// SPDX-License-Identifier: MIT
/*
 * tmp117 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef TMP117_H
#define TMP117_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TMP117_OK        =  0,
    TMP117_ERR_I2C   = -1,
    TMP117_ERR_ID    = -2,
    TMP117_ERR_PARAM = -3,
} tmp117_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} tmp117_i2c_t;

/* ---------- constants ---------- */

#define TMP117_DEV_ADDR  0x48

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} tmp117_data_t;

/* ---------- device context ---------- */

typedef struct {
    const tmp117_i2c_t *i2c;
} tmp117_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
tmp117_err_t tmp117_init(tmp117_t *dev,
                               const tmp117_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
tmp117_err_t tmp117_read(tmp117_t *dev,
                               tmp117_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TMP117_H */
