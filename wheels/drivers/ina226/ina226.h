// SPDX-License-Identifier: MIT
/*
 * ina226 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef INA226_H
#define INA226_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    INA226_OK        =  0,
    INA226_ERR_I2C   = -1,
    INA226_ERR_ID    = -2,
    INA226_ERR_PARAM = -3,
} ina226_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ina226_i2c_t;

/* ---------- constants ---------- */

#define INA226_DEV_ADDR  0x40

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} ina226_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ina226_i2c_t *i2c;
} ina226_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
ina226_err_t ina226_init(ina226_t *dev,
                               const ina226_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
ina226_err_t ina226_read(ina226_t *dev,
                               ina226_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* INA226_H */
