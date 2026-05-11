// SPDX-License-Identifier: MIT
/*
 * hd44780 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef HD44780_H
#define HD44780_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HD44780_OK        =  0,
    HD44780_ERR_I2C   = -1,
    HD44780_ERR_ID    = -2,
    HD44780_ERR_PARAM = -3,
} hd44780_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} hd44780_i2c_t;

/* ---------- constants ---------- */

#define HD44780_DEV_ADDR  0x27

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} hd44780_data_t;

/* ---------- device context ---------- */

typedef struct {
    const hd44780_i2c_t *i2c;
} hd44780_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
hd44780_err_t hd44780_init(hd44780_t *dev,
                               const hd44780_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
hd44780_err_t hd44780_read(hd44780_t *dev,
                               hd44780_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HD44780_H */
