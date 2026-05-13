// SPDX-License-Identifier: MIT
/*
 * tmp117 — TI TMP117 high-accuracy digital temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * ±0.1°C accuracy (typical), 16-bit resolution (0.0078125°C/LSB).
 * All registers are 16-bit, MSB first.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef TMP117_H
#define TMP117_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TMP117_OK         =  0,
    TMP117_ERR_I2C    = -1,
    TMP117_ERR_ID     = -2,
    TMP117_ERR_PARAM  = -3,
} tmp117_err_t;

typedef struct {
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} tmp117_i2c_t;

typedef enum {
    TMP117_AVG_NONE = 0,
    TMP117_AVG_8    = 1,
    TMP117_AVG_32   = 2,
    TMP117_AVG_64   = 3,
} tmp117_avg_t;

typedef struct {
    uint8_t     addr;
    tmp117_avg_t avg;
} tmp117_cfg_t;

typedef struct {
    const tmp117_i2c_t *i2c;
    tmp117_cfg_t cfg;
} tmp117_t;

#define TMP117_ADDR_GND  0x48
#define TMP117_ADDR_VCC  0x49
#define TMP117_ADDR_SDA  0x4A
#define TMP117_ADDR_SCL  0x4B
#define TMP117_DEVICE_ID 0x0117u

/** Configure and verify device ID. @thread_safe: no @isr_safe: no */
tmp117_err_t tmp117_init(tmp117_t *dev, const tmp117_i2c_t *i2c,
                         const tmp117_cfg_t *cfg);

/** Read temperature in degrees Celsius. @thread_safe: no @isr_safe: no */
tmp117_err_t tmp117_read(tmp117_t *dev, float *temp_c);

/** Soft reset. @thread_safe: no @isr_safe: no */
tmp117_err_t tmp117_reset(tmp117_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* TMP117_H */
