// SPDX-License-Identifier: MIT
/*
 * opt3001 — TI OPT3001 I2C digital ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * I2C register-based interface. 16-bit big-endian registers.
 * Supports single-shot and continuous measurement modes.
 * Result register encodes lux as 0.01 * 2^E * R (E=exponent, R=mantissa).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the opt3001_t struct.
 */

#ifndef OPT3001_H
#define OPT3001_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OPT3001_OK            =  0,
    OPT3001_ERR_I2C       = -1,
    OPT3001_ERR_PARAM    = -2,
    OPT3001_ERR_ID        = -3,
    OPT3001_ERR_NOT_READY = -4,
} opt3001_err_t;

typedef struct {
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} opt3001_i2c_t;

typedef struct {
    const opt3001_i2c_t *i2c;
    uint8_t addr;
} opt3001_t;

#define OPT3001_ADDR_GND_GND  0x44
#define OPT3001_ADDR_GND_VDD  0x45
#define OPT3001_ADDR_VDD_GND  0x46
#define OPT3001_ADDR_VDD_VDD  0x47

/** Configure sensor and verify manufacturer/device IDs. @thread_safe: no */
opt3001_err_t opt3001_init(opt3001_t *dev, const opt3001_i2c_t *i2c,
                           uint8_t addr);

/** Read lux value from result register. @thread_safe: caller_must_lock */
opt3001_err_t opt3001_read_lux(opt3001_t *dev, float *lux);

/** Trigger a single-shot measurement. @thread_safe: caller_must_lock */
opt3001_err_t opt3001_start_single(opt3001_t *dev);

/** Set continuous conversion mode. @thread_safe: caller_must_lock */
opt3001_err_t opt3001_set_continuous(opt3001_t *dev);

/** Check if a conversion result is ready (config register CRF bit). @thread_safe: caller_must_lock */
bool opt3001_data_ready(opt3001_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* OPT3001_H */
