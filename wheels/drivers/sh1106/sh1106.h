// SPDX-License-Identifier: MIT
/*
 * sh1106 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SH1106_OK        =  0,
    SH1106_ERR_I2C   = -1,
    SH1106_ERR_ID    = -2,
    SH1106_ERR_PARAM = -3,
} sh1106_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} sh1106_i2c_t;

/* ---------- constants ---------- */

#define SH1106_DEV_ADDR  0x3C

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} sh1106_data_t;

/* ---------- device context ---------- */

typedef struct {
    const sh1106_i2c_t *i2c;
} sh1106_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
sh1106_err_t sh1106_init(sh1106_t *dev,
                               const sh1106_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
sh1106_err_t sh1106_read(sh1106_t *dev,
                               sh1106_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SH1106_H */
