// SPDX-License-Identifier: MIT
/*
 * bmm150 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef BMM150_H
#define BMM150_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BMM150_OK        =  0,
    BMM150_ERR_I2C   = -1,
    BMM150_ERR_ID    = -2,
    BMM150_ERR_PARAM = -3,
} bmm150_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bmm150_i2c_t;

/* ---------- constants ---------- */

#define BMM150_DEV_ADDR  0x10

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} bmm150_data_t;

/* ---------- device context ---------- */

typedef struct {
    const bmm150_i2c_t *i2c;
} bmm150_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bmm150_err_t bmm150_init(bmm150_t *dev,
                               const bmm150_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
bmm150_err_t bmm150_read(bmm150_t *dev,
                               bmm150_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BMM150_H */
