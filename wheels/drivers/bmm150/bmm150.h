// SPDX-License-Identifier: MIT
/*
 * bmm150 — Bosch BMM150 3-axis geomagnetic sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Chip ID = 0x32. XY: 13-bit, Z: 15-bit. Output in microtesla (uT).
 * Uses linear scaling without trim compensation (adequate for heading).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BMM150_H
#define BMM150_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BMM150_OK        =  0,
    BMM150_ERR_I2C   = -1,
    BMM150_ERR_ID    = -2,
    BMM150_ERR_PARAM = -3,
    BMM150_ERR_OVERFLOW = -4,
} bmm150_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} bmm150_i2c_t;

typedef struct {
    uint8_t addr;
} bmm150_cfg_t;

typedef struct {
    const bmm150_i2c_t *i2c;
    bmm150_cfg_t cfg;
} bmm150_t;

typedef struct {
    float x_ut;
    float y_ut;
    float z_ut;
} bmm150_data_t;

#define BMM150_ADDR_DEFAULT 0x10
#define BMM150_CHIP_ID      0x32

/** @thread_safe: no @isr_safe: no */
bmm150_err_t bmm150_init(bmm150_t *dev, const bmm150_i2c_t *i2c,
                          const bmm150_cfg_t *cfg);

/** Read the field in uT. Returns BMM150_ERR_OVERFLOW if an axis saturated.
 *  Uses nominal scale factors, not the chip's trim data, so readings are not
 *  compensated for gain, offset or temperature.
 *  @thread_safe: no @isr_safe: no */
bmm150_err_t bmm150_read(bmm150_t *dev, bmm150_data_t *out);

/** @thread_safe: no @isr_safe: no */
bmm150_err_t bmm150_reset(bmm150_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMM150_H */
