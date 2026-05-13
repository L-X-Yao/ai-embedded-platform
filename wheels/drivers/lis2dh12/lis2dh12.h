// SPDX-License-Identifier: MIT
/*
 * lis2dh12 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef LIS2DH12_H
#define LIS2DH12_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LIS2DH12_OK        =  0,
    LIS2DH12_ERR_I2C   = -1,
    LIS2DH12_ERR_ID    = -2,
    LIS2DH12_ERR_PARAM = -3,
} lis2dh12_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} lis2dh12_i2c_t;

/* ---------- constants ---------- */

#define LIS2DH12_DEV_ADDR  0x19

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} lis2dh12_data_t;

/* ---------- device context ---------- */

typedef struct {
    const lis2dh12_i2c_t *i2c;
} lis2dh12_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
lis2dh12_err_t lis2dh12_init(lis2dh12_t *dev,
                               const lis2dh12_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
lis2dh12_err_t lis2dh12_read(lis2dh12_t *dev,
                               lis2dh12_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* LIS2DH12_H */
