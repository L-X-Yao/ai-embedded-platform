// SPDX-License-Identifier: MIT
/*
 * ms5611 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
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
    MS5611_ERR_ID    = -2,
    MS5611_ERR_PARAM = -3,
} ms5611_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ms5611_i2c_t;

/* ---------- constants ---------- */

#define MS5611_DEV_ADDR  0x77

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} ms5611_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ms5611_i2c_t *i2c;
} ms5611_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
ms5611_err_t ms5611_init(ms5611_t *dev,
                               const ms5611_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
ms5611_err_t ms5611_read(ms5611_t *dev,
                               ms5611_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MS5611_H */
