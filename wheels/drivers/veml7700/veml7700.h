// SPDX-License-Identifier: MIT
/*
 * veml7700 — Vishay VEML7700 I2C ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 16-bit register-based I2C: write/read 2 bytes (little-endian).
 * Fixed address 0x10. ALS + White channel. Configurable gain and
 * integration time for 0.0036 to 1.8432 lux/bit resolution.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the veml7700_t struct.
 */

#ifndef VEML7700_H
#define VEML7700_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VEML7700_OK         =  0,
    VEML7700_ERR_I2C    = -1,
    VEML7700_ERR_PARAM = -2,
} veml7700_err_t;

typedef enum {
    VEML7700_GAIN_1   = 0,
    VEML7700_GAIN_2   = 1,
    VEML7700_GAIN_1_8 = 2,
    VEML7700_GAIN_1_4 = 3,
} veml7700_gain_t;

typedef enum {
    VEML7700_IT_25MS  = 12,
    VEML7700_IT_50MS  =  8,
    VEML7700_IT_100MS =  0,
    VEML7700_IT_200MS =  1,
    VEML7700_IT_400MS =  2,
    VEML7700_IT_800MS =  3,
} veml7700_it_t;

typedef struct {
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} veml7700_i2c_t;

#define VEML7700_DEV_ADDR  0x10u

typedef struct {
    const veml7700_i2c_t *i2c;
    veml7700_gain_t       gain;
    veml7700_it_t         it;
} veml7700_t;

/** Configure sensor with gain and integration time. @thread_safe: no @isr_safe: no */
veml7700_err_t veml7700_init(veml7700_t *dev, const veml7700_i2c_t *i2c,
                             veml7700_gain_t gain, veml7700_it_t it);

/** Read raw ALS channel value. @thread_safe: caller_must_lock @isr_safe: no */
veml7700_err_t veml7700_read_als(veml7700_t *dev, uint16_t *raw);

/** Read raw White channel value. @thread_safe: caller_must_lock @isr_safe: no */
veml7700_err_t veml7700_read_white(veml7700_t *dev, uint16_t *raw);

/** Read ambient light in lux (raw x resolution). @thread_safe: caller_must_lock @isr_safe: no */
veml7700_err_t veml7700_read_lux(veml7700_t *dev, float *lux);

/** Change gain setting. @thread_safe: caller_must_lock @isr_safe: yes */
veml7700_err_t veml7700_set_gain(veml7700_t *dev, veml7700_gain_t gain);

/** Change integration time setting. @thread_safe: caller_must_lock @isr_safe: yes */
veml7700_err_t veml7700_set_integration_time(veml7700_t *dev, veml7700_it_t it);

/** Power on sensor (clear SD bit). @thread_safe: caller_must_lock @isr_safe: yes */
veml7700_err_t veml7700_power_on(veml7700_t *dev);

/** Shut down sensor (set SD bit). @thread_safe: caller_must_lock @isr_safe: yes */
veml7700_err_t veml7700_shutdown(veml7700_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* VEML7700_H */
