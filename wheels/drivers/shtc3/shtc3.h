// SPDX-License-Identifier: MIT
/*
 * shtc3 — Sensirion SHTC3 low-power humidity and temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. SHTC3 uses 16-bit command words
 * (not register-based) with CRC-8 protected responses. Must wakeup before
 * measurement and can sleep after.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SHTC3_H
#define SHTC3_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHTC3_OK        =  0,
    SHTC3_ERR_I2C   = -1,
    SHTC3_ERR_ID    = -2,
    SHTC3_ERR_PARAM = -3,
    SHTC3_ERR_CRC   = -4,
} shtc3_err_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint16_t ms);   /* required: reads NACK while measuring */
    void *ctx;
} shtc3_i2c_t;

typedef struct {
    const shtc3_i2c_t *i2c;
} shtc3_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
} shtc3_data_t;

#define SHTC3_ADDR  0x70

/** @thread_safe: no @isr_safe: no */
shtc3_err_t shtc3_init(shtc3_t *dev, const shtc3_i2c_t *i2c);

/** @thread_safe: no @isr_safe: no */
shtc3_err_t shtc3_read(shtc3_t *dev, shtc3_data_t *out);

/** @thread_safe: no @isr_safe: no */
shtc3_err_t shtc3_sleep(shtc3_t *dev);

/** @thread_safe: no @isr_safe: no */
shtc3_err_t shtc3_wakeup(shtc3_t *dev);

/** @thread_safe: no @isr_safe: no */
shtc3_err_t shtc3_reset(shtc3_t *dev);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t shtc3_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SHTC3_H */
