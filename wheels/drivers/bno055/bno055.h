// SPDX-License-Identifier: MIT
/*
 * bno055 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef BNO055_H
#define BNO055_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BNO055_OK        =  0,
    BNO055_ERR_I2C   = -1,
    BNO055_ERR_ID    = -2,
    BNO055_ERR_PARAM = -3,
} bno055_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} bno055_i2c_t;

/* ---------- constants ---------- */

#define BNO055_DEV_ADDR  0x28

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} bno055_data_t;

/* ---------- device context ---------- */

typedef struct {
    const bno055_i2c_t *i2c;
} bno055_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
bno055_err_t bno055_init(bno055_t *dev,
                               const bno055_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
bno055_err_t bno055_read(bno055_t *dev,
                               bno055_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BNO055_H */
