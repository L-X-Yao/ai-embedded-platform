// SPDX-License-Identifier: MIT
/*
 * pcf8563 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef PCF8563_H
#define PCF8563_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    PCF8563_OK        =  0,
    PCF8563_ERR_I2C   = -1,
    PCF8563_ERR_ID    = -2,
    PCF8563_ERR_PARAM = -3,
} pcf8563_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} pcf8563_i2c_t;

/* ---------- constants ---------- */

#define PCF8563_DEV_ADDR  0x51

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} pcf8563_data_t;

/* ---------- device context ---------- */

typedef struct {
    const pcf8563_i2c_t *i2c;
} pcf8563_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
pcf8563_err_t pcf8563_init(pcf8563_t *dev,
                               const pcf8563_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
pcf8563_err_t pcf8563_read(pcf8563_t *dev,
                               pcf8563_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PCF8563_H */
