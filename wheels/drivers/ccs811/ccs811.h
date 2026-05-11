// SPDX-License-Identifier: MIT
/*
 * ccs811 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef CCS811_H
#define CCS811_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CCS811_OK        =  0,
    CCS811_ERR_I2C   = -1,
    CCS811_ERR_ID    = -2,
    CCS811_ERR_PARAM = -3,
} ccs811_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ccs811_i2c_t;

/* ---------- constants ---------- */

#define CCS811_DEV_ADDR  0x5A

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} ccs811_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ccs811_i2c_t *i2c;
} ccs811_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_init(ccs811_t *dev,
                               const ccs811_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
ccs811_err_t ccs811_read(ccs811_t *dev,
                               ccs811_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CCS811_H */
