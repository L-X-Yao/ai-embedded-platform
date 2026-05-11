// SPDX-License-Identifier: MIT
/*
 * adxl345 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ADXL345_OK        =  0,
    ADXL345_ERR_I2C   = -1,
    ADXL345_ERR_ID    = -2,
    ADXL345_ERR_PARAM = -3,
} adxl345_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} adxl345_i2c_t;

/* ---------- constants ---------- */

#define ADXL345_DEV_ADDR  0x53

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} adxl345_data_t;

/* ---------- device context ---------- */

typedef struct {
    const adxl345_i2c_t *i2c;
} adxl345_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
adxl345_err_t adxl345_init(adxl345_t *dev,
                               const adxl345_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
adxl345_err_t adxl345_read(adxl345_t *dev,
                               adxl345_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ADXL345_H */
