// SPDX-License-Identifier: MIT
/*
 * tmp102 — TI TMP102 I2C digital temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 12-bit resolution (0.0625 C/LSB), temp reg 0x00 (big-endian, bits[15:4]),
 * config reg 0x01, alert high/low regs 0x02-0x03.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef TMP102_H
#define TMP102_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TMP102_OK        =  0,
    TMP102_ERR_I2C   = -1,
    TMP102_ERR_PARAM = -3,
    TMP102_ERR_NOT_READY = -4,  /* the first read found no result, see tmp102_read_temp */
} tmp102_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} tmp102_i2c_t;

typedef struct {
    uint8_t addr;
} tmp102_cfg_t;

typedef struct {
    const tmp102_i2c_t *i2c;
    tmp102_cfg_t cfg;
    bool first_read;    /* internal: no read since init */
} tmp102_t;

#define TMP102_ADDR_GND  0x48u
#define TMP102_ADDR_VS   0x49u
#define TMP102_ADDR_SDA  0x4Au
#define TMP102_ADDR_SCL  0x4Bu

/** Initialise and configure for continuous conversion (12-bit).
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_init(tmp102_t *dev, const tmp102_i2c_t *i2c,
                         const tmp102_cfg_t *cfg);

/** Read temperature in degrees Celsius (0.0625 C resolution).
 *
 *  TMP102_ERR_NOT_READY, with temp_c left alone, if the first read after init finds
 *  the register at 0: the chip reads 0 until its first conversion ends, up to 15 ms
 *  after power-up. Wait those 15 ms and read again. That read is believed, so a
 *  sensor really at 0 C costs one retry, and a retry with no wait can still return
 *  0 C with TMP102_OK. Later reads are never refused.
 *
 *  After tmp102_wake the register holds the last result until the next conversion
 *  ends, up to 15 ms later, and a read then returns that older result.
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_read_temp(tmp102_t *dev, float *temp_c);

/** Set high alert threshold in degrees Celsius. Values past the ends of the
 *  12-bit format saturate at 127.9375 C and -128 C; NaN returns
 *  TMP102_ERR_PARAM.
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_set_alert_high(tmp102_t *dev, float temp_c);

/** Set low alert threshold in degrees Celsius. Values past the ends of the
 *  12-bit format saturate at 127.9375 C and -128 C; NaN returns
 *  TMP102_ERR_PARAM.
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_set_alert_low(tmp102_t *dev, float temp_c);

/** Enter shutdown (low-power) mode.
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_shutdown(tmp102_t *dev);

/** Wake from shutdown to continuous conversion.
 *  @thread_safe: no @isr_safe: no */
tmp102_err_t tmp102_wake(tmp102_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* TMP102_H */
